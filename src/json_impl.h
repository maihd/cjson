#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

#ifndef JSON_VALUE_BUCKETS
#define JSON_VALUE_BUCKETS 4096
#endif

#ifndef JSON_STRING_BUCKETS
#define JSON_STRING_BUCKETS 4096
#endif

#ifndef JSON_VALUE_POOL_COUNT
#define JSON_VALUE_POOL_COUNT (4096/sizeof(JsonValue))
#endif

typedef struct JsonPool   JsonPool;
typedef struct JsonBucket JsonBucket;

struct JsonPool
{
    JsonPool* prev;
    JsonPool* next;

    void** head;
};

struct JsonBucket
{
    JsonBucket* prev;
    JsonBucket* next;

    size_t size;
    size_t count;
    size_t capacity;
};

struct JsonParser
{
    JsonParser* next;
    JsonPool*   valuePool;

    JsonBucket* stringBucket;
    JsonBucket* arrayValuesBucket;
    JsonBucket* objectValuesBucket;
    
    size_t   line;
    size_t   column;
    size_t   cursor;
    JsonType parsingType;
    
    size_t      length;
    const char* buffer;
    
    JsonError   errnum;
    char*       errmsg;
    jmp_buf     errjmp;

    JsonAllocator allocator; /* Runtime allocator */
};

static JsonParser* rootParser = NULL;
const struct JsonValue JSON_VALUE_NONE;

/* @funcdef: Json_Alloc */
static void* Json_Alloc(void* data, size_t size)
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

