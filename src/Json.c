#ifndef __JSON_H__
#include "Json.h"
#endif // __JSON_H__

// -------------------------------------------------------------------
// Dependencies
// -------------------------------------------------------------------

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

// -------------------------------------------------------------------
// Compiler options
// -------------------------------------------------------------------

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

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

#ifndef JSON_ASSERT
#define JSON_ASSERT(cond, msg, ...) assert((cond) && (msg))
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

static int32_t JsonAllocator_BlockSize(int32_t size)
{
	int32_t alignment = sizeof(Json);
	int32_t misalign = size & (alignment - 1);
	int32_t adjustment = (misalign != 0) * (alignment - misalign);
	return size + adjustment;
}

static bool JsonAllocator_Init(JsonAllocator* allocator, void* buffer, int32_t bufferSize)
{
    if (buffer && bufferSize > 0)
    {
		allocator->buffer = (uint8_t*)buffer;
		allocator->length = bufferSize;

		// Aligned buffer for cache-friendly processing
		const int32_t	alignment	= sizeof(Json);
		const int32_t	mask		= alignment - 1;

		const uintptr_t address		= (uintptr_t)buffer;
		const int32_t	misalign	= address & mask;
		const int32_t	adjustment	= (misalign != 0) * (alignment - misalign);

		buffer = (void*)(address + adjustment);
		bufferSize = JsonAllocator_BlockSize(bufferSize - adjustment);

        allocator->lowerMarker = (uint8_t*)buffer;
        allocator->upperMarker = (uint8_t*)buffer + bufferSize;

        return true;
    }

    return false;
}

static int32_t JsonAllocator_RemainSize(JsonAllocator* allocator)
{
	int32_t remain = (int32_t)(allocator->upperMarker - allocator->lowerMarker);
	return  remain;
}

static bool JsonAllocator_CanAlloc(JsonAllocator* allocator, int32_t size)
{
    int32_t remain = JsonAllocator_RemainSize(allocator);
    return  remain >= size;
}

static void JsonAllocator_FreeLower(JsonAllocator* allocator, void* buffer, int32_t size)
{
	const int32_t blockSize = JsonAllocator_BlockSize(size);
	void* lastBuffer = allocator->lowerMarker - blockSize;
	if (lastBuffer == buffer)
	{
		allocator->lowerMarker = lastBuffer;
	}
}

static void* JsonAllocator_AllocLower(JsonAllocator* allocator, void* oldBuffer, int32_t oldSize, int32_t newSize)
{
    JsonAllocator_FreeLower(allocator, oldBuffer, oldSize);

    if (newSize <= 0)
    {
        return NULL;
    }

	const int32_t blockSize = JsonAllocator_BlockSize(newSize);
    if (JsonAllocator_CanAlloc(allocator, blockSize))
    {
        void* result = allocator->lowerMarker;
        allocator->lowerMarker += blockSize;
        return result;
    }

    return NULL;
}

static void JsonAllocator_FreeUpper(JsonAllocator* allocator, void* buffer, int32_t size)
{
    void* lastBuffer = allocator->upperMarker;
    if (lastBuffer == buffer)
    {
		const int32_t blockSize = JsonAllocator_BlockSize(size);
        allocator->upperMarker += blockSize;
    }
}

