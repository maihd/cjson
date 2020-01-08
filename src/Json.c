#include "Json.h"

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

#define JSON_SUPEROF(ptr, T, member) (T*)((char*)ptr - offsetof(T, member))

#ifndef JSON_INLINE
#  if defined(_MSC_VER)
#     define JSON_INLINE __forceinline
#  elif defined(__GNUC__) || defined(__clang__)
#     define JSON_INLINE __inline__ __attribute__((always_inline))
#  elif defined(__cplusplus)
#     define JSON_INLINE inline
#  else
#     define JSON_INLINE 
#  endif
#endif

static const int JSON_REVERSED0 = 'JSON';
static const int JSON_REVERSED1 = 'json';

/*
Utility
*/

#define JSON_ALLOC(a, size) (a)->alloc((a)->data, size) 
#define JSON_FREE(a, ptr)   (a)->free((a)->data, ptr)

/* Next power of two */
static JSON_INLINE int Json_NextPOT(int x)
{
    x  = x - 1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

/* 
JsonArray: dynamic, scalable array
@note: internal only
*/
typedef struct JsonArray
{
    int  size;
    int  count;
    char buffer[];
} JsonArray;

#define JsonArray_GetRaw(a)             ((JsonArray*)(a) - 1)
#define JsonArray_Init()                0
#define JsonArray_Free(a, alloc)        ((a) ? (alloc)->free((alloc)->data, JsonArray_GetRaw(a)) : (void)0)
#define JsonArray_GetSize(a)            ((a) ? JsonArray_GetRaw(a)->size  : 0)
#define JsonArray_GetCount(a)           ((a) ? JsonArray_GetRaw(a)->count : 0)
#define JsonArray_GetUsageMemory(a)     (sizeof(JsonArray) + JsonArray_GetCount(a) * sizeof(*(a)))
#define JsonArray_Push(a, v, alloc)     (JsonArray_Ensure(a, JsonArray_GetCount(a) + 1, alloc) ? ((void)((a)[JsonArray_GetRaw(a)->count++] = v), 1) : 0)
#define JsonArray_Pop(a, v, alloc)      ((a)[--JsonArray_GetRaw(a)->count]);
#define JsonArray_Ensure(a, n, alloc)   ((!(a) || JsonArray_GetSize(a) < (n)) ? (*((void**)&(a))=JsonArray_Grow(a, Json_NextPOT(n), sizeof((a)[0]), alloc)) != NULL : 1)
#define JsonArray_Clear(a)              ((a) ? (void)(JsonArray_GetRaw(a)->count = 0) : (void)0)
#define JsonArray_Clone(a, alloc)       ((a) ? memcpy(JsonArray_Grow(0, JsonArray_GetCount(a), sizeof((a)[0]), alloc), JsonArray_GetRaw(a), JsonArray_GetUsageMemory(a)) : NULL)

static JSON_INLINE void* JsonArray_Grow(void* array, int reqsize, int elemsize, JsonAllocator* allocator)
{
    assert(elemsize > 0);
    assert(allocator != NULL);

    JsonArray*  raw   = array ? JsonArray_GetRaw(array) : NULL;
    int         size  = JsonArray_GetSize(array);
    int         count = JsonArray_GetCount(array);

    if (size >= reqsize)
    {
        return array;
    }
    else
    {
        size = reqsize;
    }

    JsonArray* newArray = (JsonArray*)JSON_ALLOC(allocator, sizeof(JsonArray) + size * elemsize);
    if (newArray)
    {
        newArray->size  = size;
        newArray->count = count;

        if (raw && count > 0)
        {
            memcpy(newArray->buffer, raw->buffer, count * elemsize);
            free(raw);
        }

        return newArray->buffer;
    }
    else
    {
        return NULL;
    }
}

/*
JsonTempArray: memory-wise array for containing parsing value
@note: internal only 
*/
#define JsonTempArray(T, CAPACITY)  struct {    \
    T*  array;                                  \
    int count;                                  \
    T   buffer[CAPACITY]; }