static void Json_SetErrorArgs(JsonParser* parser, JsonType type, JsonError code, const char* fmt, va_list valist)
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

    parser->errnum = code;
    if (parser->errmsg == NULL)
    {
        parser->errmsg = (char*)parser->allocator.alloc(parser->allocator.data, errmsg_size);
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

/* @funcdef: Json_SetError */
static void Json_SetError(JsonParser* parser, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_SetErrorArgs(parser, type, code, fmt, varg);
    va_end(varg);
}

/* funcdef: Json_Panic */
static void Json_Panic(JsonParser* parser, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    Json_SetErrorArgs(parser, type, code, fmt, varg);
    va_end(varg);

    longjmp(parser->errjmp, code);
}

/* funcdef: JsonPool_Make */
static JsonPool* JsonPool_Make(JsonParser* parser, JsonPool* prev, int count, int size)
{
    if (count <= 0 || size <= 0)
    {
		return NULL;
    }

    int pool_size = count * (sizeof(void*) + size);
    JsonPool* pool = (JsonPool*)parser->allocator.alloc(parser->allocator.data, sizeof(JsonPool) + pool_size);
    if (pool)
    {
        if (prev)
        {
            prev->next = pool;
        }

		pool->prev = prev;
		pool->next = NULL;
		pool->head = (void**)((char*)pool + sizeof(JsonPool));
		
		int i;
		void** node = pool->head;
		for (i = 0; i < count - 1; i++)
		{
			node = (void**)(*node = (char*)node + sizeof(void*) + size);
		}
		*node = NULL;
    }
    
    return pool;
}

/* funcdef: JsonPool_Free */
static void JsonPool_Free(JsonParser* parser, JsonPool* pool)
{
    if (pool)
    {
		JsonPool_Free(parser, pool->prev);
		parser->allocator.free(parser->allocator.data, pool);
    }
}

/* funcdef: JsonPool_Acquire */
static void* JsonPool_Acquire(JsonPool* pool)
{
    if (pool->head)
    {
		void** head = pool->head;
		void** next = (void**)(*head);
		
		pool->head = next;
		return (void*)((char*)head + sizeof(void*));
    }
    else
    {
		return NULL;
    }
}

#if 0 && UNUSED
/* funcdef: JsonPool_Release */
static void JsonPool_Release(JsonPool* pool, void* ptr)
{
    if (ptr)
    {
		void** node = (void**)((char*)ptr - sizeof(void*));
		*node = pool->head;
		pool->head = node;
    }
}
#endif

/* funcdef: JsonBucket_Make */
static JsonBucket* JsonBucket_Make(JsonParser* parser, JsonBucket* prev, size_t count, size_t size)
{
    if (count <= 0 || size <= 0)
    {
        return NULL;
    }

    JsonBucket* bucket = (JsonBucket*)parser->allocator.alloc(parser->allocator.data, sizeof(JsonBucket) + count * size);
    if (bucket)
    {
        if (prev)
        {
            prev->next = bucket;
        }

        bucket->prev     = prev;
        bucket->next     = NULL;
        bucket->size     = size;
        bucket->count    = 0;
        bucket->capacity = count;
    }
    return bucket;
}

/* funcdef: JsonBucket_Free */
static void JsonBucket_Free(JsonParser* parser, JsonBucket* bucket)
{
    if (bucket)
    {
        /* Free next */
        JsonBucket_Free(parser, bucket->next);

        /* Free now */ 
        parser->allocator.free(parser->allocator.data, bucket);
    }
}

/* funcdef: JsonBucket_Acquire */
static void* JsonBucket_Acquire(JsonBucket* bucket, int count)
{
    if (!bucket || count <= 0)
    {
        return NULL;
    }
    else if (bucket->count + count <= bucket->capacity)
    {
        void* res = (char*)bucket + sizeof(JsonBucket) + bucket->size * bucket->count;
        bucket->count += count;
        return res;
    }
    else
    {
        return NULL;
    }
}

/* funcdef: JsonBucket_Resize */
static void* JsonBucket_Resize(JsonBucket* bucket, void* ptr, int oldCount, int newCount)
{
    if (!bucket || newCount <= 0)
    {
        return NULL;
    }

    if (!ptr)
    {
        return JsonBucket_Acquire(bucket, newCount);
    }

    char* begin = (char*)bucket + sizeof(JsonBucket);
    char* end   = begin + bucket->size * bucket->count;
    if ((char*)ptr + bucket->size * oldCount == end && bucket->count + (newCount - oldCount) <= bucket->capacity)
    {
        bucket->count += (newCount - oldCount);
        return ptr;
    }
    else
    {
        return NULL;
    }
}

/* @funcdef: JsonValue_Make */
static JsonValue* JsonValue_Make(JsonParser* parser, JsonType type)
{
    if (!parser->valuePool || !parser->valuePool->head)
    {
        if (parser->valuePool && parser->valuePool->prev)
        {
            parser->valuePool = parser->valuePool->prev;
        }
        else
        {
            parser->valuePool = JsonPool_Make(parser, parser->valuePool, JSON_VALUE_POOL_COUNT, sizeof(JsonValue));
        }

		if (!parser->valuePool)
		{
			Json_Panic(parser, type, JSON_ERROR_MEMORY, "Out of memory");
		}
    }
    
    JsonValue* value = (JsonValue*)JsonPool_Acquire(parser->valuePool);
    if (value)
    {
		memset(value, 0, sizeof(JsonValue));
		value->type    = type;
		value->boolean = JSON_FALSE;
    }
    else
    {
		Json_Panic(parser, type, JSON_ERROR_MEMORY, "Out of memory");
    }
    return value;
}

/* @funcdef: JsonParser_Make */
static JsonParser* JsonParser_Make(const char* json, const JsonAllocator* allocator)
{
    JsonParser* parser = (JsonParser*)allocator->alloc(allocator->data, sizeof(JsonParser));
    if (parser)
    {
		parser->next   = NULL;
		
		parser->line   = 1;
		parser->column = 1;
		parser->cursor = 0;
		parser->buffer = json;
		parser->length = strlen(json);

		parser->errmsg = NULL;
		parser->errnum = JSON_ERROR_NONE;

		parser->valuePool            = NULL;
        parser->stringBucket         = NULL;
        //parser->arrayValuesBucket    = NULL;
        //parser->objectValuesBucket   = NULL;

        parser->allocator = *allocator;
    }
    return parser;
}

/* @funcdef: JsonParser_Reuse */
static JsonParser* JsonParser_Reuse(JsonParser* parser, const char* json, const JsonAllocator* allocator)
{
    if (parser)
    {
        if (parser == rootParser)
        {
            rootParser = parser->next;
        }
        else
        {
            JsonParser* list = rootParser;
            while (list)
            {
                if (list->next == parser)
                {
                    list->next = parser->next;
                }
            }

		    parser->next = NULL;
        }

		parser->line   = 1;
		parser->column = 1;
		parser->cursor = 0;
		parser->buffer = json;
		parser->errnum = JSON_ERROR_NONE;

        if (parser->allocator.data != allocator->data ||
            parser->allocator.free != allocator->free ||
            parser->allocator.alloc != allocator->alloc)
        {
            JsonPool_Free(parser, parser->valuePool);

            JsonBucket_Free(parser, parser->stringBucket);
            //JsonBucket_Free(parser, parser->arrayValuesBucket);
            //JsonBucket_Free(parser, parser->objectValuesBucket);

		    parser->valuePool            = NULL;
            parser->stringBucket         = NULL;
            //parser->arrayValuesBucket    = NULL;
            //parser->objectValuesBucket   = NULL;

            parser->allocator.free(parser->allocator.data, parser->errmsg); 
            parser->errmsg = NULL;
        }
        else
        {
            if (parser->errmsg) parser->errmsg[0] = 0;

            while (parser->valuePool)
            {
                parser->valuePool->head = (void**)(parser->valuePool + 1);
                if (parser->valuePool->prev)
                {
                    break;
                }
                else
                {
                    parser->valuePool = parser->valuePool->prev;
                }
            }

            while (parser->stringBucket)
            {
                parser->stringBucket->count = 0;
                if (parser->stringBucket->prev)
                {
                    parser->stringBucket = parser->stringBucket->prev;
                }
                else
                {
                    break;
                }
            }

            while (parser->arrayValuesBucket)
            {
                parser->arrayValuesBucket->count = 0;
                if (parser->arrayValuesBucket->prev)
                {
                    parser->arrayValuesBucket = parser->arrayValuesBucket->prev;
                }
                else
                {
                    break;
                }
            }
            
            while (parser->objectValuesBucket)
            {
                parser->objectValuesBucket->count = 0;
                if (parser->objectValuesBucket->prev)
                {
                    parser->objectValuesBucket = parser->objectValuesBucket->prev;
                }
                else
                {
                    break;
                }
            }
        }
    }
    return parser;
}

/* @funcdef: JsonParser_Free */
static void JsonParser_Free(JsonParser* parser)
{
    if (parser)
    {
		JsonParser* next = parser->next;

        //JsonBucket_Free(parser, parser->objectValuesBucket);
        //JsonBucket_Free(parser, parser->arrayValuesBucket);
        JsonBucket_Free(parser, parser->stringBucket);
		JsonPool_Free(parser, parser->valuePool);

		parser->allocator.free(parser->allocator.data, parser->errmsg);
		parser->allocator.free(parser->allocator.data, parser);

		JsonParser_Free(next);
    }
}

/* @funcdef: JsonParser_IsEOF */
static int JsonParser_IsEOF(JsonParser* parser)
{
    return parser->cursor >= parser->length || parser->buffer[parser->cursor] <= 0;
}

/* @funcdef: JsonParser_PeekChar */
static int JsonParser_PeekChar(JsonParser* parser)
{
    return parser->buffer[parser->cursor];
}

/* @funcdef: JsonParser_NextChar */
static int JsonParser_NextChar(JsonParser* parser)
{
    if (JsonParser_IsEOF(parser))
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

#if 0 /* UNUSED */
/* @funcdef: JsonValue_Make */
static int next_line(JsonParser* parser)
{
    int c = JsonParser_PeekChar(parser);
    while (c > 0 && c != '\n')
    {
		c = JsonParser_NextChar(parser);
    }
    return JsonParser_NextChar(parser);
}
#endif

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
        Json_Panic(parser, type, JSON_ERROR_UNMATCH, "Expected '%c'", c);
		return -1;
    }
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
            k  = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h  = (h << 13) | (h >> 19);
            h  = (h * 5) + 0xe6546b64;
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
            k  |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k  = (k << 15) | (k >> 17);
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

/* All parse functions declaration */

static JsonValue* JsonParser_ParseArray(JsonParser* parser, JsonValue* value);
static JsonValue* JsonParser_ParseSingle(JsonParser* parser, JsonValue* value);
static JsonValue* JsonParser_ParseObject(JsonParser* parser, JsonValue* value);
static JsonValue* JsonParser_ParseNumber(JsonParser* parser, JsonValue* value);
static JsonValue* JsonParser_ParseString(JsonParser* parser, JsonValue* value);

/* @funcdef: JsonParser_ParseNumber */
static JsonValue* JsonParser_ParseNumber(JsonParser* parser, JsonValue* value)
{
    if (JsonParser_SkipSpace(parser) < 0)
    {
		return NULL;
    }
    else
    {
		int c = JsonParser_PeekChar(parser);
		int sign = 1;
		
		if (c == '+')
		{
			c = JsonParser_NextChar(parser);
			Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "JSON does not support number start with '+'");
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
				Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "JSON does not support number start with '0' (only standalone '0' is accepted)");
			}
		}
		else if (!isdigit(c))
		{
			Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Unexpected '%c'", c);
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
                    Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many 'e' are presented in a <number>");
                }
                else if (dot && numpow == 1)
                {
                    Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED,
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
                    Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Cannot has '.' after 'e' is presented in a <number>");
                }
				else if (dot)
				{
					Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many '.' are presented in a <number>");
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
                    Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'%c' is presented after digits are presented of exponent part", c);
                }
                else if (expsgn)
                {
                    Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many signed characters are presented after 'e'");
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
            Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'e' is presented in number token, but require a digit after 'e' ('%c')", c);
        }
		if (dot && numpow == 1)
		{
			Json_Panic(parser, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'.' is presented in number token, but require a digit after '.' ('%c')", c);
			return NULL;
		}
		else
		{
			if (!value)
            {
                value = JsonValue_Make(parser, JSON_NUMBER);
            }
            else
            {
                value->type = JSON_NUMBER;
            }

			value->number = sign * number;

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
                    value->number /= tmp;
                }
                else
                {
                    value->number *= tmp;
                }
            }

			return value;
		}
    }
}