static void* JsonAllocator_AllocUpper(JsonAllocator* allocator, void* oldBuffer, int32_t oldSize, int32_t newSize)
{
    JsonAllocator_FreeUpper(allocator, oldBuffer, oldSize);

    if (newSize <= 0)
    {
        return NULL;
    }

	const int32_t blockSize = JsonAllocator_BlockSize(newSize);
    if (JsonAllocator_CanAlloc(allocator, blockSize))
    {
        allocator->upperMarker -= blockSize;
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

#define JsonArray_GetHeader(a)              ((JsonArray*)(a) - 1)
#define JsonArray_Init()                    NULL
#define JsonArray_Free(a, alloc)            JsonAllocator_FreeUpper(alloc, a ? JsonArray_GetHeader(a) : NULL, JsonArray_GetSize(a) * sizeof((a)[0]))
#define JsonArray_GetSize(a)                ((a) ? JsonArray_GetHeader(a)->size  : 0)
#define JsonArray_GetCount(a)               ((a) ? JsonArray_GetHeader(a)->count : 0)
#define JsonArray_GetAllocMemory(a)         (sizeof(JsonArray) + JsonArray_GetSize(a) * sizeof(*(a)))
#define JsonArray_GetUsageMemory(a)         (sizeof(JsonArray) + JsonArray_GetCount(a) * sizeof(*(a)))
#define JsonArray_Push(a, v, alloc)         (JsonArray_EnsureSize(a, JsonArray_GetCount(a) + 1, alloc) ? ((void)((a)[JsonArray_GetHeader(a)->count++] = v), 1) : 0)
#define JsonArray_Pop(a, v, alloc)          ((a)[--JsonArray_GetHeader(a)->count]);
#define JsonArray_EnsureSize(a, n, alloc)   ((!(a) || JsonArray_GetSize(a) < (n)) ? (*((void**)&(a))=JsonArray_Grow(a, n + 1, sizeof((a)[0]), alloc)) != NULL : 1)
#define JsonArray_Clear(a)                  ((a) ? (void)(JsonArray_GetHeader(a)->count = 0) : (void)0)
#define JsonArray_Clone(a, alloc)           ((a) ? memcpy(JsonArray_Grow(0, JsonArray_GetCount(a), sizeof((a)[0]), alloc), JsonArray_GetHeader(a), JsonArray_GetUsageMemory(a)) : NULL)

JSON_INLINE void* JsonArray_Grow(void* array, int32_t reqsize, int32_t elemsize, JsonAllocator* allocator)
{
    assert(elemsize > 0);
    assert(allocator != NULL);

    JsonArray*  raw   = array ? JsonArray_GetHeader(array) : NULL;
    int32_t     size  = JsonArray_GetSize(array);
    int32_t     count = JsonArray_GetCount(array);

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
        newArray->size  = newSize;
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

#define JsonTempArray_Init(a)             { a, 0 }
#define JsonTempArray_Free(a, alloc)      JsonArray_Free((a)->array, alloc)
#define JsonTempArray_Push(a, v, alloc)   ((a)->count >= (int32_t)(sizeof((a)->buffer) / sizeof((a)->buffer[0])) ? JsonArray_Push((a)->array, v, alloc) : ((a)->buffer[(a)->count++] = v, 1))
#define JsonTempArray_GetCount(a)         ((a)->count + JsonArray_GetCount((a)->array))
#define JsonTempArray_ToBuffer(a, alloc)  JsonTempArray_ToBufferFunc((a)->buffer, (a)->count, (a)->array, (int)sizeof((a)->buffer[0]), alloc)

JSON_INLINE void* JsonTempArray_ToBufferFunc(void* buffer, int32_t count, void* dynamicBuffer, int32_t itemSize, JsonAllocator* allocator)
{
    int total = count + JsonArray_GetCount(dynamicBuffer);
    if (total > 0)
    {
        // Aligned total
        const int32_t mod = (total * itemSize) & (sizeof(Json) - 1);
        const int32_t size = total * itemSize + (mod != 0) * (sizeof(Json) - mod);

        void* array = (JsonArray*)JsonAllocator_AllocLower(allocator, NULL, 0, size);
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

typedef struct JsonParser JsonParser;
struct JsonParser
{
    JsonParseFlags      flags;

    int32_t             line;
    int32_t             column;
    int32_t             cursor;
    //JsonType            parsingType;
    
    int32_t             length;         /* Reference only */
    const char*         buffer;         /* Reference only */
    
    JsonError           errnum;
    char*               errmsg;
    jmp_buf             errjmp;

    JsonAllocator       allocator;      /* Runtime allocator */
};

static void JsonParser_SetErrorWithArgs(JsonParser* parser, JsonType type, JsonError code, const char* fmt, va_list valist)
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

    parser->errnum = code;
    if (parser->errmsg == NULL)
    {
        parser->errmsg = (char*)JsonAllocator_AllocUpper(&parser->allocator, NULL, 0, errmsg_size);
    }

    char final_format[1024];
    char templ_format[1024] = "%s\n\tAt line %d, column %d. Parsing token: <%s>.";

#if defined(_MSC_VER) && _MSC_VER >= 1200
    sprintf_s(final_format, sizeof(final_format), templ_format, fmt, parser->line, parser->column, type_name);
    sprintf_s(parser->errmsg, errmsg_size, final_format, valist);
#else
    sprintf(final_format, templ_format, fmt, parser->line, parser->column, type_name);
    sprintf(parser->errmsg, final_format, valist);
#endif
}

/* @funcdef: JsonParser_SetError */
static void JsonParser_SetError(JsonParser* parser, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    JsonParser_SetErrorWithArgs(parser, type, code, fmt, varg);
    va_end(varg);
}

/* funcdef: JsonParser_Panic */
static void JsonParser_Panic(JsonParser* parser, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    JsonParser_SetErrorWithArgs(parser, type, code, fmt, varg);
    va_end(varg);

    longjmp(parser->errjmp, code);
}

/* @funcdef: JsonParser_Init */
static bool JsonParser_Init(JsonParser* parser, const char* jsonCode, int32_t jsonLength, JsonAllocator allocator, JsonParseFlags flags)
{
    JSON_ASSERT(parser, "parser mustnot be null");

    parser->flags        = flags;

	parser->line         = 1;
	parser->column       = 1;
	parser->cursor       = 0;
	parser->buffer       = jsonCode;
	parser->length       = jsonLength;

	parser->errmsg       = "Success!";
	parser->errnum       = JsonError_None;

    parser->allocator    = allocator;

    return true;
}

/* @funcdef: JsonParser_IsAtEnd */
static int JsonParser_IsAtEnd(const JsonParser* parser)
{
    return parser->cursor >= parser->length || parser->buffer[parser->cursor] <= 0;
}

/* @funcdef: JsonParser_PeekChar */
static int JsonParser_PeekChar(const JsonParser* parser)
{
    return parser->buffer[parser->cursor];
}

/* @funcdef: JsonParser_NextChar */
static int JsonParser_NextChar(JsonParser* parser)
{
    if (JsonParser_IsAtEnd(parser))
    {
		return -1;
    }
    else
    {
		int c = parser->buffer[++parser->cursor];

		if (c == '\n')
		{
			parser->line++;
			parser->column = 1;
		}
		else
		{
			parser->column = parser->column + 1;
		}
		
		return c;
    }
}

/* @funcdef: JsonParser_NextLine */
static int JsonParser_NextLine(JsonParser* parser)
{
    if (JsonParser_IsAtEnd(parser))
    {
        return -1;
    }
    else
    {
        int c = parser->buffer[parser->cursor];
        while (c != '\n')
        {
            c = parser->buffer[++parser->cursor];
        }

        c = parser->buffer[++parser->cursor];
        parser->line++;
        parser->column = 1;
        return c;
    }
}

/* @funcdef: JsonParser_SkipSpace */
static int JsonParser_SkipSpace(JsonParser* parser)
{
    int c = JsonParser_PeekChar(parser);
    while (c > 0 && isspace(c))
    {
		c = JsonParser_NextChar(parser);
    }
    return c;
}

/* @funcdef: JsonParser_MatchChar */
static int JsonParser_MatchChar(JsonParser* parser, JsonType type, int c)
{
    if (JsonParser_PeekChar(parser) == c)
    {
		return JsonParser_NextChar(parser);
    }
    else
    {
        JsonParser_Panic(parser, type, JsonError_UnmatchToken, "Expected '%c'", (char)c);
		return -1;
    }
}

/* @funcdef: JsonParser_SkipComments */
static int JsonParser_SkipComments(JsonParser* parser)
{
    while (true)
    {
        int c = JsonParser_NextChar(parser);
        if (c == '/')
        {
            c = JsonParser_NextChar(parser);
            if (c == '/')
            {
                JsonParser_NextLine(parser);
            }    
            else if (c == '*')
            {
                int c0 = JsonParser_NextChar(parser);
                int c1 = JsonParser_NextChar(parser);
                while (c0 != '*' || c1 != '/')
                {
                    c0 = c1;
                    c1 = JsonParser_NextChar(parser);
                }
            }
            else
            {
                JsonParser_Panic(parser, JsonType_Null, JsonError_UnexpectedToken, "Unexpected token '%c'", c);
            }
        }
        else
        {
            break;
        }
    }
    
    return JsonParser_PeekChar(parser);
}

/* All parse functions declaration */

static void JsonParser_ParseArray(JsonParser* parser, Json* outValue);
static void JsonParser_ParseSingle(JsonParser* parser, Json* outValue);
static void JsonParser_ParseObject(JsonParser* parser, Json* outValue);
static void JsonParser_ParseNumber(JsonParser* parser, Json* outValue);
static void JsonParser_ParseString(JsonParser* parser, Json* outValue);

/* @funcdef: JsonParser_ParseNumber */
static void JsonParser_ParseNumber(JsonParser* parser, Json* outValue)
{
    int c = JsonParser_SkipSpace(parser);
    if (c > 0)
    {
		int sign = 1;
		
		if (c == '+')
		{
			c = JsonParser_NextChar(parser);
			JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "JSON does not support number start with '+'");
		}
		else if (c == '-')
		{
			sign = -1;
			c = JsonParser_NextChar(parser);
		}
		else if (c == '0')
		{
			c = JsonParser_NextChar(parser);
			if (!isspace(c) && !ispunct(c))
			{
				JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "JSON does not support number start with '0' (only standalone '0' is accepted)");
			}
		}
		else if (!isdigit(c))
		{
			JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "Unexpected '%c'", c);
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
                    JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "Too many 'e' are presented in a <number>");
                }
                else if (dot && numpow == 1)
                {
                    JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken,
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
                    JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "Cannot has '.' after 'e' is presented in a <number>");
                }
				else if (dot)
				{
					JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "Too many '.' are presented in a <number>");
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
                    JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "'%c' is presented after digits are presented of exponent part", c);
                }
                else if (expsgn)
                {
                    JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "Too many signed characters are presented after 'e'");
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

			c = JsonParser_NextChar(parser);
		}

        if (exp && !expchk)
        {
            JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "'e' is presented in number token, but require a digit after 'e' ('%c')", (char)c);
        }
		if (dot && numpow == 1)
		{
			JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "'.' is presented in number token, but require a digit after '.' ('%c')", (char)c);
		}
		else
		{
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
                    number /= tmp;
                }
                else
                {
                    number *= tmp;
                }
            }

            Json value;
            value.type = JsonType_Number;
            value.length = 0;
            value.number = sign * number;

            *outValue = value;
		}
    }
    else
    {
        JsonParser_Panic(parser, JsonType_Number, JsonError_UnexpectedToken, "Reached the end of json!");
    }
}

