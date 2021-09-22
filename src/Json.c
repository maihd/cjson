#ifndef __JSON_H__
#include "Json.h"
#endif // __JSON_H__

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>

#ifndef JSON_INLINE
#  if defined(_MSC_VER)
#     define JSON_INLINE static __forceinline
#  elif defined(__GNUC__) || defined(__clang__)
#     define JSON_INLINE static __inline__ __attribute__((always_inline))
#  elif defined(__cplusplus)
#     define JSON_INLINE static inline
#  else
#     define JSON_INLINE static 
#  endif
#endif

// -----------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------

typedef struct JsonAllocator
{
    uint8_t*        buffer;
    int32_t         length;

    uint8_t*        lowerMarker;
    uint8_t*        upperMarker;
} JsonAllocator;

static bool JsonAllocator_Init(JsonAllocator* allocator, void* buffer, int32_t length)
{
    if (buffer && length > 0)
    {
        allocator->buffer       = (uint8_t*)buffer;
        allocator->length       = length;
        allocator->lowerMarker  = (uint8_t*)buffer;
        allocator->upperMarker  = (uint8_t*)buffer + length;

        return true;
    }

    return false;
}

static bool JsonAllocator_CanAlloc(JsonAllocator* allocator, int32_t size)
{
    int32_t remain = (int32_t)(allocator->upperMarker - allocator->lowerMarker);
    return  remain >= size;
}

static void JsonAllocator_FreeLower(JsonAllocator* allocator, void* buffer, int32_t size)
{
    void* lastBuffer = allocator->lowerMarker;
    if (lastBuffer == buffer)
    {
        allocator->lowerMarker -= size;
    }
}

static void* JsonAllocator_AllocLower(JsonAllocator* allocator, void* oldBuffer, int32_t oldSize, int32_t size)
{
    JsonAllocator_FreeLower(allocator, oldBuffer, oldSize);

    if (JsonAllocator_CanAlloc(allocator, size))
    {
        void* result = allocator->lowerMarker;
        allocator->lowerMarker += size;
        return result;
    }

    return NULL;
}

static void JsonAllocator_FreeUpper(JsonAllocator* allocator, void* buffer, int32_t size)
{
    void* lastBuffer = allocator->upperMarker;
    if (lastBuffer == buffer)
    {
        allocator->upperMarker += size;
    }
}

static void* JsonAllocator_AllocUpper(JsonAllocator* allocator, void* oldBuffer, int32_t oldSize, int32_t newSize)
{
    JsonAllocator_FreeUpper(allocator, oldBuffer, oldSize);

    if (JsonAllocator_CanAlloc(allocator, newSize))
    {
        allocator->upperMarker -= newSize;
        return allocator->upperMarker;
    }

    return NULL;
}

/* 
JsonArray: dynamic, scalable array
@note: internal only
*/
typedef struct JsonArray
{
    int32_t size;
    int32_t count;
    Json    buffer[];
} JsonArray;

#define JsonArray_getHeader(a)              ((JsonArray*)(a) - 1)
#define JsonArray_init()                    0
#define JsonArray_free(a, alloc)            JsonAllocator_FreeUpper(alloc, a ? JsonArray_getHeader(a) : NULL, JsonArray_getSize(a) * sizeof((a)[0]))
#define JsonArray_getSize(a)                ((a) ? JsonArray_getHeader(a)->size  : 0)
#define JsonArray_getCount(a)               ((a) ? JsonArray_getHeader(a)->count : 0)
#define JsonArray_getUsageMemory(a)         (sizeof(JsonArray) + JsonArray_getCount(a) * sizeof(*(a)))
#define JsonArray_push(a, v, alloc)         (JsonArray_ensureSize(a, JsonArray_getCount(a) + 1, alloc) ? ((void)((a)[JsonArray_getHeader(a)->count++] = v), 1) : 0)
#define JsonArray_pop(a, v, alloc)          ((a)[--JsonArray_getHeader(a)->count]);
#define JsonArray_ensureSize(a, n, alloc)   ((!(a) || JsonArray_getSize(a) < (n)) ? (*((void**)&(a))=JsonArray_Grow(a, n + 1, sizeof((a)[0]), alloc)) != NULL : 1)
#define JsonArray_clear(a)                  ((a) ? (void)(JsonArray_getHeader(a)->count = 0) : (void)0)
#define JsonArray_clone(a, alloc)           ((a) ? memcpy(JsonArray_Grow(0, JsonArray_getCount(a), sizeof((a)[0]), alloc), JsonArray_getHeader(a), JsonArray_getUsageMemory(a)) : NULL)