#define JsonTempArray_Init(a)             { a, 0 }
#define JsonTempArray_Free(a, alloc)      JsonArray_Free((a)->array, alloc)
#define JsonTempArray_Push(a, v, alloc)   ((a)->count >= sizeof((a)->buffer) / sizeof((a)->buffer[0]) ? JsonArray_Push((a)->array, v, alloc) : ((a)->buffer[(a)->count++] = v, 1))
#define JsonTempArray_GetCount(a)         ((a)->count + JsonArray_GetCount((a)->array))
#define JsonTempArray_ToBuffer(a, alloc)  JsonTempArray_ToBufferFunc((a)->buffer, (a)->count, (a)->array, (int)sizeof((a)->buffer[0]), alloc)

static JSON_INLINE void* JsonTempArray_ToBufferFunc(void* buffer, int count, void* dynamicBuffer, int itemSize, JsonAllocator* allocator)
{
    int total = count + JsonArray_GetCount(dynamicBuffer);
    if (total > 0)
    {
        void* array = (JsonArray*)JSON_ALLOC(allocator, total * itemSize);
        if (array)
        {
            memcpy(array, buffer, count * itemSize);
            if (total > count)
            {
                memcpy((char*)array + count * itemSize, dynamicBuffer, (total - count) * itemSize);
            }
        }
        return array;
    }
    return NULL;
}

typedef struct JsonState JsonState;
struct JsonState
{
    int reversed0;
    int reversed1;

    Json           root;
    JsonState*          next;

    int                 line;
    int                 column;
    int                 cursor;
    //JsonType            parsingType;
    
    int                 length;         /* Reference only */
    const char*         buffer;         /* Reference only */
    
    JsonError           errnum;
    char*               errmsg;
    jmp_buf             errjmp;

    JsonAllocator       allocator;      /* Runtime allocator */
};

/* @funcdef: Json_Alloc */
static void* Json_Alloc(void* data, int size)
{
    (void)data;
    return malloc(size);
}

/* @funcdef: Json_Free */
static void Json_Free(void* data, void* pointer)
{
    (void)data;
    free(pointer);
}

static void Json_SetErrorArgs(JsonState* state, JsonType type, JsonError code, const char* fmt, va_list valist)
{
    const int errmsg_size = 1024;

    const char* type_name;
    switch (type)
    {
    case JSON_NULL:
        type_name = "null";
        break;

    case JSON_BOOLEAN:
        type_name = "boolean";
        break;

    case JSON_NUMBER:
        type_name = "number";
        break;

    case JSON_ARRAY:
        type_name = "array";
        break;

    case JSON_STRING:
        type_name = "string";
        break;

    case JSON_OBJECT:
        type_name = "object";
        break;

    default:
        type_name = "unknown";
        break;
    }

    state->errnum = code;
    if (state->errmsg == NULL)
    {
        state->errmsg = (char*)state->allocator.alloc(state->allocator.data, errmsg_size);
    }

    char final_format[1024];
    char templ_format[1024] = "%s\n\tAt line %d, column %d. Parsing token: <%s>.";

#if defined(_MSC_VER) && _MSC_VER >= 1200
    sprintf_s(final_format, sizeof(final_format), templ_format, fmt, state->line, state->column, type_name);
    sprintf_s(state->errmsg, errmsg_size, final_format, valist);
#else
    sprintf(final_format, templ_format, fmt, state->line, state->column, type_name);
    sprintf(state->errmsg, final_format, valist);
#endif
}

/* @funcdef: Json_SetError */
static void Json_SetError(JsonState* state, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_SetErrorArgs(state, type, code, fmt, varg);
    va_end(varg);
}

/* funcdef: Json_Panic */
static void Json_Panic(JsonState* state, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_SetErrorArgs(state, type, code, fmt, varg);
    va_end(varg);

    longjmp(state->errjmp, code);
}