/* @funcdef: JsonParser_ParseArray */
static JsonValue* JsonParser_ParseArray(JsonParser* parser, JsonValue* root)
{
    if (JsonParser_SkipSpace(parser) < 0)
    {
        return NULL;
    }
    else
    {
	    JsonParser_MatchChar(parser, JSON_ARRAY, '[');
	
	    if (!root)
        {
            root = JsonValue_Make(parser, JSON_ARRAY);
        }
        else
        {
            root->type = JSON_ARRAY;
        }

	    int         length = 0;
	    JsonValue** values = NULL;
	
	    while (JsonParser_SkipSpace(parser) > 0 && JsonParser_PeekChar(parser) != ']')
	    {
	        if (length > 0)
	        {
                JsonParser_MatchChar(parser, JSON_ARRAY, ',');
	        }
	    
	        JsonValue* value = JsonParser_ParseSingle(parser, NULL);
            
            int   oldSize   = sizeof(int) + (length + 0) * sizeof(JsonValue*);
            int   newSize   = sizeof(int) + (length + 1) * sizeof(JsonValue*);
            //void* newValues = values;
            void* newValues = JsonBucket_Resize(parser->arrayValuesBucket, values ? (int*)values - 1 : NULL, oldSize, newSize);
            if (!newValues)
            {
                /* Get from unused buckets (a.k.a reuse json_state_t) */
                while (parser->arrayValuesBucket && parser->arrayValuesBucket->prev)
                {
                    parser->arrayValuesBucket = parser->arrayValuesBucket->prev;
                    newValues = JsonBucket_Acquire(parser->arrayValuesBucket, newSize);
                    if (!newValues)
                    {
                        break;
                    }
                }
            
                if (!newValues)
                {
                    /* Create new buckets */
                    parser->arrayValuesBucket = JsonBucket_Make(parser, parser->arrayValuesBucket, JSON_VALUE_BUCKETS, 1);
                    
                    newValues = JsonBucket_Acquire(parser->arrayValuesBucket, newSize);
                    if (!newValues)
                    {
                        Json_Panic(parser, JSON_ARRAY, JSON_ERROR_MEMORY, "Out of memory when create <array>");
                    }
                    else if (values)
                    {
                        memcpy(newValues, (int*)values - 1, oldSize);
                    }
                }
            }

            values = (JsonValue**)((int*)newValues + 1);
	        values[length++] = value;
	    }

	    JsonParser_SkipSpace(parser);
	    JsonParser_MatchChar(parser, JSON_ARRAY, ']');

        if (values)
        {
            *((int*)values - 1) = length;
        }

	    root->array = values;
	    return root;
    }
}