JSON_INLINE void* JsonArray_Grow(void* array, int32_t reqsize, int32_t elemsize, JsonAllocator* allocator)
{
    assert(elemsize > 0);
    assert(allocator != NULL);

    JsonArray*  raw   = array ? JsonArray_getHeader(array) : NULL;
    int32_t     size  = JsonArray_getSize(array);
    int32_t     count = JsonArray_getCount(array);

    if (size >= reqsize)
    {
        return array;
    }

    int32_t newSize = size;
    while (newSize < reqsize)
    {
        newSize += 32;
    }

    JsonArray* newArray = (JsonArray*)JsonAllocator_AllocUpper(allocator, raw, sizeof(JsonArray) + size * elemsize, sizeof(JsonArray) + newSize * elemsize);
    if (newArray)
    {
        newArray->size  = size;
        newArray->count = count;

        if (raw && count > 0)
        {
            memcpy(newArray->buffer, raw->buffer, count * elemsize);
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
#define JsonTempArray(T, CAPACITY)                  \
    struct {                                        \
        T*      array;                              \
        int32_t count;                              \
        T       buffer[CAPACITY];                   \
    }

#define JsonTempArray_init(a)             { a, 0 }
#define JsonTempArray_free(a, alloc)      JsonArray_free((a)->array, alloc)
#define JsonTempArray_push(a, v, alloc)   ((a)->count >= sizeof((a)->buffer) / sizeof((a)->buffer[0]) ? JsonArray_push((a)->array, v, alloc) : ((a)->buffer[(a)->count++] = v, 1))
#define JsonTempArray_getCount(a)         ((a)->count + JsonArray_getCount((a)->array))
#define JsonTempArray_toBuffer(a, alloc)  JsonTempArray_toBufferFunc((a)->buffer, (a)->count, (a)->array, (int)sizeof((a)->buffer[0]), alloc)

JSON_INLINE void* JsonTempArray_toBufferFunc(void* buffer, int32_t count, void* dynamicBuffer, int32_t itemSize, JsonAllocator* allocator)
{
    int total = count + JsonArray_getCount(dynamicBuffer);
    if (total > 0)
    {
        void* array = (JsonArray*)JsonAllocator_AllocLower(allocator, NULL, 0, total * itemSize);
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
    JsonFlags           flags;

    int32_t             line;
    int32_t             column;
    int32_t             cursor;
    //JsonType            parsingType;
    
    int32_t             length;         /* Reference only */
    const char*         buffer;         /* Reference only */
    
    JsonErrorCode       errnum;
    char*               errmsg;
    jmp_buf             errjmp;

    JsonAllocator       allocator;      /* Runtime allocator */
};

static void Json_setErrorWithArgs(JsonState* state, JsonType type, JsonErrorCode code, const char* fmt, va_list valist)
{
    const int errmsg_size = 1024;

    const char* type_name;
    switch (type)
    {
    case JsonType_Null:
        type_name = "null";
        break;

    case JsonType_Boolean:
        type_name = "boolean";
        break;

    case JsonType_Number:
        type_name = "number";
        break;

    case JsonType_Array:
        type_name = "array";
        break;

    case JsonType_String:
        type_name = "string";
        break;

    case JsonType_Object:
        type_name = "object";
        break;

    default:
        type_name = "unknown";
        break;
    }

    state->errnum = code;
    if (state->errmsg == NULL)
    {
        state->errmsg = (char*)JsonAllocator_AllocUpper(&state->allocator, NULL, 0, errmsg_size);
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

/* @funcdef: Json_setError */
static void Json_setError(JsonState* state, JsonType type, JsonErrorCode code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_setErrorWithArgs(state, type, code, fmt, varg);
    va_end(varg);
}

/* funcdef: Json_panic */
static void Json_panic(JsonState* state, JsonType type, JsonErrorCode code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_setErrorWithArgs(state, type, code, fmt, varg);
    va_end(varg);

    longjmp(state->errjmp, code);
}

/* @funcdef: JsonState_Init */
static void JsonState_Init(JsonState* state, const char* jsonCode, int32_t jsonLength, JsonAllocator allocator, JsonFlags flags)
{
    if (state)
    {
        state->flags        = flags;

		state->line         = 1;
		state->column       = 1;
		state->cursor       = 0;
		state->buffer       = jsonCode;
		state->length       = jsonLength;

		state->errmsg       = NULL;
		state->errnum       = JsonError_None;

        state->allocator    = allocator;
    }
}

/* @funcdef: JsonState_isEOF */
static int JsonState_isEOF(const JsonState* state)
{
    return state->cursor >= state->length || state->buffer[state->cursor] <= 0;
}

/* @funcdef: JsonState_peekChar */
static int JsonState_peekChar(const JsonState* state)
{
    return state->buffer[state->cursor];
}

/* @funcdef: JsonState_nextChar */
static int JsonState_nextChar(JsonState* state)
{
    if (JsonState_isEOF(state))
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

/* @funcdef: JsonState_nextLine */
static int JsonState_nextLine(JsonState* state)
{
    if (JsonState_isEOF(state))
    {
        return -1;
    }
    else
    {
        int c = state->buffer[state->cursor];
        while (c != '\n')
        {
            c = state->buffer[++state->cursor];
        }

        c = state->buffer[++state->cursor];
        state->line++;
        state->column = 1;
        return c;
    }
}

/* @funcdef: JsonState_skipSpace */
static int JsonState_skipSpace(JsonState* state)
{
    int c = JsonState_peekChar(state);
    while (c > 0 && isspace(c))
    {
		c = JsonState_nextChar(state);
    }
    return c;
}

/* @funcdef: JsonState_matchChar */
static int JsonState_matchChar(JsonState* state, JsonType type, int c)
{
    if (JsonState_peekChar(state) == c)
    {
		return JsonState_nextChar(state);
    }
    else
    {
        Json_panic(state, type, JsonError_UnmatchToken, "Expected '%c'", (char)c);
		return -1;
    }
}

/* @funcdef: JsonState_skipComments */
static int JsonState_skipComments(JsonState* state)
{
    while (true)
    {
        int c = JsonState_nextChar(state);
        if (c == '/')
        {
            c = JsonState_nextChar(state);
            if (c == '/')
            {
                JsonState_nextLine(state);
            }    
            else if (c == '*')
            {
                int c0 = JsonState_nextChar(state);
                int c1 = JsonState_nextChar(state);
                while (c0 != '*' || c1 != '/')
                {
                    c0 = c1;
                    c1 = JsonState_nextChar(state);
                }
            }
            else
            {
                Json_panic(state, JsonType_Null, JsonError_UnexpectedToken, "Unexpected token '%c'", c);
            }
        }
        else
        {
            break;
        }
    }
    
    return JsonState_peekChar(state);
}

/* All parse functions declaration */

static void JsonState_parseArray(JsonState* state, Json* outValue);
static void JsonState_parseSingle(JsonState* state, Json* outValue);
static void JsonState_parseObject(JsonState* state, Json* outValue);
static void JsonState_parseNumber(JsonState* state, Json* outValue);
static void JsonState_parseString(JsonState* state, Json* outValue);

/* @funcdef: JsonState_parseNumber */
static void JsonState_parseNumber(JsonState* state, Json* outValue)
{
    if (JsonState_skipSpace(state) > 0)
    {
		int c = JsonState_peekChar(state);
		int sign = 1;
		
		if (c == '+')
		{
			c = JsonState_nextChar(state);
			Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "JSON does not support number start with '+'");
		}
		else if (c == '-')
		{
			sign = -1;
			c = JsonState_nextChar(state);
		}
		else if (c == '0')
		{
			c = JsonState_nextChar(state);
			if (!isspace(c) && !ispunct(c))
			{
				Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "JSON does not support number start with '0' (only standalone '0' is accepted)");
			}
		}
		else if (!isdigit(c))
		{
			Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "Unexpected '%c'", c);
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
                    Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "Too many 'e' are presented in a <number>");
                }
                else if (dot && numpow == 1)
                {
                    Json_panic(state, JsonType_Number, JsonError_UnexpectedToken,
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
                    Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "Cannot has '.' after 'e' is presented in a <number>");
                }
				else if (dot)
				{
					Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "Too many '.' are presented in a <number>");
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
                    Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "'%c' is presented after digits are presented of exponent part", c);
                }
                else if (expsgn)
                {
                    Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "Too many signed characters are presented after 'e'");
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

			c = JsonState_nextChar(state);
		}

        if (exp && !expchk)
        {
            Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "'e' is presented in number token, but require a digit after 'e' ('%c')", (char)c);
        }
		if (dot && numpow == 1)
		{
			Json_panic(state, JsonType_Number, JsonError_UnexpectedToken, "'.' is presented in number token, but require a digit after '.' ('%c')", (char)c);
		}
		else
		{
            Json value = { JsonType_Number };
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

/* @funcdef: JsonState_parseArray */
static void JsonState_parseArray(JsonState* state, Json* outValue)
{
    if (JsonState_skipSpace(state) > 0)
    {
	    JsonState_matchChar(state, JsonType_Array, '[');

        JsonTempArray(Json, 64) values = JsonTempArray_init(NULL);
	    while (JsonState_skipSpace(state) > 0 && JsonState_peekChar(state) != ']')
	    {
	        if (values.count > 0)
	        {
                JsonState_matchChar(state, JsonType_Array, ',');
	        }
	    
            Json value;
            JsonState_parseSingle(state, &value);

            JsonTempArray_push(&values, value, &state->allocator);
	    }

	    JsonState_skipSpace(state);
	    JsonState_matchChar(state, JsonType_Array, ']');

        outValue->type   = JsonType_Array;
        outValue->length = JsonTempArray_getCount(&values);
        outValue->array  = (Json*)JsonTempArray_toBuffer(&values, &state->allocator);

        JsonTempArray_free(&values, &state->allocator);
    }
}

/* JsonState_ParseSingle */
static void JsonState_parseSingle(JsonState* state, Json* outValue)
{
    if (JsonState_skipSpace(state) > 0)
    {
	    int c = JsonState_peekChar(state);
	
	    switch (c)
	    {
	    case '[':
	        JsonState_parseArray(state, outValue);
            break;
	    
	    case '{':
	        JsonState_parseObject(state, outValue);
            break;
	    
	    case '"':
	        JsonState_parseString(state, outValue);
            break;

	    case '+': case '-': case '0': 
        case '1': case '2': case '3': 
        case '4': case '5': case '6': 
        case '7': case '8': case '9':
	        JsonState_parseNumber(state, outValue);
            break;


        case '/':
            if (state->flags & JsonFlags_SupportComment)
            {
                JsonState_skipComments(state);
                JsonState_parseSingle(state, outValue);
            }
            else
            {
                Json_panic(state, JsonType_String, JsonError_UnknownToken, "Unknown token '%c'", c);
            }
            break;
	    
        default:
	    {
	        int length = 0;
	        while (c > 0 && isalpha(c))
	        {
                length++;
                c = JsonState_nextChar(state);
	        }

	        const char* token = state->buffer + state->cursor - length;
	        if (length == 4 && strncmp(token, "true", 4) == 0)
	        {
                outValue->type      = JsonType_Boolean;
                outValue->length    = 1;
                outValue->boolean   = true;
	        }
	        else if (length == 4 && strncmp(token, "null", 4) == 0)
            {
                outValue->type      = JsonType_Null;
                outValue->length    = 1;
            }
	        else if (length == 5 && strncmp(token, "false", 5) == 0)
	        {
                outValue->type      = JsonType_Boolean;
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

                Json_panic(state, JsonType_Null, JsonError_UnexpectedToken, "Unexpected token '%s'", tmp);
	        }
	    } break;
	    /* END OF SWITCH STATEMENT */
        }
    }
}

static char* JsonState_parseStringNoToken(JsonState* state, int* outLength)
{
    JsonState_matchChar(state, JsonType_String, '"');

    int i;
    int c0, c1;

    JsonTempArray(char, 2048) buffer = JsonTempArray_init(NULL);
    while (!JsonState_isEOF(state) && (c0 = JsonState_peekChar(state)) != '"')
    {
        if (c0 == '\\')
        {
            c0 = JsonState_nextChar(state);
            switch (c0)
            {
            case 'n':
                JsonTempArray_push(&buffer, '\n', &state->allocator);
                break;

            case 't':
                JsonTempArray_push(&buffer, '\t', &state->allocator);
                break;

            case 'r':
                JsonTempArray_push(&buffer, '\r', &state->allocator);
                break;

            case 'b':
                JsonTempArray_push(&buffer, '\b', &state->allocator);
                break;

            case '\\':
                JsonTempArray_push(&buffer, '\\', &state->allocator);
                break;

            case '"':
                JsonTempArray_push(&buffer, '\"', &state->allocator);
                break;

            case 'u':
                c1 = 0;
                for (i = 0; i < 4; i++)
                {
                    if (isxdigit((c0 = JsonState_nextChar(state))))
                    {
                        c1 = c1 * 10 + (isdigit(c0) ? c0 - '0' : c0 < 'a' ? c0 - 'A' : c0 - 'a');
                    }
                    else
                    {
                        Json_panic(state, JsonType_String, JsonError_UnknownToken, "Expected hexa character in unicode character");
                    }
                }

                if (c1 <= 0x7F)
                {
                    JsonTempArray_push(&buffer, (char)c1, &state->allocator);
                }
                else if (c1 <= 0x7FF)
                {
                    char c2 = (char)(0xC0 | (c1 >> 6));            /* 110xxxxx */
                    char c3 = (char)(0x80 | (c1 & 0x3F));          /* 10xxxxxx */
                    JsonTempArray_push(&buffer, c2, &state->allocator);
                    JsonTempArray_push(&buffer, c3, &state->allocator);
                }
                else if (c1 <= 0xFFFF)
                {
                    char c2 = (char)(0xE0 | (c1 >> 12));           /* 1110xxxx */
                    char c3 = (char)(0x80 | ((c1 >> 6) & 0x3F));   /* 10xxxxxx */
                    char c4 = (char)(0x80 | (c1 & 0x3F));          /* 10xxxxxx */
                    JsonTempArray_push(&buffer, c2, &state->allocator);
                    JsonTempArray_push(&buffer, c3, &state->allocator);
                    JsonTempArray_push(&buffer, c4, &state->allocator);
                }
                else if (c1 <= 0x10FFFF)
                {
                    char c2 = 0xF0 | (c1 >> 18);           /* 11110xxx */
                    char c3 = 0x80 | ((c1 >> 12) & 0x3F);  /* 10xxxxxx */
                    char c4 = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                    char c5 = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    JsonTempArray_push(&buffer, c2, &state->allocator);
                    JsonTempArray_push(&buffer, c3, &state->allocator);
                    JsonTempArray_push(&buffer, c4, &state->allocator);
                    JsonTempArray_push(&buffer, c5, &state->allocator);
                }
                break;

            default:
                Json_panic(state, JsonType_String, JsonError_UnknownToken, "Unknown escape character");
                break;
            }
        }
        else
        {
            switch (c0)
            {
            case '\r':
            case '\n':
                Json_panic(state, JsonType_String, JsonError_UnexpectedToken, "Unexpected newline characters '%c'", c0);
                break;

            default:
                JsonTempArray_push(&buffer, (char)c0, &state->allocator);
                break;
            }
        }

        JsonState_nextChar(state);
    }

    JsonState_matchChar(state, JsonType_String, '"');
    if (buffer.count > 0)
    {
        if (outLength) *outLength = JsonTempArray_getCount(&buffer);
        JsonTempArray_push(&buffer, 0, &state->allocator);

        char* string = (char*)JsonTempArray_toBuffer(&buffer, &state->allocator);
        JsonTempArray_free(&buffer, &state->allocator);

        return string;
    }
    else
    {
        if (outLength) *outLength = 0;
        return NULL;
    }
}

/* @funcdef: JsonState_parseString */
static void JsonState_parseString(JsonState* state, Json* outValue)
{
    if (JsonState_skipSpace(state) > 0)
    {
        int length;
        const char* string = JsonState_parseStringNoToken(state, &length);

        outValue->type   = JsonType_String;
        outValue->string = string;
        outValue->length = length;
    }
}

/* @funcdef: JsonState_parseObject */
static void JsonState_parseObject(JsonState* state, Json* outValue)
{
    if (JsonState_skipSpace(state) > 0)
    {
        JsonState_matchChar(state, JsonType_Object, '{');

        JsonTempArray(JsonObjectMember, 32) values = JsonTempArray_init(NULL);
        while (JsonState_skipSpace(state) > 0 && JsonState_peekChar(state) != '}')
        {
            if (values.count > 0)
            {
                JsonState_matchChar(state, JsonType_Object, ',');
            }

            if (JsonState_skipSpace(state) != '"')
            {
                Json_panic(state, JsonType_Object, JsonError_UnexpectedToken, "Expected <string> for <member-key> of <object>");
            }

            const char* name = JsonState_parseStringNoToken(state, 0);

            JsonState_skipSpace(state);
            JsonState_matchChar(state, JsonType_Object, ':');

            Json value;
            JsonState_parseSingle(state, &value);

            /* Well done */
            JsonObjectMember member;
            member.name  = name;
            member.value = value;
            JsonTempArray_push(&values, member, &state->allocator);
        }

        JsonState_skipSpace(state);
        JsonState_matchChar(state, JsonType_Object, '}');

        outValue->type   = JsonType_Object;
        outValue->length = JsonTempArray_getCount(&values);
        outValue->object = (JsonObjectMember*)JsonTempArray_toBuffer(&values, &state->allocator);

        JsonTempArray_free(&values, &state->allocator);
    }
}
         
/* Internal parsing function
 */
static Json* JsonState_parseTopLevel(JsonState* state)
{
    Json* value = (Json*)JsonAllocator_AllocLower(&state->allocator, NULL, 0, sizeof(Json));
    value->type = JsonType_Null;

    if (!state)
    {
        return NULL;
    }

    // Skip meta comment in header of the file
    if (state->flags & JsonFlags_SupportComment)
    {
        JsonState_skipSpace(state);
        JsonState_skipComments(state);
    }

    // Just parse value from the top level
    if (state->flags & JsonFlags_NoStrictTopLevel)
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonState_parseSingle(state, value);
            return value;
        }
        else
        {
            return NULL;
        }
    }
    // Make sure the toplevel is JsonType_Object
    else if (JsonState_skipSpace(state) == '{')
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonState_parseObject(state, value);

            JsonState_skipSpace(state);
            if (!JsonState_isEOF(state))
            {
                Json_panic(state, JsonType_Null, JsonError_WrongFormat, "JSON is not well-formed. JSON is start with <object>.");
            }

            return value;
        }
        else
        {
            return NULL;
        }
    }
    // Make sure the toplevel is JsonType_Array
    else if (JsonState_skipSpace(state) == '[')
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonState_parseArray(state, value);

            JsonState_skipSpace(state);
            if (!JsonState_isEOF(state))
            {
                Json_panic(state, JsonType_Null, JsonError_WrongFormat, "JSON is not well-formed. JSON is start with <array>.");
            }

            return value;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        Json_setError(state, JsonType_Null, JsonError_WrongFormat, "JSON must be starting with '{' or '[', first character is '%c'", JsonState_peekChar(state));
        return NULL;
    }
}