static void JsonValue_ReleaseMemory(Json* value, JsonAllocator* allocator)
{
    if (value)
    {
        int i, n;
        switch (value->type)
        {
        case JSON_ARRAY:
            for (i = 0, n = value->length; i < n; i++)
            {
                JsonValue_ReleaseMemory(&value->array[i], allocator);
            }
            JSON_FREE(allocator, value->array);
            break;

        case JSON_OBJECT:
            for (i = 0, n = value->length; i < n; i++)
            {
                JsonValue_ReleaseMemory(&value->object[i].value, allocator);
            }
            JSON_FREE(allocator, value->object);
            break;

        case JSON_STRING:
            JSON_FREE(allocator, (void*)value->string);
            break;

        default:
            break;
        }
    }
}

/* @funcdef: JsonState_Make */
static JsonState* JsonState_Make(const char* json, int jsonLength, JsonAllocator allocator)
{
    JsonState* state = (JsonState*)JSON_ALLOC(&allocator, sizeof(JsonState));
    if (state)
    {
        state->reversed0    = JSON_REVERSED0;
        state->reversed1    = JSON_REVERSED1;

		state->next         = NULL;

		state->line         = 1;
		state->column       = 1;
		state->cursor       = 0;
		state->buffer       = json;
		state->length       = jsonLength;

		state->errmsg       = NULL;
		state->errnum       = JSON_ERROR_NONE;

        state->allocator    = allocator;
    }
    return state;
}

/* @funcdef: JsonState_Free */
static void JsonState_Free(JsonState* state)
{
    if (state)
    {
        Json* root = &state->root;
        JsonValue_ReleaseMemory(root, &state->allocator);

		JsonState* next = state->next;

		JSON_FREE(&state->allocator, state->errmsg);
		JSON_FREE(&state->allocator, state);

		JsonState_Free(next);
    }
}

/* @funcdef: Json_IsEOF */
static int Json_IsEOF(JsonState* state)
{
    return state->cursor >= state->length || state->buffer[state->cursor] <= 0;
}

/* @funcdef: Json_PeekChar */
static int Json_PeekChar(JsonState* state)
{
    return state->buffer[state->cursor];
}

/* @funcdef: Json_NextChar */
static int Json_NextChar(JsonState* state)
{
    if (Json_IsEOF(state))
    {
		return -1;
    }
    else
    {
		int c = state->buffer[++state->cursor];

		if (c == '\n')
		{
			state->line++;
			state->column = 1;
		}
		else
		{
			state->column = state->column + 1;
		}
		
		return c;
    }
}

/* @funcdef: Json_SkipSpace */
static int Json_SkipSpace(JsonState* state)
{
    int c = Json_PeekChar(state);
    while (c > 0 && isspace(c))
    {
		c = Json_NextChar(state);
    }
    return c;
}

/* @funcdef: Json_MatchChar */
static int Json_MatchChar(JsonState* state, JsonType type, int c)
{
    if (Json_PeekChar(state) == c)
    {
		return Json_NextChar(state);
    }
    else
    {
        Json_Panic(state, type, JSON_ERROR_UNMATCH, "Expected '%c'", (char)c);
		return -1;
    }
}

/* All parse functions declaration */

static void Json_ParseArray(JsonState* state, Json* outValue);
static void Json_ParseSingle(JsonState* state, Json* outValue);
static void Json_ParseObject(JsonState* state, Json* outValue);
static void Json_ParseNumber(JsonState* state, Json* outValue);
static void Json_ParseString(JsonState* state, Json* outValue);