/* JsonParser_ParseSingle */
static JsonValue* JsonParser_ParseSingle(JsonParser* parser, JsonValue* value)
{
    if (JsonParser_SkipSpace(parser) < 0)
    {
        return NULL;
    }
    else
    {
	    int c = JsonParser_PeekChar(parser);
	
	    switch (c)
	    {
	    case '[':
	        return JsonParser_ParseArray(parser, value);
	    
	    case '{':
	        return JsonParser_ParseObject(parser, value);
	    
	    case '"':
	        return JsonParser_ParseString(parser, value);

	    case '+': case '-': case '0': 
        case '1': case '2': case '3': 
        case '4': case '5': case '6': 
        case '7': case '8': case '9':
	        return JsonParser_ParseNumber(parser, value);
	    
        default:
	    {
	        int length = 0;
	        while (c > 0 && isalpha(c))
	        {
                length++;
                c = JsonParser_NextChar(parser);
	        }

	        const char* token = parser->buffer + parser->cursor - length;
	        if (length == 4 && strncmp(token, "true", 4) == 0)
	        {
                if (!value) value = JsonValue_Make(parser, JSON_BOOLEAN);
                else        value->type = JSON_BOOLEAN;
                value->boolean = JSON_TRUE;
                return value;
	        }
	        else if (length == 4 && strncmp(token, "null", 4) == 0)
            {
                return value ? (value->type = JSON_NULL, value) : JsonValue_Make(parser, JSON_NULL);
            }
	        else if (length == 5 && strncmp(token, "false", 5) == 0)
	        {
                return value ? (value->type = JSON_BOOLEAN, value->boolean = JSON_FALSE, value) : JsonValue_Make(parser, JSON_BOOLEAN);
	        }
	        else
	        {
                char tmp[256];
                tmp[length] = 0;
                while (length--)
                {
                    tmp[length] = token[length]; 
                }

                Json_Panic(parser, JSON_NONE, JSON_ERROR_UNEXPECTED, "Unexpected token '%s'", tmp);
	        }
	    } break;
	    /* END OF SWITCH STATEMENT */
        }

        return NULL;
    }
}