/* @funcdef: JsonParse */
JsonError JsonParse(const char* jsonCode, int32_t jsonCodeLength, JsonFlags flags, void* buffer, int32_t bufferSize, Json** result)
{
    if (!buffer || bufferSize < sizeof(JsonState))
    {
        JsonError error = { JsonError_OutOfMemory, "Buffer is too small" };
        return error;
    }

    JsonAllocator tempAllocator;
    JsonAllocator_Init(&tempAllocator, buffer, bufferSize);

    JsonState state;
    JsonState_Init(&state, jsonCode, jsonCodeLength, tempAllocator, flags);

    Json* value = JsonState_parseTopLevel(&state);

    *result = value;

    JsonError error = { state.errnum, state.errmsg };
    return error;
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
    case JsonType_Null:
        return true;

    case JsonType_Number:
        return a->number == b->number;

    case JsonType_Boolean:
        return a->boolean == b->boolean;

    case JsonType_Array:
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

    case JsonType_Object:
        if ((n = a->length) == b->length)
        {
            for (i = 0; i < n; i++)
            {
                if (strncmp(a->object[i].name, b->object[i].name, n) != 0)
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

    case JsonType_String:
        return a->length == b->length && strncmp(a->string, b->string, a->length) == 0;
    }

    return false;
}

/* @funcdef: JsonFind */
const Json* JsonFind(const Json* obj, const char* name)
{
    if (obj && obj->type == JsonType_Object)
    {
        int i, n;
        int len  = (int)strlen(name);
        for (i = 0, n = obj->length; i < n; i++)
        {
            JsonObjectMember* member = &obj->object[i];
            if (strncmp(name, member->name, len) == 0)
            {
                return &member->value;
            }
        }
    }

    return NULL;
}