/* @funcdef: Json_ParseNumber */
static void Json_ParseNumber(JsonState* state, Json* outValue)
{
    if (Json_SkipSpace(state) > 0)
    {
		int c = Json_PeekChar(state);
		int sign = 1;
		
		if (c == '+')
		{
			c = Json_NextChar(state);
			Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "JSON does not support number start with '+'");
		}
		else if (c == '-')
		{
			sign = -1;
			c = Json_NextChar(state);
		}
		else if (c == '0')
		{
			c = Json_NextChar(state);
			if (!isspace(c) && !ispunct(c))
			{
				Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "JSON does not support number start with '0' (only standalone '0' is accepted)");
			}
		}
		else if (!isdigit(c))
		{
			Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Unexpected '%c'", c);
		}

		int    dot    = 0;
        int    exp    = 0;
        int    expsgn = 0;
        int    exppow = 0;
        int    expchk = 0;
		int    numpow = 1;
		double number = 0;

		while (c > 0)
		{
            if (c == 'e')
            {
                if (exp)
                {
                    Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many 'e' are presented in a <number>");
                }
                else if (dot && numpow == 1)
                {
                    Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED,
                                "'.' is presented in number token, but require a digit after '.' ('%c')", c);
                }
                else
                {
                    exp    = 1;
                    expchk = 0;
                }
            }
			else if (c == '.')
			{
                if (exp)
                {
                    Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Cannot has '.' after 'e' is presented in a <number>");
                }
				else if (dot)
				{
					Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many '.' are presented in a <number>");
				}
				else
				{
					dot = 1;
				}
			}
            else if (exp && (c == '-' || c == '+'))
            {
                if (expchk)
                {
                    Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'%c' is presented after digits are presented of exponent part", c);
                }
                else if (expsgn)
                {
                    Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many signed characters are presented after 'e'");
                }
                else
                {
                    expsgn = (c == '-' ? -1 : 1);
                }
            }
			else if (!isdigit(c))
			{
				break;
			}
			else
			{
                if (exp)
                {
                    expchk = 1;
                    exppow = exppow * 10 + (c - '0');
                }
                else
                {
                    if (dot)
                    {
                        numpow *= 10;
                        number += (c - '0') / (double)numpow;
                    }
                    else
                    {
                        number = number * 10 + (c - '0');
                    }
                }
			}

			c = Json_NextChar(state);
		}

        if (exp && !expchk)
        {
            Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'e' is presented in number token, but require a digit after 'e' ('%c')", (char)c);
        }
		if (dot && numpow == 1)
		{
			Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'.' is presented in number token, but require a digit after '.' ('%c')", (char)c);
		}
		else
		{
            Json value = { JSON_NUMBER };
            value.number = sign * number;

            if (exp)
            {
                int i;
                double tmp = 1;
                for (i = 0; i < exppow; i++)
                {
                    tmp *= 10;
                }
                
                if (expsgn < 0)
                {
                    value.number /= tmp;
                }
                else
                {
                    value.number *= tmp;
                }
            }

            *outValue = value;
		}
    }
}

/* @funcdef: Json_ParseArray */
static void Json_ParseArray(JsonState* state, Json* outValue)
{
    if (Json_SkipSpace(state) > 0)
    {
	    Json_MatchChar(state, JSON_ARRAY, '[');

        JsonTempArray(Json, 64) values = JsonTempArray_Init(NULL);
	    while (Json_SkipSpace(state) > 0 && Json_PeekChar(state) != ']')
	    {
	        if (values.count > 0)
	        {
                Json_MatchChar(state, JSON_ARRAY, ',');
	        }
	    
            Json value;
            Json_ParseSingle(state, &value);

            JsonTempArray_Push(&values, value, &state->allocator);
	    }

	    Json_SkipSpace(state);
	    Json_MatchChar(state, JSON_ARRAY, ']');

        outValue->type   = JSON_ARRAY;
        outValue->length = JsonTempArray_GetCount(&values);
        outValue->array  = (Json*)JsonTempArray_ToBuffer(&values, &state->allocator);

        JsonTempArray_Free(&values, &state->allocator);
    }
}

/* Json_ParseSingle */
static void Json_ParseSingle(JsonState* state, Json* outValue)
{
    if (Json_SkipSpace(state) > 0)
    {
	    int c = Json_PeekChar(state);
	
	    switch (c)
	    {
	    case '[':
	        Json_ParseArray(state, outValue);
            break;
	    
	    case '{':
	        Json_ParseObject(state, outValue);
            break;
	    
	    case '"':
	        Json_ParseString(state, outValue);
            break;

	    case '+': case '-': case '0': 
        case '1': case '2': case '3': 
        case '4': case '5': case '6': 
        case '7': case '8': case '9':
	        Json_ParseNumber(state, outValue);
            break;
	    
        default:
	    {
	        int length = 0;
	        while (c > 0 && isalpha(c))
	        {
                length++;
                c = Json_NextChar(state);
	        }

	        const char* token = state->buffer + state->cursor - length;
	        if (length == 4 && strncmp(token, "true", 4) == 0)
	        {
                outValue->type      = JSON_BOOLEAN;
                outValue->length    = 1;
                outValue->boolean   = true;
	        }
	        else if (length == 4 && strncmp(token, "null", 4) == 0)
            {
                outValue->type      = JSON_NULL;
                outValue->length    = 1;
            }
	        else if (length == 5 && strncmp(token, "false", 5) == 0)
	        {
                outValue->type      = JSON_BOOLEAN;
                outValue->length    = 1;
                outValue->boolean   = false;
	        }
	        else
	        {
                char tmp[256];
                tmp[length] = 0;
                while (length--)
                {
                    tmp[length] = token[length]; 
                }

                Json_Panic(state, JSON_NONE, JSON_ERROR_UNEXPECTED, "Unexpected token '%s'", tmp);
	        }
	    } break;
	    /* END OF SWITCH STATEMENT */
        }
    }
}