/* @funcdef: JsonParser_ParseArray */
static void JsonParser_ParseArray(JsonParser* parser, Json* outValue)
{
    if (JsonParser_SkipSpace(parser) > 0)
    {
	    JsonParser_MatchChar(parser, JsonType_Array, '[');

        JsonTempArray(Json, 64) values = JsonTempArray_Init(NULL);
	    while (JsonParser_SkipSpace(parser) > 0 && JsonParser_PeekChar(parser) != ']')
	    {
	        if (values.count > 0)
	        {
                JsonParser_MatchChar(parser, JsonType_Array, ',');
	        }
	    
            Json value;
            JsonParser_ParseSingle(parser, &value);

            JsonTempArray_Push(&values, value, &parser->allocator);
	    }

	    JsonParser_SkipSpace(parser);
	    JsonParser_MatchChar(parser, JsonType_Array, ']');

        outValue->type   = JsonType_Array;
        outValue->length = JsonTempArray_GetCount(&values);
        outValue->array  = (Json*)JsonTempArray_ToBuffer(&values, &parser->allocator);

        JsonTempArray_Free(&values, &parser->allocator);
    }
}

/* JsonState_ParseSingle */
static void JsonParser_ParseSingle(JsonParser* parser, Json* outValue)
{
    if (JsonParser_SkipSpace(parser) > 0)
    {
	    int c = JsonParser_PeekChar(parser);
	
	    switch (c)
	    {
	    case '[':
	        JsonParser_ParseArray(parser, outValue);
            break;
	    
	    case '{':
	        JsonParser_ParseObject(parser, outValue);
            break;
	    
	    case '"':
	        JsonParser_ParseString(parser, outValue);
            break;

	    case '+': case '-': case '0': 
        case '1': case '2': case '3': 
        case '4': case '5': case '6': 
        case '7': case '8': case '9':
	        JsonParser_ParseNumber(parser, outValue);
            break;


        case '/':
            if (parser->flags & JsonParseFlags_SupportComment)
            {
                JsonParser_SkipComments(parser);
                JsonParser_ParseSingle(parser, outValue);
            }
            else
            {
                JsonParser_Panic(parser, JsonType_String, JsonError_UnknownToken, "Unknown token '%c'", c);
            }
            break;
	    
        default:
	    {
	        int length = 0;
	        while (c > 0 && isalpha(c))
	        {
                length++;
                c = JsonParser_NextChar(parser);
	        }

	        const char* token = parser->buffer + parser->cursor - length;
            if (length == 4 && strncmp(token, "null", length) == 0)
            {
                *outValue = JSON_NULL;
            }
            else if (length == 4 && strncmp(token, "true", length) == 0)
	        {
                *outValue = JSON_TRUE;
	        }
	        else if (length == 5 && strncmp(token, "false", length) == 0)
	        {
                *outValue = JSON_FALSE;
	        }
	        else
	        {
                char tmp[256];
                tmp[length] = 0;
                while (length--)
                {
                    tmp[length] = token[length]; 
                }

                JsonParser_Panic(parser, JsonType_Null, JsonError_UnexpectedToken, "Unexpected token '%s'", tmp);
	        }
	    } break;
	    /* END OF SWITCH STATEMENT */
        }
    }
}

