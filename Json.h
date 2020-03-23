#ifndef __JSON_H__
#define __JSON_H__
/******************************************************
 * Simple json state written in ANSI C
 *
 * @author: MaiHD
 * @license: Public domain
 * @copyright: MaiHD @ ${HOME}, 2018 - 2020
 ******************************************************/

#pragma once

#ifndef JSON_API
#define JSON_API
#endif

/* START OF EXTERN "C" */
#ifdef __cplusplus
extern "C" {
#endif

/* Define boolean type if needed */
#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
typedef char bool;
enum { false = 0, true = 1 };
#endif

/**
 * JSON type of json value
 */
typedef enum JsonType
{
    JsonType_Null,
    JsonType_Array,
    JsonType_Object,
    JsonType_Number,
    JsonType_String,
    JsonType_Boolean,
} JsonType;

/**
 * JSON error code
 */
typedef enum JsonError
{
    JsonError_None,

    JsonError_InvalidValue,

    /* Parsing error */

    JsonError_WrongFormat,
    JsonError_UnmatchToken,
    JsonError_UnknownToken,
    JsonError_UnexpectedToken,
    JsonError_UnsupportedToken,

    /* Runtime error */

    JsonError_OutOfMemory,
    JsonError_InternalFatal,
} JsonError;

typedef struct Json             Json;
typedef struct JsonAllocator    JsonAllocator;
typedef struct JsonObjectEntry  JsonObjectEntry;

JSON_API Json*          Json_parse(const char* jsonCode, int jsonCodeLength);
JSON_API Json*          Json_parseEx(const char* jsonCode, int jsonCodeLength, JsonAllocator allocator);

JSON_API void           Json_release(Json* rootValue);

JSON_API JsonError      Json_getError(const Json* rootValue);
JSON_API const char*    Json_getErrorMessage(const Json* rootValue);

JSON_API bool           Json_equals(const Json* a, const Json* b);

JSON_API Json*          Json_find(const Json* x, const char* name);

struct Json
{
    JsonType type;                      /* Type of value: number, boolean, string, array, object    */
    int      length;                    /* Length of value, always 1 on primitive types             */
    union
    {
        double              number;
        bool                boolean;

        const char*         string;

        Json*               array;

        JsonObjectEntry*    object;
    };
};

struct JsonObjectEntry
{
    const char* name;
    Json        value;
};

struct JsonAllocator
{
    void* data;
    void  (*free)(void* data, void* ptr);
    void* (*alloc)(void* data, int size);
    //void* (*realloc)(void* data, void* ptr, int size);
};

/* END OF EXTERN "C" */
#ifdef __cplusplus
}
#endif
/* * */
#endif /* __JSON_H__ */

#ifdef JSON_IMPL

/******************************************************
 * Simple json state written in ANSI C
 *
 * @author: MaiHD
 * @license: Public domain
 * @copyright: MaiHD @ ${HOME}, 2018 - 2020
 ******************************************************/

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
#include <stdint.h>

#define JSON_SUPEROF(ptr, T, member) (T*)((char*)ptr - offsetof(T, member))

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

static const uint64_t JSON_REVERSED = (1246973774ULL << 31) | 1785950062ULL;

/*
Utility
*/

#define JSON_ALLOC(a, size) (a)->alloc((a)->data, size) 
#define JSON_FREE(a, ptr)   (a)->free((a)->data, ptr)

/* Next power of two */
JSON_INLINE int Json_nextPOT(int x)
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
    int     size;
    int     count;
    Json    buffer[];
} JsonArray;

#define JsonArray_getHeader(a)              ((JsonArray*)(a) - 1)
#define JsonArray_init()                    0
#define JsonArray_free(a, alloc)            ((a) ? (alloc)->free((alloc)->data, JsonArray_getHeader(a)) : (void)0)
#define JsonArray_getSize(a)                ((a) ? JsonArray_getHeader(a)->size  : 0)
#define JsonArray_getCount(a)               ((a) ? JsonArray_getHeader(a)->count : 0)
#define JsonArray_getUsageMemory(a)         (sizeof(JsonArray) + JsonArray_getCount(a) * sizeof(*(a)))
#define JsonArray_push(a, v, alloc)         (JsonArray_ensureSize(a, JsonArray_getCount(a) + 1, alloc) ? ((void)((a)[JsonArray_getHeader(a)->count++] = v), 1) : 0)
#define JsonArray_pop(a, v, alloc)          ((a)[--JsonArray_getHeader(a)->count]);
#define JsonArray_ensureSize(a, n, alloc)   ((!(a) || JsonArray_getSize(a) < (n)) ? (*((void**)&(a))=JsonArray_grow(a, Json_nextPOT(n), sizeof((a)[0]), alloc)) != NULL : 1)
#define JsonArray_clear(a)                  ((a) ? (void)(JsonArray_getHeader(a)->count = 0) : (void)0)
#define JsonArray_clone(a, alloc)           ((a) ? memcpy(JsonArray_grow(0, JsonArray_getCount(a), sizeof((a)[0]), alloc), JsonArray_getHeader(a), JsonArray_getUsageMemory(a)) : NULL)