static char* Json_ParseStringNoToken(JsonState* state, int* outLength)
{
    Json_MatchChar(state, JSON_STRING, '"');

    int   i;
    int   c0, c1;

    JsonTempArray(char, 2048) buffer = JsonTempArray_Init(NULL);
    while (!Json_IsEOF(state) && (c0 = Json_PeekChar(state)) != '"')
    {
        if (c0 == '\\')
        {
            c0 = Json_NextChar(state);
            switch (c0)
            {
            case 'n':
                JsonTempArray_Push(&buffer, '\n', &state->allocator);
                break;

            case 't':
                JsonTempArray_Push(&buffer, '\t', &state->allocator);
                break;

            case 'r':
                JsonTempArray_Push(&buffer, '\r', &state->allocator);
                break;

            case 'b':
                JsonTempArray_Push(&buffer, '\b', &state->allocator);
                break;

            case '\\':
                JsonTempArray_Push(&buffer, '\\', &state->allocator);
                break;

            case '"':
                JsonTempArray_Push(&buffer, '\"', &state->allocator);
                break;

            case 'u':
                c1 = 0;
                for (i = 0; i < 4; i++)
                {
                    if (isxdigit((c0 = Json_NextChar(state))))
                    {
                        c1 = c1 * 10 + (isdigit(c0) ? c0 - '0' : c0 < 'a' ? c0 - 'A' : c0 - 'a');
                    }
                    else
                    {
                        Json_Panic(state, JSON_STRING, JSON_ERROR_UNKNOWN, "Expected hexa character in unicode character");
                    }
                }

                if (c1 <= 0x7F)
                {
                    JsonTempArray_Push(&buffer, (char)c1, &state->allocator);
                }
                else if (c1 <= 0x7FF)
                {
                    char c2 = (char)(0xC0 | (c1 >> 6));            /* 110xxxxx */
                    char c3 = (char)(0x80 | (c1 & 0x3F));          /* 10xxxxxx */
                    JsonTempArray_Push(&buffer, c2, &state->allocator);
                    JsonTempArray_Push(&buffer, c3, &state->allocator);
                }
                else if (c1 <= 0xFFFF)
                {
                    char c2 = (char)(0xE0 | (c1 >> 12));           /* 1110xxxx */
                    char c3 = (char)(0x80 | ((c1 >> 6) & 0x3F));   /* 10xxxxxx */
                    char c4 = (char)(0x80 | (c1 & 0x3F));          /* 10xxxxxx */
                    JsonTempArray_Push(&buffer, c2, &state->allocator);
                    JsonTempArray_Push(&buffer, c3, &state->allocator);
                    JsonTempArray_Push(&buffer, c4, &state->allocator);
                }
                else if (c1 <= 0x10FFFF)
                {
                    char c2 = 0xF0 | (c1 >> 18);           /* 11110xxx */
                    char c3 = 0x80 | ((c1 >> 12) & 0x3F);  /* 10xxxxxx */
                    char c4 = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                    char c5 = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    JsonTempArray_Push(&buffer, c2, &state->allocator);
                    JsonTempArray_Push(&buffer, c3, &state->allocator);
                    JsonTempArray_Push(&buffer, c4, &state->allocator);
                    JsonTempArray_Push(&buffer, c5, &state->allocator);
                }
                break;

            default:
                Json_Panic(state, JSON_STRING, JSON_ERROR_UNKNOWN, "Unknown escape character");
                break;
            }
        }
        else
        {
            switch (c0)
            {
            case '\r':
            case '\n':
                Json_Panic(state, JSON_STRING, JSON_ERROR_UNEXPECTED, "Unexpected newline characters '%c'", c0);
                break;

            default:
                JsonTempArray_Push(&buffer, (char)c0, &state->allocator);
                break;
            }
        }

        Json_NextChar(state);
    }

    Json_MatchChar(state, JSON_STRING, '"');
    if (buffer.count > 0)
    {
        if (outLength) *outLength = JsonTempArray_GetCount(&buffer);
        JsonTempArray_Push(&buffer, 0, &state->allocator);

        char* string = (char*)JsonTempArray_ToBuffer(&buffer, &state->allocator);
        JsonTempArray_Free(&buffer, &state->allocator);

        return string;
    }
    else
    {
        if (outLength) *outLength = 0;
        return NULL;
    }
}