static char* JsonParser_ParseStringNoToken(JsonParser* parser, int* outLength)
{
    JsonParser_MatchChar(parser, JsonType_String, '"');

    int i;
    int c0, c1;

    JsonTempArray(char, 2048) buffer = JsonTempArray_Init(NULL);
    while (!JsonParser_IsAtEnd(parser) && (c0 = JsonParser_PeekChar(parser)) != '"')
    {
        if (c0 == '\\')
        {
            c0 = JsonParser_NextChar(parser);
            switch (c0)
            {
            case 'n':
                JsonTempArray_Push(&buffer, '\n', &parser->allocator);
                break;

            case 't':
                JsonTempArray_Push(&buffer, '\t', &parser->allocator);
                break;

            case 'r':
                JsonTempArray_Push(&buffer, '\r', &parser->allocator);
                break;

            case 'b':
                JsonTempArray_Push(&buffer, '\b', &parser->allocator);
                break;

            case '\\':
                JsonTempArray_Push(&buffer, '\\', &parser->allocator);
                break;

            case '"':
                JsonTempArray_Push(&buffer, '\"', &parser->allocator);
                break;

            case 'u':
                c1 = 0;
                for (i = 0; i < 4; i++)
                {
                    if (isxdigit((c0 = JsonParser_NextChar(parser))))
                    {
                        c1 = c1 * 10 + (isdigit(c0) ? c0 - '0' : c0 < 'a' ? c0 - 'A' : c0 - 'a');
                    }
                    else
                    {
                        JsonParser_Panic(parser, JsonType_String, JsonError_UnknownToken, "Expected hexa character in unicode character");
                    }
                }

                if (c1 <= 0x7F)
                {
                    JsonTempArray_Push(&buffer, (char)c1, &parser->allocator);
                }
                else if (c1 <= 0x7FF)
                {
                    char c2 = (char)(0xC0 | (c1 >> 6));            /* 110xxxxx */
                    char c3 = (char)(0x80 | (c1 & 0x3F));          /* 10xxxxxx */
                    JsonTempArray_Push(&buffer, c2, &parser->allocator);
                    JsonTempArray_Push(&buffer, c3, &parser->allocator);
                }
                else if (c1 <= 0xFFFF)
                {
                    char c2 = (char)(0xE0 | (c1 >> 12));           /* 1110xxxx */
                    char c3 = (char)(0x80 | ((c1 >> 6) & 0x3F));   /* 10xxxxxx */
                    char c4 = (char)(0x80 | (c1 & 0x3F));          /* 10xxxxxx */
                    JsonTempArray_Push(&buffer, c2, &parser->allocator);
                    JsonTempArray_Push(&buffer, c3, &parser->allocator);
                    JsonTempArray_Push(&buffer, c4, &parser->allocator);
                }
                else if (c1 <= 0x10FFFF)
                {
                    char c2 = 0xF0 | (c1 >> 18);           /* 11110xxx */
                    char c3 = 0x80 | ((c1 >> 12) & 0x3F);  /* 10xxxxxx */
                    char c4 = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                    char c5 = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    JsonTempArray_Push(&buffer, c2, &parser->allocator);
                    JsonTempArray_Push(&buffer, c3, &parser->allocator);
                    JsonTempArray_Push(&buffer, c4, &parser->allocator);
                    JsonTempArray_Push(&buffer, c5, &parser->allocator);
                }
                break;

            default:
                JsonParser_Panic(parser, JsonType_String, JsonError_UnknownToken, "Unknown escape character");
                break;
            }
        }
        else
        {
            switch (c0)
            {
            case '\r':
            case '\n':
                JsonParser_Panic(parser, JsonType_String, JsonError_UnexpectedToken, "Unexpected newline characters '%c'", c0);
                break;

            default:
                JsonTempArray_Push(&buffer, (char)c0, &parser->allocator);
                break;
            }
        }

        JsonParser_NextChar(parser);
    }

    JsonParser_MatchChar(parser, JsonType_String, '"');
    if (buffer.count > 0)
    {
        if (outLength) *outLength = JsonTempArray_GetCount(&buffer);
        JsonTempArray_Push(&buffer, 0, &parser->allocator);

        char* string = (char*)JsonTempArray_ToBuffer(&buffer, &parser->allocator);
        JsonTempArray_Free(&buffer, &parser->allocator);

        return string;
    }
    else
    {
        if (outLength) *outLength = 0;
        return NULL;
    }
}