JSON_INLINE void* JsonArray_grow(void* array, int reqsize, int elemsize, JsonAllocator* allocator)
{
    assert(elemsize > 0);
    assert(allocator != NULL);

    JsonArray*  raw   = array ? JsonArray_getHeader(array) : NULL;
    int         size  = JsonArray_getSize(array);
    int         count = JsonArray_getCount(array);

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

#define JsonTempArray_init(a)             { a, 0 }
#define JsonTempArray_free(a, alloc)      JsonArray_free((a)->array, alloc)
#define JsonTempArray_push(a, v, alloc)   ((a)->count >= sizeof((a)->buffer) / sizeof((a)->buffer[0]) ? JsonArray_push((a)->array, v, alloc) : ((a)->buffer[(a)->count++] = v, 1))
#define JsonTempArray_getCount(a)         ((a)->count + JsonArray_getCount((a)->array))
#define JsonTempArray_toBuffer(a, alloc)  JsonTempArray_toBufferFunc((a)->buffer, (a)->count, (a)->array, (int)sizeof((a)->buffer[0]), alloc)

JSON_INLINE void* JsonTempArray_toBufferFunc(void* buffer, int count, void* dynamicBuffer, int itemSize, JsonAllocator* allocator)
{
    int total = count + JsonArray_getCount(dynamicBuffer);
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
    uint64_t            reversed;

    Json                root;
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

/* @funcdef: Json_alloc */
static void* Json_alloc(void* data, int size)
{
    (void)data;
    return malloc(size);
}

/* @funcdef: Json_free */
static void Json_free(void* data, void* pointer)
{
    (void)data;
    free(pointer);
}

static void Json_setErrorWithArgs(JsonState* state, JsonType type, JsonError code, const char* fmt, va_list valist)
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

/* @funcdef: Json_setError */
static void Json_setError(JsonState* state, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_setErrorWithArgs(state, type, code, fmt, varg);
    va_end(varg);
}

/* funcdef: Json_panic */
static void Json_panic(JsonState* state, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_setErrorWithArgs(state, type, code, fmt, varg);
    va_end(varg);

    longjmp(state->errjmp, code);
}

static void Json_releaseMemory(Json* value, JsonAllocator* allocator)
{
    if (value)
    {
        int i, n;
        switch (value->type)
        {
        case JsonType_Array:
            for (i = 0, n = value->length; i < n; i++)
            {
                Json_releaseMemory(&value->array[i], allocator);
            }
            JSON_FREE(allocator, value->array);
            break;

        case JsonType_Object:
            for (i = 0, n = value->length; i < n; i++)
            {
                Json_releaseMemory(&value->object[i].value, allocator);
            }
            JSON_FREE(allocator, value->object);
            break;

        case JsonType_String:
            JSON_FREE(allocator, (void*)value->string);
            break;

        default:
            break;
        }
    }
}

/* @funcdef: JsonState_new */
static JsonState* JsonState_new(const char* jsonCode, int jsonLength, JsonAllocator allocator)
{
    JsonState* state = (JsonState*)JSON_ALLOC(&allocator, sizeof(JsonState));
    if (state)
    {
        state->reversed     = JSON_REVERSED;

		state->next         = NULL;

		state->line         = 1;
		state->column       = 1;
		state->cursor       = 0;
		state->buffer       = jsonCode;
		state->length       = jsonLength;

		state->errmsg       = NULL;
		state->errnum       = JsonError_None;

        state->allocator    = allocator;
    }
    return state;
}

/* @funcdef: JsonState_free */
static void JsonState_free(JsonState* state)
{
    if (state)
    {
        Json* root = &state->root;
        Json_releaseMemory(root, &state->allocator);

		JsonState* next = state->next;

		JSON_FREE(&state->allocator, state->errmsg);
		JSON_FREE(&state->allocator, state);

		JsonState_free(next);
    }
}

/* @funcdef: JsonState_isEOF */
static int JsonState_isEOF(JsonState* state)
{
    return state->cursor >= state->length || state->buffer[state->cursor] <= 0;
}

/* @funcdef: JsonState_peekChar */
static int JsonState_peekChar(JsonState* state)
{
    return state->buffer[state->cursor];
}

/* @funcdef: JsonState_NextChar */
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

        JsonTempArray(JsonObjectEntry, 32) values = JsonTempArray_init(NULL);
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
            JsonObjectEntry entry;
            entry.name  = name;
            entry.value = value;
            JsonTempArray_push(&values, entry, &state->allocator);
        }

        JsonState_skipSpace(state);
        JsonState_matchChar(state, JsonType_Object, '}');

        outValue->type   = JsonType_Object;
        outValue->length = JsonTempArray_getCount(&values);
        outValue->object = (JsonObjectEntry*)JsonTempArray_toBuffer(&values, &state->allocator);

        JsonTempArray_free(&values, &state->allocator);
    }
}
         