/* @funcdef: Json_ParseString */
static void Json_ParseString(JsonState* state, Json* outValue)
{
    if (Json_SkipSpace(state) > 0)
    {
        int length;
        const char* string = Json_ParseStringNoToken(state, &length);

        outValue->type   = JSON_STRING;
        outValue->string = string;
        outValue->length = length;
    }
}

/* @funcdef: Json_ParseObject */
static void Json_ParseObject(JsonState* state, Json* outValue)
{
    if (Json_SkipSpace(state) > 0)
    {
        Json_MatchChar(state, JSON_OBJECT, '{');

        JsonTempArray(JsonObjectEntry, 32) values = JsonTempArray_Init(NULL);
        while (Json_SkipSpace(state) > 0 && Json_PeekChar(state) != '}')
        {
            if (values.count > 0)
            {
                Json_MatchChar(state, JSON_OBJECT, ',');
            }

            if (Json_SkipSpace(state) != '"')
            {
                Json_Panic(state, JSON_OBJECT, JSON_ERROR_UNEXPECTED, "Expected <string> for <member-key> of <object>");
            }

            int         nameLength;
            const char* name = Json_ParseStringNoToken(state, &nameLength);

            Json_SkipSpace(state);
            Json_MatchChar(state, JSON_OBJECT, ':');

            Json value;
            Json_ParseSingle(state, &value);

            /* Well done */
            JsonObjectEntry entry;
            entry.hash       = JsonHash(name, nameLength);
            entry.name       = name;
            entry.value      = value;
            entry.nameLength = nameLength;
            JsonTempArray_Push(&values, entry, &state->allocator);
        }

        Json_SkipSpace(state);
        Json_MatchChar(state, JSON_OBJECT, '}');

        outValue->type   = JSON_OBJECT;
        outValue->length = JsonTempArray_GetCount(&values);
        outValue->object = (JsonObjectEntry*)JsonTempArray_ToBuffer(&values, &state->allocator);

        JsonTempArray_Free(&values, &state->allocator);
    }
}
         
/* Internal parsing function
 */