/* @funcdef: JsonParser_ParseString */
static void JsonParser_ParseString(JsonParser* parser, Json* outValue)
{
    if (JsonParser_SkipSpace(parser) > 0)
    {
        int length;
        const char* string = JsonParser_ParseStringNoToken(parser, &length);

        outValue->type   = JsonType_String;
        outValue->string = string;
        outValue->length = length;
    }
}

/* @funcdef: JsonParser_ParseObject */
static void JsonParser_ParseObject(JsonParser* parser, Json* outValue)
{
    if (JsonParser_SkipSpace(parser) > 0)
    {
        JsonParser_MatchChar(parser, JsonType_Object, '{');

        JsonTempArray(JsonObjectMember, 32) values = JsonTempArray_Init(NULL);
        while (JsonParser_SkipSpace(parser) > 0 && JsonParser_PeekChar(parser) != '}')
        {
            if (values.count > 0)
            {
                JsonParser_MatchChar(parser, JsonType_Object, ',');
            }

            if (JsonParser_SkipSpace(parser) != '"')
            {
                JsonParser_Panic(parser, JsonType_Object, JsonError_UnexpectedToken, "Expected <string> for <member-key> of <object>");
            }

            const char* name = JsonParser_ParseStringNoToken(parser, 0);

            JsonParser_SkipSpace(parser);
            JsonParser_MatchChar(parser, JsonType_Object, ':');

            Json value;
            JsonParser_ParseSingle(parser, &value);

            /* Well done */
            JsonObjectMember member;
            member.name  = name;
            member.value = value;
            JsonTempArray_Push(&values, member, &parser->allocator);
        }

        JsonParser_SkipSpace(parser);
        JsonParser_MatchChar(parser, JsonType_Object, '}');

        outValue->type   = JsonType_Object;
        outValue->length = JsonTempArray_GetCount(&values);
        outValue->object = (JsonObjectMember*)JsonTempArray_ToBuffer(&values, &parser->allocator);

        JsonTempArray_Free(&values, &parser->allocator);
    }
}
         