/* Internal parsing function
 */
static Json* JsonState_parseTopLevel(JsonState* state)
{
    if (!state)
    {
        return NULL;
    }

    if (JsonState_skipSpace(state) == '{')
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonState_parseObject(state, &state->root);

            JsonState_skipSpace(state);
            if (!JsonState_isEOF(state))
            {
                Json_panic(state, JsonType_Null, JsonError_WrongFormat, "JSON is not well-formed. JSON is start with <object>.");
            }

            return &state->root;
        }
        else
        {
            return NULL;
        }
    }
    else if (JsonState_skipSpace(state) == '[')
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonState_parseArray(state, &state->root);

            JsonState_skipSpace(state);
            if (!JsonState_isEOF(state))
            {
                Json_panic(state, JsonType_Null, JsonError_WrongFormat, "JSON is not well-formed. JSON is start with <array>.");
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
        Json_setError(state, JsonType_Null, JsonError_WrongFormat, "JSON must be starting with '{' or '[', first character is '%c'", JsonState_peekChar(state));
        return NULL;
    }
}

/* @funcdef: Json_parse */
Json* Json_parse(const char* json, int jsonLength)
{
    JsonAllocator allocator;
    allocator.data  = NULL;
    allocator.free  = Json_free;
    allocator.alloc = Json_alloc;

    return Json_parseEx(json, jsonLength, allocator);
}

/* @funcdef: Json_parseEx */
Json* Json_parseEx(const char* json, int jsonLength, JsonAllocator allocator)
{
    JsonState* state = JsonState_new(json, jsonLength, allocator);
    Json* value = JsonState_parseTopLevel(state);

    if (!value)
    {
        JsonState_free(state);
    }

    return value;
}

/* @funcdef: Json_release */
void Json_release(Json* rootValue)
{
    if (rootValue)
    {
        JsonState* state = JSON_SUPEROF(rootValue, JsonState, root);
        if (state->reversed == JSON_REVERSED)
        {
            JsonState_free(state);
        }
    }
}

/* @funcdef: Json_getError */
JsonError Json_getError(const Json* rootValue)
{
    if (rootValue)
    {
        JsonState* state = JSON_SUPEROF(rootValue, JsonState, root);
        if (state->reversed == JSON_REVERSED)
        {
            return state->errnum;
        }
    }

    return JsonError_InvalidValue;
}

/* @funcdef: Json_getErrorMessage */
const char* Json_getErrorMessage(const Json* rootValue)
{
    if (rootValue)
    {
        JsonState* state = JSON_SUPEROF(rootValue, JsonState, root);
        if (state->reversed == JSON_REVERSED)
        {
            return state->errmsg;
        }
    }

    return "JSON_ERROR_NO_VALUE";
}

/* @funcdef: Json_equals */
bool Json_equals(const Json* a, const Json* b)
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
                if (!Json_equals(&a->array[i], &b->array[i]))
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

                if (!Json_equals(&a->object[i].value, &b->object[i].value))
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

/* @funcdef: Json_find */
Json* Json_find(const Json* obj, const char* name)
{
    if (obj && obj->type == JsonType_Object)
    {
        int i, n;
        int len  = (int)strlen(name);
        for (i = 0, n = obj->length; i < n; i++)
        {
            JsonObjectEntry* entry = &obj->object[i];
            if (strncmp(name, entry->name, len) == 0)
            {
                return &entry->value;
            }
        }
    }

    return NULL;
}


#endif /* JSON_IMPL */