static Json* Json_ParseTopLevel(JsonState* state)
{
    if (!state)
    {
        return NULL;
    }

    if (Json_SkipSpace(state) == '{')
    {
        if (setjmp(state->errjmp) == 0)
        {
            Json_ParseObject(state, &state->root);

            Json_SkipSpace(state);
            if (!Json_IsEOF(state))
            {
                Json_Panic(state, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <object>.");
            }

            return &state->root;
        }
        else
        {
            return NULL;
        }
    }
    else if (Json_SkipSpace(state) == '[')
    {
        if (setjmp(state->errjmp) == 0)
        {
            Json_ParseArray(state, &state->root);

            Json_SkipSpace(state);
            if (!Json_IsEOF(state))
            {
                Json_Panic(state, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <array>.");
            }

            return &state->root;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        Json_SetError(state, JSON_NONE, JSON_ERROR_FORMAT, "JSON must be starting with '{' or '[', first character is '%c'", Json_PeekChar(state));
        return NULL;
    }
}

/* @funcdef: JsonParse */
Json* JsonParse(const char* json, int jsonLength, JsonState** outState)
{
    JsonAllocator allocator;
    allocator.data  = NULL;
    allocator.free  = Json_Free;
    allocator.alloc = Json_Alloc;

    return JsonParseEx(json, jsonLength, allocator, outState);
}

/* @funcdef: JsonParseEx */
Json* JsonParseEx(const char* json, int jsonLength, JsonAllocator allocator)
{
    JsonState* state = JsonState_Make(json, jsonLength, allocator);
    Json* value = Json_ParseTopLevel(state);

    if (!value)
    {
        JsonState_Free(state);
    }

    return value;
}

/* @funcdef: JsonRelease */
void JsonRelease(Json* rootValue)
{
    if (rootValue)
    {
        JsonState* state = JSON_SUPEROF(rootValue, JsonState, root);
        if (state->reversed0 == JSON_REVERSED0 && state->reversed1)
        {
            JsonState_Free(state);
        }
    }
}

/* @funcdef: JsonGetError */
JsonError JsonGetError(const Json* rootValue)
{
    if (rootValue)
    {
        JsonState* state = JSON_SUPEROF(rootValue, JsonState, root);
        if (state->reversed0 == JSON_REVERSED0 && state->reversed1 == JSON_REVERSED1)
        {
            return state->errnum;
        }
    }

    return JSON_ERROR_NONE;
}

/* @funcdef: JsonGetErrorString */
const char* JsonGetErrorString(const Json* rootValue)
{
    if (rootValue)
    {
        JsonState* state = JSON_SUPEROF(rootValue, JsonState, root);
        if (state->reversed0 == JSON_REVERSED0 && state->reversed1)
        {
            return state->errmsg;
        }
    }

    return JSON_ERROR_NONE;
}

/* @funcdef: JsonEquals */
bool JsonEquals(const Json* a, const Json* b)
{
    int i, n;

    if (a == b)
    {
        return true;
    }

    if (!a || !b)
    {
        return false;
    }

    if (a->type != b->type)
    {
        return false;
    }

    switch (a->type)
    {
    case JSON_NULL:
    case JSON_NONE:
        return true;

    case JSON_NUMBER:
        return a->number == b->number;

    case JSON_BOOLEAN:
        return a->boolean == b->boolean;

    case JSON_ARRAY:
        if ((n = a->length) == a->length)
        {
            for (i = 0; i < n; i++)
            {
                if (!JsonEquals(&a->array[i], &b->array[i]))
                {
                    return false;
                }
            }
        }
        return true;

    case JSON_OBJECT:
        if ((n = a->length) == b->length)
        {
            for (i = 0; i < n; i++)
            {
                if (a->object[i].hash != b->object[i].hash || strncmp(a->object[i].name, b->object[i].name, n) != 0)
                {
                    return false;
                }

                if (!JsonEquals(&a->object[i].value, &b->object[i].value))
                {
                    return false;
                }
            }
        }
        return true;

    case JSON_STRING:
        return a->length == b->length && strncmp(a->string, b->string, a->length) == 0;
    }

    return false;
}

/* @funcdef: JsonHash */
int JsonHash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

/* @funcdef: JsonFind */
Json* JsonFind(const Json* obj, const char* name)
{
    if (obj && obj->type == JSON_OBJECT)
    {
        int i, n;
        int len  = (int)strlen(name);
        int hash = JsonHash(name, len);
        for (i = 0, n = obj->length; i < n; i++)
        {
            JsonObjectEntry* entry = &obj->object[i];
            if (hash == entry->hash && strncmp(name, entry->name, len) == 0)
            {
                return &entry->value;
            }
        }
    }

    return NULL;
}

/* @funcdef: JsonFindWithHash */
Json* JsonFindWithHash(const Json* obj, int hash)
{
    if (obj && obj->type == JSON_OBJECT)
    {
        int i, n;
        for (i = 0, n = obj->length; i < n; i++)
        {
            if (hash == obj->object[i].hash)
            {
                return &obj->object[i].value;
            }
        }
    }

    return NULL;
}