/* Internal parsing function
 */
static Json* JsonState_ParseTopLevel(JsonParser* parser)
{
    JSON_ASSERT(parser, "parser mustnot be null");

    Json* value = (Json*)JsonAllocator_AllocLower(&parser->allocator, NULL, 0, sizeof(Json));
    value->type = JsonType_Null;

    // Skip meta comment in header of the file
    if (parser->flags & JsonParseFlags_SupportComment)
    {
        JsonParser_SkipSpace(parser);
        JsonParser_SkipComments(parser);
    }

    // Use setjmp for quick exit when parse error happend
    if (setjmp(parser->errjmp) == 0)
    {
        // Just parse value from the top level
        if (parser->flags & JsonParseFlags_NoStrictTopLevel)
        {
            JsonParser_ParseSingle(parser, value);
        }
        // Make sure the toplevel is JsonType_Object
        else if (JsonParser_SkipSpace(parser) == '{')
        {
            JsonParser_ParseObject(parser, value);

            JsonParser_SkipSpace(parser);
            if (!JsonParser_IsAtEnd(parser))
            {
                JsonParser_Panic(parser, JsonType_Null, JsonError_WrongFormat, "JSON is not well-formed. JSON is start with <object>.");
            }
        }
        // Make sure the toplevel is JsonType_Array
        else if (JsonParser_SkipSpace(parser) == '[')
        {
            JsonParser_ParseArray(parser, value);

            JsonParser_SkipSpace(parser);
            if (!JsonParser_IsAtEnd(parser))
            {
                JsonParser_Panic(parser, JsonType_Null, JsonError_WrongFormat, "JSON is not well-formed. JSON is start with <array>.");
            }
        }
        else
        {
            JsonParser_SetError(parser, JsonType_Null, JsonError_WrongFormat, "JSON must be starting with '{' or '[', first character is '%c'", JsonParser_PeekChar(parser));
        }
    }

    return value;
}