static const char* JsonParser_ParseStringNoToken(JsonParser* parser)
{
    const int HEADER_SIZE = sizeof(int);

    int   i;
    int   c0, c1;
    int   length = 0;
    char  tmpBuffer[1024];
    char* tmpString = tmpBuffer;
    int   capacity = sizeof(tmpBuffer);
    while (!JsonParser_IsEOF(parser) && (c0 = JsonParser_PeekChar(parser)) != '"')
    {
        if (length > capacity)
        {
            capacity <<= 1;
            if (tmpString != tmpBuffer)
            {
                tmpString = (char*)malloc(capacity);
            }
            else
            {
                tmpString = (char*)realloc(tmpString, capacity);
                if (!tmpString)
                {
                    Json_Panic(parser, JSON_STRING, JSON_ERROR_MEMORY, "Out of memory when create new <string>");
                    return NULL;
                }
            }
        }

        if (c0 == '\\')
        {
            c0 = JsonParser_NextChar(parser);
            switch (c0)
            {
            case 'n':
                tmpString[length++] = '\n';
                break;

            case 't':
                tmpString[length++] = '\t';
                break;

            case 'r':
                tmpString[length++] = '\r';
                break;

            case 'b':
                tmpString[length++] = '\b';
                break;

            case '\\':
                tmpString[length++] = '\\';
                break;

            case '"':
                tmpString[length++] = '\"';
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
                        Json_Panic(parser, JSON_STRING, JSON_ERROR_UNKNOWN, "Expected hexa character in unicode character");
                    }
                }

                if (c1 <= 0x7F)
                {
                    tmpString[length++] = c1;
                }
                else if (c1 <= 0x7FF)
                {
                    tmpString[length++] = 0xC0 | (c1 >> 6);            /* 110xxxxx */
                    tmpString[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                }
                else if (c1 <= 0xFFFF)
                {
                    tmpString[length++] = 0xE0 | (c1 >> 12);           /* 1110xxxx */
                    tmpString[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                    tmpString[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                }
                else if (c1 <= 0x10FFFF)
                {
                    tmpString[length++] = 0xF0 | (c1 >> 18);           /* 11110xxx */
                    tmpString[length++] = 0x80 | ((c1 >> 12) & 0x3F);  /* 10xxxxxx */
                    tmpString[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                    tmpString[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                }
                break;

            default:
                Json_Panic(parser, JSON_STRING, JSON_ERROR_UNKNOWN, "Unknown escape character");
                break;
            }
        }
        else
        {
            switch (c0)
            {
            case '\r':
            case '\n':
                Json_Panic(parser, JSON_STRING, JSON_ERROR_UNEXPECTED, "Unexpected newline characters '%c'", c0);
                break;

            default:
                tmpString[length++] = c0;
                break;
            }
        }

        JsonParser_NextChar(parser);
    }
    JsonParser_MatchChar(parser, JSON_STRING, '"');

    if (tmpString)
    {
        tmpString[length] = 0;

        size_t size   = HEADER_SIZE + ((size_t)length + 1);
        char*  string = (char*)JsonBucket_Acquire(parser->stringBucket, size);
        if (!string)
        {
            /* Get from unused buckets */
            while (parser->stringBucket && parser->stringBucket->prev)
            {
                parser->stringBucket = parser->stringBucket->prev;
                string = (char*)JsonBucket_Acquire(parser->stringBucket, capacity);
                if (string)
                {
                    break;
                }
            }

            /* Create new bucket */
            parser->stringBucket = JsonBucket_Make(parser, parser->stringBucket, JSON_STRING_BUCKETS, 1);
            string = (char*)JsonBucket_Acquire(parser->stringBucket, capacity);
            if (!string)
            {
                Json_Panic(parser, JSON_STRING, JSON_ERROR_MEMORY, "Out of memory when create new <string>");
                return NULL;
            }
        }

        /* String header */
        ((int*)string)[0] = length;
        //((int*)string)[1] = JsonHash(tmpString, length);
        string = string + HEADER_SIZE;

#if defined(_MSC_VER) && _MSC_VER >= 1200
        strncpy_s(string, length + 1, tmpString, length);
#else
        strncpy(string, tmpString, length);
#endif

        if (tmpString != tmpBuffer)
        {
            free(tmpString);
        }

        return string;
    }

    return NULL;
}

/* @funcdef: JsonParser_ParseString */
static JsonValue* JsonParser_ParseString(JsonParser* parser, JsonValue* value)
{
    if (JsonParser_SkipSpace(parser) < 0)
    {
        return NULL;
    }
    else
    {
        JsonParser_MatchChar(parser, JSON_STRING, '"');

        const char* string = JsonParser_ParseStringNoToken(parser);
        if (!string)
        {

            return NULL;
        }

        if (!value)
        {
            value = JsonValue_Make(parser, JSON_STRING);
        }
        else
        {
            value->type = JSON_STRING;
        }

        value->string = string;
        return value;
    }
}

/* @funcdef: JsonParser_ParseObject */
static JsonValue* JsonParser_ParseObject(JsonParser* parser, JsonValue* root)
{
    if (JsonParser_SkipSpace(parser) < 0)
    {
        return NULL;
    }
    else
    {
        JsonParser_MatchChar(parser, JSON_OBJECT, '{');

        if (!root)
        {
            root = JsonValue_Make(parser, JSON_OBJECT);
        }
        else
        {
            root->type   = JSON_OBJECT;
            root->object = NULL;
        }

        int length = 0;
        while (JsonParser_SkipSpace(parser) > 0 && JsonParser_PeekChar(parser) != '}')
        {
            if (length > 0)
            {
                JsonParser_MatchChar(parser, JSON_OBJECT, ',');
            }

            if (JsonParser_SkipSpace(parser) != '"')
            {
                Json_Panic(parser, JSON_OBJECT, JSON_ERROR_UNEXPECTED, "Expected <string> for <member-key> of <object>");
            }

            const char* name       = JsonParser_ParseStringNoToken(parser);
            int         nameLength = JsonLength(name);

            JsonParser_SkipSpace(parser);
            JsonParser_MatchChar(parser, JSON_OBJECT, ':');

            JsonValue* value = JsonParser_ParseSingle(parser, NULL);

            /* Append new pair of value to container */
            int   oldLength = length++;
            int   oldSize   = sizeof(int) + oldLength * sizeof(root->object[0]);
            int   newSize   = sizeof(int) + length * sizeof(root->object[0]);
            //void* newValues = NULL;
            void* newValues = JsonBucket_Resize(parser->objectValuesBucket,
                                                   root->object ? (int*)root->object - 1 : NULL,
                                                   oldSize, 
                                                   newSize);
            if (!newValues)
            {
                /* Get from unused buckets */
                while (parser->objectValuesBucket && parser->objectValuesBucket->prev)
                {
                    parser->objectValuesBucket = parser->objectValuesBucket->prev;
                    newValues = JsonBucket_Acquire(parser->objectValuesBucket, length);
                    if (newValues)
                    {
                        break;
                    }
                }
            
                if (!newValues)
                {
                    /* Create new buffer */
                    parser->objectValuesBucket = JsonBucket_Make(parser, parser->objectValuesBucket, JSON_VALUE_BUCKETS, 1);
                    
                    /* Continue get new buffer for values */
                    newValues = JsonBucket_Acquire(parser->objectValuesBucket, length);
                    if (!newValues)
                    {
                        Json_Panic(parser, JSON_OBJECT, JSON_ERROR_MEMORY, "Out of memory when create <object>");
                    }
                    else if (root->object)
                    {
                        memcpy(newValues, (int*)root->object - 1, oldSize);
                    }
                }
            }

            /* When code reach here, newValues should not invalid */
            assert(newValues != NULL && "An error occurred but is not handled");

            /* Well done */
            *((void**)&root->object) = (int*)newValues + 1;
            root->object[oldLength].hash  = JsonHash(name, nameLength);
#ifdef JSON_OBJECT_KEYNAME
            root->object[oldLength].name  = name;
#endif
            root->object[oldLength].value = value;
        }

        if (root->object)
        {
            *((int*)root->object - 1) = length;
        }

        JsonParser_SkipSpace(parser);
        JsonParser_MatchChar(parser, JSON_OBJECT, '}');
        return root;
    }
}
         
/* Internal parsing function
 */
static JsonValue* JsonParser_ParseTopLevel(JsonParser* parser)
{
    if (!parser)
    {
        return NULL;
    }

    if (JsonParser_SkipSpace(parser) == '{')
    {
        if (setjmp(parser->errjmp) == 0)
        {
            JsonValue* value = JsonParser_ParseObject(parser, NULL);

            JsonParser_SkipSpace(parser);
            if (!JsonParser_IsEOF(parser))
            {
                Json_Panic(parser, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <object>.");
            }

            return value;
        }
        else
        {
            return NULL;
        }
    }
    else if (JsonParser_SkipSpace(parser) == '[')
    {
        if (setjmp(parser->errjmp) == 0)
        {
            JsonValue* value = JsonParser_ParseArray(parser, NULL);

            JsonParser_SkipSpace(parser);
            if (!JsonParser_IsEOF(parser))
            {
                Json_Panic(parser, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <array>.");
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
        Json_SetError(parser, JSON_NONE, JSON_ERROR_FORMAT, 
                      "JSON must be starting with '{' or '[', first character is '%c'", JsonParser_PeekChar(parser));
        return NULL;
    }
}

/* @funcdef: JsonParse */
JsonValue* JsonParse(const char* json, JsonParser** out_state)
{
    JsonAllocator allocator;
    allocator.data  = NULL;
    allocator.free  = Json_Free;
    allocator.alloc = Json_Alloc;

    return JsonParseEx(json, &allocator, out_state);
}

/* @funcdef: JsonParseEx */
JsonValue* JsonParseEx(const char* json, const JsonAllocator* allocator, JsonParser** outParser)
{
    JsonParser* parser = outParser && *outParser ? JsonParser_Reuse(*outParser, json, allocator) : JsonParser_Make(json, allocator);
    JsonValue*  value = JsonParser_ParseTopLevel(parser);

    if (value)
    {
        if (outParser)
        {
            *outParser = parser;
        }
        else
        {
            if (parser)
            {
                parser->next = rootParser;
                rootParser  = parser;
            }
        }
    }
    else
    {
        if (outParser)
        {
            *outParser = parser;
        }
        else
        {
            JsonParser_Free(parser);
        }
    }

    return value;
}

/* @funcdef: JsonRelease */
void JsonRelease(JsonParser* parser)
{
    if (parser)
    {
        JsonParser_Free(parser);
    }
    else
    {
        JsonParser_Free(rootParser);
        rootParser = NULL;
    }
}

/* @funcdef: JsonGetError */
JsonError JsonGetError(const JsonParser* parser)
{
    if (parser)
    {
        return parser->errnum;
    }
    else
    {
        return JSON_ERROR_NONE;
    }
}

/* @funcdef: JsonGetErrorString */
const char* JsonGetErrorString(const JsonParser* parser)
{
    if (parser)
    {
        return parser->errmsg;
    }
    else
    {
        return NULL;
    }
}

/* @funcdef: JsonLength */
int JsonLength(const JsonValue* x)
{
    if (x)
    {
        switch (x->type)
        {
        case JSON_ARRAY:
            return x->array ? *((int*)x->array - 1) : 0;

        case JSON_STRING:
            return x->string ? *((int*)x->string - 2) : 0;

        case JSON_OBJECT:
            return x->object ? *((int*)x->object - 1) : 0;

        default:
            break;
        }
    }

    return 0;
}

/* @funcdef: JsonEquals */
JsonBoolean JsonEquals(const JsonValue* a, const JsonValue* b)
{
    int i, n;

    if (a == b)
    {
        return JSON_TRUE;
    }

    if (!a || !b)
    {
        return JSON_FALSE;
    }

    if (a->type != b->type)
    {
        return JSON_FALSE;
    }

    switch (a->type)
    {
    case JSON_NULL:
    case JSON_NONE:
        return JSON_TRUE;

    case JSON_NUMBER:
        return a->number == b->number;

    case JSON_BOOLEAN:
        return a->boolean == b->boolean;

    case JSON_ARRAY:
        if ((n = JsonLength(a)) == JsonLength(b))
        {
            for (i = 0; i < n; i++)
            {
                if (!JsonEquals(a->array[i], b->array[i]))
                {
                    return JSON_FALSE;
                }
            }
        }
        return JSON_TRUE;

    case JSON_OBJECT:
        if ((n = JsonLength(a)) == JsonLength(b))
        {
            for (i = 0; i < n; i++)
            {
                //const char* str0 = a->object[i].name;
                //const char* str1 = b->object[i].name;
                //if (((int*)str0 - 2)[1] != ((int*)str1 - 2)[1] || strcmp(str0, str1) == 0)
                //{
                //    return JSON_FALSE;
                //}
                if (a->object[i].hash != b->object[i].hash)
                {
                    return JSON_FALSE;
                }

                if (!JsonEquals(a->object[i].value, b->object[i].value))
                {
                    return JSON_FALSE;
                }
            }
        }
        return JSON_TRUE;

    case JSON_STRING:
        //return ((int*)a->string - 2)[1] == ((int*)b->string - 2)[1] && strcmp(a->string, b->string) == 0;
        return strcmp(a->string, b->string) == 0;
    }

    return JSON_FALSE;
}

/* @funcdef: JsonFind */
JsonValue* JsonFind(const JsonValue* obj, const char* name)
{
    if (obj && obj->type == JSON_OBJECT)
    {
        int i, n;
        int hash = JsonHash((void*)name, strlen(name));
        for (i = 0, n = JsonLength(obj); i < n; i++)
        {
            if (hash == obj->object[i].hash)
            {
                return obj->object[i].value;
            }
        }
    }

    return NULL;
}

/* @funcdef: JsonFindWithHash */
JsonValue* JsonFindWithHash(const JsonValue* obj, int hash)
{
    if (obj && obj->type == JSON_OBJECT)
    {
        int i, n;
        for (i = 0, n = JsonLength(obj); i < n; i++)
        {
            if (hash == obj->object[i].hash)
            {
                return obj->object[i].value;
            }
        }
    }

    return NULL;
}

/* @funcdef: JsonWrite */
void JsonWrite(const JsonValue* value, FILE* out)
{
    if (value)
    {
        int i, n;

        switch (value->type)
        {
        case JSON_NULL:
            fprintf(out, "null");
            break;

        case JSON_NUMBER:
            fprintf(out, "%lf", value->number);
            break;

        case JSON_BOOLEAN:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JSON_STRING:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JSON_ARRAY:
            fprintf(out, "[");
            for (i = 0, n = JsonLength(value); i < n; i++)
            {
                JsonWrite(value->array[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JSON_OBJECT:
            fprintf(out, "{");
            for (i = 0, n = JsonLength(value); i < n; i++)
            {
#ifdef JSON_OBJECT_KEYNAME
                fprintf(out, "\"%s\" : ", value->object[i].name);
#else
                fprintf(out, "\"<hash: %d>\" : ", value->object[i].hash);
#endif

                JsonWrite(value->object[i].value, out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }            
            }
            fprintf(out, "}");
            break;

        case JSON_NONE:
        default:
            break;
        }
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const JsonValue* value, FILE* out)
{
    if (value)
    {
        int i, n;
        static int indent = 0;

        switch (value->type)
        {
        case JSON_NULL:
            fprintf(out, "null");
            break;

        case JSON_NUMBER:
            fprintf(out, "%lf", value->number);
            break;

        case JSON_BOOLEAN:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JSON_STRING:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JSON_ARRAY:
            fprintf(out, "[\n");

            indent++;
            for (i = 0, n = JsonLength(value); i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                JsonPrint(value->array[i], out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }  
            fputc(']', out);
            break;

        case JSON_OBJECT:
            fprintf(out, "{\n");

            indent++;
            for (i = 0, n = JsonLength(value); i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

#ifdef JSON_OBJECT_KEYNAME
                fprintf(out, "\"%s\" : ", value->object[i].name);
#else
                fprintf(out, "\"<hash: %d>\" : ", value->object[i].hash);
#endif
                JsonPrint(value->object[i].value, out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fputc('\n', out);
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {                    
                fputc(' ', out);
            }                    
            fputc('}', out);
            break;

        case JSON_NONE:
        default:
            break;
        }
    }
}