/* @funcdef: JsonParse */
JsonResult JsonParse(const char* jsonCode, int32_t jsonCodeLength, JsonParseFlags flags, void* buffer, int32_t bufferSize, Json* outValue)
{
    JSON_ASSERT(outValue, "outValue mustnot be null");

    // Validate json input
    if (!jsonCode || jsonCodeLength <= 0)
    {
        const JsonResult result = { JsonError_WrongFormat, "Json code is empty", 0 };
        return result;
    }
    
    // Create new allocator
    JsonAllocator allocator;
    if (!JsonAllocator_Init(&allocator, buffer, bufferSize))
    {
        const JsonResult result = { JsonError_OutOfMemory, "Buffer is too small", 0 };
        return result;
    }

    // Create parser
    JsonParser parser;
    if (!JsonParser_Init(&parser, jsonCode, jsonCodeLength, allocator, flags))
    {
        const JsonResult result = { JsonError_InternalFatal, "Wrong behaviour when create new parser", 0 };
        return result;
    }
    
    // Parse the top level
    Json* value = JsonState_ParseTopLevel(&parser);
    JSON_ASSERT(value, "value mustnot be null");
    *outValue = *value;

    // Done!
	JsonResult result;
	result.error = parser.errnum;
	result.message = parser.errmsg;
	result.memoryUsage = (int32_t)(parser.allocator.lowerMarker - (uint8_t*)buffer);
    return result;
}

/* @funcdef: JsonEquals */
bool JsonEquals(const Json a, const Json b)
{
    if (a.type != b.type)
    {
        return false;
    }

    switch (a.type)
    {
    case JsonType_Null:
        return true;

    case JsonType_Number:
        return a.number == b.number;

    case JsonType_Boolean:
        return a.boolean == b.boolean;

    case JsonType_Array: {
        int32_t n;
        if ((n = a.length) == a.length)
        {
            for (int32_t i = 0; i < n; i++)
            {
                if (!JsonEquals(a.array[i], b.array[i]))
                {
                    return false;
                }
            }
        }

        return true;
    }

    case JsonType_Object: {
        int32_t n;
        if ((n = a.length) == b.length)
        {
            for (int32_t i = 0; i < n; i++)
            {
                if (strncmp(a.object[i].name, b.object[i].name, n) != 0)
                {
                    return false;
                }

                if (!JsonEquals(a.object[i].value, b.object[i].value))
                {
                    return false;
                }
            }
        }

        return true;
    }

    case JsonType_String:
        return a.length == b.length && strncmp(a.string, b.string, a.length) == 0;

    default:
        JSON_ASSERT(false, "invalid json type");
        break;
    }

    return false;
}

/* @funcdef: JsonFind */
bool JsonFind(const Json parent, const char* name, Json* outResult)
{
    JSON_ASSERT(outResult, "outResult mustnot be null");
    JSON_ASSERT(JsonValidType(parent), "invalid json type");
    JSON_ASSERT(name, "Attempt using nullptr as string");

    if (parent.type == JsonType_Object)
    {
        const int32_t nameLength = (int32_t)strlen(name);
        for (int32_t i = 0, n = parent.length; i < n; i++)
        {
            const JsonObjectMember* member = &parent.object[i];
            JSON_ASSERT(member && JsonValidType(member->value), "invalid json type");

            if (strncmp(name, member->name, nameLength) == 0)
            {
                *outResult = member->value;
                return true;
            }
        }
    }

    *outResult = JSON_NULL;
    return false;
}

/* @funcdef: JsonFindWithType */
JsonError JsonFindWithType(const Json parent, const char* name, JsonType type, Json* outResult)
{
    JSON_ASSERT(outResult, "outResult mustnot be null");
    JSON_ASSERT(JsonValidType(parent), "invalid json type");
    JSON_ASSERT(name, "Attempt using nullptr as string");

    if (parent.type == JsonType_Object)
    {
        const int32_t nameLength = (int32_t)strlen(name);
        for (int32_t i = 0, n = parent.length; i < n; i++)
        {
            const JsonObjectMember* member = &parent.object[i];
            JSON_ASSERT(member && JsonValidType(member->value), "invalid json type");

            if (strncmp(name, member->name, nameLength) == 0)
            {
                *outResult = member->value;
                return member->value.type == type ? JsonError_None : JsonError_WrongType;
            }
        }


        *outResult = JSON_NULL;
        return JsonError_MissingField;
    }

    *outResult = JSON_NULL;
    return JsonError_WrongType;
}

// -------------------------------------------------------------------
// Turn-off compiler options, because of single-header library
// -------------------------------------------------------------------

#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wmissing-field-initializers"
#endif

//! LEAVE AN EMPTY LINE HERE, REQUIRE BY GCC/G++
