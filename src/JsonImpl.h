#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

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

#if defined(__GNUC__) || defined(__clang__)
#   define JSON_MAYBE_UNUSED __attribute__((unused))
#else
#   define JSON_MAYBE_UNUSED
#endif

#ifndef JSON_VALUE_BUCKETS
#define JSON_VALUE_BUCKETS 4096
#endif

#ifndef JSON_STRING_BUCKETS
#define JSON_STRING_BUCKETS 4096
#endif

#ifndef JSON_VALUE_POOL_COUNT
#define JSON_VALUE_POOL_COUNT (4096/sizeof(JsonValue))
#endif

typedef struct JsonPool JsonPool;

typedef struct JsonArray
{
    void*       next;
    int         size;
    int         count;
    char        buffer[];
} JsonArray;

/* Next power of two */
static JSON_INLINE JSON_MAYBE_UNUSED int Json_NextPOT(int x)
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

#define JsonArray_GetRaw(a)             ((JsonArray*)((char*)(a) - sizeof(JsonArray)))
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

static void* JSON_MAYBE_UNUSED JsonArray_Grow(void* array, int reqsize, int elemsize, JsonAllocator* allocator)
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

    JsonArray* newArray = (JsonArray*)allocator->alloc(allocator->data, sizeof(JsonArray) + size * elemsize);
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

#define JsonTempArray(T, CAPACITY)  struct {    \
    T*  array;                                  \
    int count;                                  \
    T   buffer[CAPACITY]; }

#define JsonTempArray_Init()              { 0, 0 }
#define JsonTempArray_Free(a, alloc)      JsonArray_Free((a)->array, alloc)
#define JsonTempArray_Push(a, v, alloc)   ((a)->count >= sizeof((a)->buffer) / sizeof((a)->buffer[0]) ? 0 : ((a)->buffer[(a)->count++] = v, 1))
#define JsonTempArray_ToArray(a, alloc)   JsonTempArray_ToArrayFunc(a, (int)sizeof((a)->buffer[0]), alloc)

static void* JsonTempArray_ToArrayFunc(void* tempArray, int itemSize, JsonAllocator* allocator)
{
    typedef JsonTempArray(char, 0) TempArrayView;
    TempArrayView* tempArrayView = (TempArrayView*)tempArray;

    void* array = JsonArray_Grow(0, tempArrayView->count, itemSize, allocator);
    if (array)
    {
        JsonArray* realArray = JsonArray_GetRaw(array);
        realArray->count = tempArrayView->count;
        memcpy(array, tempArrayView->buffer, tempArrayView->count * itemSize);

        //if (itemSize == 8)
        //{
        //    JsonValue** values = (JsonValue*)array;
        //    (void)values;
        //    values += 1;
        //}
    }
    return array;
}

struct JsonPool
{
    int             grow;
    int             size;

    JsonPool*       prev;
    JsonPool*       next;

    void**          head;

    JsonAllocator*  allocator;
};

struct JsonStringPool
{
    int  size;
    int  count;
    char buffer[];
};

struct JsonState
{
    JsonValue*      root;
    JsonState*     next;
    JsonPool*       valuePool;
    JsonPool*       arrayPool;  /* For parsing */

    int             line;
    int             column;
    int             cursor;
    JsonType        parsingType;
    
    int             length;
    char*           buffer;
    
    JsonError       errnum;
    char*           errmsg;
    jmp_buf         errjmp;

    JsonAllocator   allocator; /* Runtime allocator */
};

static JsonState* rootState = NULL;

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

/* funcdef: JsonPool_Make */
static JsonPool* JsonPool_Make(JsonState* state, JsonPool* prev, int count, int size, int grow)
{
    assert(size > 0);
    assert(count >= 0);

    int pool_size = count * (sizeof(void*) + size);
    JsonPool* pool = (JsonPool*)state->allocator.alloc(state->allocator.data, sizeof(JsonPool) + pool_size);
    if (pool)
    {
        if (prev)
        {
            prev->next = pool;
        }

        pool->grow      = grow;
        pool->size      = size;
		pool->prev      = prev;
		pool->next      = NULL;
        pool->allocator = &state->allocator;

        if (count > 0)
        {
            pool->head = (void**)((char*)pool + sizeof(JsonPool));

            int i;
            void** node = pool->head;
            for (i = 0; i < count - 1; i++)
            {
                node = (void**)(*node = (char*)node + sizeof(void*) + size);
            }
            *node = NULL;
        }
        else
        {
            pool->head = NULL;
        }
    }
    
    return pool;
}

/* funcdef: JsonPool_Free */
static void JsonPool_Free(JsonState* state, JsonPool* pool)
{
    if (pool)
    {
		JsonPool_Free(state, pool->prev);
		state->allocator.free(state->allocator.data, pool);
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
    else if (pool->grow)
    {
        void* res = pool->allocator->alloc(pool->allocator->data, pool->size);
        return res;
    }

    return NULL;
}

/* funcdef: JsonPool_Release */
static void JSON_MAYBE_UNUSED JsonPool_Release(JsonPool* pool, void* ptr)
{
    if (ptr)
    {
		void** node = (void**)((char*)ptr - sizeof(void*));
		*node = pool->head;
		pool->head = node;
    }
}

/* @funcdef: JsonValue_Make */
static JsonValue* JsonValue_Make(JsonState* state, JsonType type)
{
    if (!state->valuePool || !state->valuePool->head)
    {
        if (state->valuePool && state->valuePool->prev)
        {
            state->valuePool = state->valuePool->prev;
        }
        else
        {
            state->valuePool = JsonPool_Make(state, state->valuePool, JSON_VALUE_POOL_COUNT, sizeof(JsonValue), JSON_FALSE);
        }

		if (!state->valuePool)
		{
			Json_Panic(state, type, JSON_ERROR_MEMORY, "Out of memory");
		}
    }
    
    JsonValue* value = (JsonValue*)JsonPool_Acquire(state->valuePool);
    if (value)
    {
		memset(value, 0, sizeof(JsonValue));
		value->type    = type;
		value->boolean = JSON_FALSE;
    }
    else
    {
		Json_Panic(state, type, JSON_ERROR_MEMORY, "Out of memory");
    }
    return value;
}

static void JsonValue_ReleaseMemory(JsonValue* value, JsonAllocator* allocator)
{
    if (value)
    {
        switch (value->type)
        {
        case JSON_ARRAY:
            for (int i = 0, n = JsonLength(value); i < n; i++)
            {
                JsonValue_ReleaseMemory(value->array[i], allocator);
            }
            JsonArray_Free(value->array, allocator);
            break;

        case JSON_OBJECT:
            for (int i = 0, n = JsonLength(value); i < n; i++)
            {
                JsonValue_ReleaseMemory(value->object[i].value, allocator);
            }
            JsonArray_Free(value->object, allocator);
            break;

        default:
            break;
        }
    }
}

/* @funcdef: JsonState_Make */
static JsonState* JsonState_Make(const char* json, const JsonAllocator* allocator)
{
    JsonState* state = (JsonState*)allocator->alloc(allocator->data, sizeof(JsonState));
    if (state)
    {
		state->next            = NULL;

		state->line            = 1;
		state->column          = 1;
		state->cursor          = 0;
		state->length          = (int)strlen(json);
		state->buffer          = (char*)memcpy(allocator->alloc(allocator->data, state->length), json, state->length);

		state->errmsg          = NULL;
		state->errnum          = JSON_ERROR_NONE;

		state->valuePool       = NULL;
        state->arrayPool       = NULL;
        //state->arrayValuesBucket    = NULL;
        //state->objectValuesBucket   = NULL;

        state->allocator = *allocator;
    }
    return state;
}

/* @funcdef: JsonState_Reuse */
static JsonState* JsonState_Reuse(JsonState* state, const char* json, const JsonAllocator* allocator)
{
    if (state)
    {
        if (state == rootState)
        {
            rootState = state->next;
        }
        else
        {
            JsonState* list = rootState;
            while (list)
            {
                if (list->next == state)
                {
                    list->next = state->next;
                }
            }

		    state->next = NULL;
        }

		state->line   = 1;
		state->column = 1;
		state->cursor = 0;
		state->errnum = JSON_ERROR_NONE;

        int newLength = (int)strlen(json);

        if (state->allocator.data  != allocator->data ||
            state->allocator.free  != allocator->free ||
            state->allocator.alloc != allocator->alloc)
        {
            JsonPool_Free(state, state->valuePool);
            state->valuePool = NULL;

            state->allocator.free(state->allocator.data, state->errmsg); 
            state->errmsg = NULL;

            state->allocator.free(state->allocator.data, state->buffer);
            state->buffer = allocator->alloc(allocator->data, newLength);
        }
        else
        {
            if (state->errmsg) state->errmsg[0] = 0;

            while (state->valuePool)
            {
                state->valuePool->head = (void**)(state->valuePool + 1);
                if (state->valuePool->prev)
                {
                    break;
                }
                else
                {
                    state->valuePool = state->valuePool->prev;
                }
            }

            if (state->length < newLength)
            {
                allocator->free(allocator->data, state->buffer);
                state->buffer = allocator->alloc(allocator->data, newLength);
            }
        }

        state->length = newLength;
        state->buffer = (char*)memcpy(state->buffer, json, newLength + 1);
    }
    return state;
}

/* @funcdef: JsonState_Free */
static void JsonState_Free(JsonState* state)
{
    if (state)
    {
        JsonValue* root = state->root;
        JsonValue_ReleaseMemory(root, &state->allocator);

		JsonState* next = state->next;

        //JsonStringPool_Free(state, state->objectValuesBucket);
        //JsonStringPool_Free(state, state->arrayValuesBucket);
		JsonPool_Free(state, state->valuePool);

        state->allocator.free(state->allocator.data, state->buffer);
		state->allocator.free(state->allocator.data, state->errmsg);
		state->allocator.free(state->allocator.data, state);

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

#if 0 /* UNUSED */
static int Json_NextLine(JsonState* state)
{
    int c = Json_PeekChar(state);
    while (c > 0 && c != '\n')
    {
		c = Json_NextChar(state);
    }
    return Json_NextChar(state);
}
#endif

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
        Json_Panic(state, type, JSON_ERROR_UNMATCH, "Expected '%c'", c);
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

static JsonValue* Json_ParseArray(JsonState* state);
static JsonValue* Json_ParseSingle(JsonState* state);
static JsonValue* Json_ParseObject(JsonState* state);
static JsonValue* Json_ParseNumber(JsonState* state);
static JsonValue* Json_ParseString(JsonState* state);

/* @funcdef: Json_ParseNumber */
static JsonValue* Json_ParseNumber(JsonState* state)
{
    if (Json_SkipSpace(state) < 0)
    {
		return NULL;
    }
    else
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
            Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'e' is presented in number token, but require a digit after 'e' ('%c')", c);
        }
		if (dot && numpow == 1)
		{
			Json_Panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'.' is presented in number token, but require a digit after '.' ('%c')", c);
			return NULL;
		}
		else
		{
            JsonValue* value = JsonValue_Make(state, JSON_NUMBER);
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

/* @funcdef: Json_ParseArray */
static JsonValue* Json_ParseArray(JsonState* state)
{
    if (Json_SkipSpace(state) < 0)
    {
        return NULL;
    }
    else
    {
	    Json_MatchChar(state, JSON_ARRAY, '[');

        //if (!state->arrayPool)
        //{
        //    state->arrayPool = JsonPool_Make(state, NULL, 0, 16 * sizeof(JsonValue*), JSON_FALSE);
        //}

	    int         length = 0;
	    //JsonValue** values = (JsonValue**)JsonPool_Acquire(state->arrayPool);
        //
        //JsonArray_Clear(values);

        JsonTempArray(JsonValue*, 128) values = JsonTempArray_Init();

	    while (Json_SkipSpace(state) > 0 && Json_PeekChar(state) != ']')
	    {
	        if (length > 0)
	        {
                Json_MatchChar(state, JSON_ARRAY, ',');
	        }
	    
	        JsonValue* value = Json_ParseSingle(state);
            //JsonArray_Push(values, value, &state->allocator);
            JsonTempArray_Push(&values, value, &state->allocator);
            length++;
	    }

	    Json_SkipSpace(state);
	    Json_MatchChar(state, JSON_ARRAY, ']');

        JsonValue* value = JsonValue_Make(state, JSON_ARRAY);
        //value->array = JsonArray_Clone(values, &state->allocator);
        //JsonPool_Release(state->arrayPool, values);
        value->array = (JsonValue**)JsonTempArray_ToArray(&values, &state->allocator);
        JsonTempArray_Free(&values, &state->allocator);
	    return value;
    }
}

/* Json_ParseSingle */
static JsonValue* Json_ParseSingle(JsonState* state)
{
    if (Json_SkipSpace(state) < 0)
    {
        return NULL;
    }
    else
    {
	    int c = Json_PeekChar(state);
	
	    switch (c)
	    {
	    case '[':
	        return Json_ParseArray(state);
	    
	    case '{':
	        return Json_ParseObject(state);
	    
	    case '"':
	        return Json_ParseString(state);

	    case '+': case '-': case '0': 
        case '1': case '2': case '3': 
        case '4': case '5': case '6': 
        case '7': case '8': case '9':
	        return Json_ParseNumber(state);
	    
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
                JsonValue* value = JsonValue_Make(state, JSON_BOOLEAN);
                value->boolean = JSON_TRUE;
                return value;
	        }
	        else if (length == 4 && strncmp(token, "null", 4) == 0)
            {
                JsonValue* value = JsonValue_Make(state, JSON_NULL);
                return value;
            }
	        else if (length == 5 && strncmp(token, "false", 5) == 0)
	        {
                JsonValue* value = JsonValue_Make(state, JSON_NULL);
                value->boolean = JSON_FALSE;
                return value;
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

        return NULL;
    }
}

static const char* Json_ParseStringNoToken(JsonState* state, int* outLength)
{
    Json_MatchChar(state, JSON_STRING, '"');

    int   i;
    int   c0, c1;
    int   length = 0;
    char* string = &state->buffer[state->cursor];
    while (!Json_IsEOF(state) && (c0 = Json_PeekChar(state)) != '"')
    {
        if (c0 == '\\')
        {
            c0 = Json_NextChar(state);
            switch (c0)
            {
            case 'n':
                string[length++] = '\n';
                break;

            case 't':
                string[length++] = '\t';
                break;

            case 'r':
                string[length++] = '\r';
                break;

            case 'b':
                string[length++] = '\b';
                break;

            case '\\':
                string[length++] = '\\';
                break;

            case '"':
                string[length++] = '\"';
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
                    string[length++] = c1;
                }
                else if (c1 <= 0x7FF)
                {
                    string[length++] = 0xC0 | (c1 >> 6);            /* 110xxxxx */
                    string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                }
                else if (c1 <= 0xFFFF)
                {
                    string[length++] = 0xE0 | (c1 >> 12);           /* 1110xxxx */
                    string[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                    string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                }
                else if (c1 <= 0x10FFFF)
                {
                    string[length++] = 0xF0 | (c1 >> 18);           /* 11110xxx */
                    string[length++] = 0x80 | ((c1 >> 12) & 0x3F);  /* 10xxxxxx */
                    string[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                    string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
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
                length++;
                //state->buffer[length++] = c0;
                break;
            }
        }

        Json_NextChar(state);
    }

    Json_MatchChar(state, JSON_STRING, '"');
    if (outLength) *outLength = length;
    string[length] = 0;
    return string;
}

/* @funcdef: Json_ParseString */
static JsonValue* Json_ParseString(JsonState* state)
{
    if (Json_SkipSpace(state) < 0)
    {
        return NULL;
    }
    else
    {
        const char* string = Json_ParseStringNoToken(state, NULL);
        if (!string)
        {

            return NULL;
        }

        JsonValue* value = JsonValue_Make(state, JSON_STRING);
        value->string = string;
        return value;
    }
}

/* @funcdef: Json_ParseObject */
static JsonValue* Json_ParseObject(JsonState* state)
{
    if (Json_SkipSpace(state) < 0)
    {
        return NULL;
    }
    else
    {
        Json_MatchChar(state, JSON_OBJECT, '{');

        JsonTempArray(JsonObjectEntry, 128) values = JsonTempArray_Init();

        JsonValue* root = JsonValue_Make(state, JSON_OBJECT);
        root->object = NULL;

        int length = 0;
        while (Json_SkipSpace(state) > 0 && Json_PeekChar(state) != '}')
        {
            if (length > 0)
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

            JsonValue* value = Json_ParseSingle(state);

            /* Well done */
            //*((void**)&root->object) = (int*)newValues + 1;
            JsonObjectEntry entry;
            entry.hash  = JsonHash(name, nameLength);
#ifdef JSON_OBJECT_KEYNAME
            entry.name  = name;
#endif
            entry.value = value;
            JsonTempArray_Push(&values, entry, &state->allocator);
            //JsonArray_Push(root->object, entry, &state->allocator);

            length++;
        }

        Json_SkipSpace(state);
        Json_MatchChar(state, JSON_OBJECT, '}');

        root->object = (JsonObjectEntry*)JsonTempArray_ToArray(&values, &state->allocator);
        JsonTempArray_Free(&values, &state->allocator);
        return root;
    }
}
         
/* Internal parsing function
 */
static JsonValue* Json_ParseTopLevel(JsonState* state)
{
    if (!state)
    {
        return NULL;
    }

    if (Json_SkipSpace(state) == '{')
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonValue* value = Json_ParseObject(state);

            Json_SkipSpace(state);
            if (!Json_IsEOF(state))
            {
                Json_Panic(state, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <object>.");
            }

            return value;
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
            JsonValue* value = Json_ParseArray(state);

            Json_SkipSpace(state);
            if (!Json_IsEOF(state))
            {
                Json_Panic(state, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <array>.");
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
        Json_SetError(state, JSON_NONE, JSON_ERROR_FORMAT, 
                      "JSON must be starting with '{' or '[', first character is '%c'", Json_PeekChar(state));
        return NULL;
    }
}

/* @funcdef: JsonParse */
JsonValue* JsonParse(const char* json, JsonState** outState)
{
    JsonAllocator allocator;
    allocator.data  = NULL;
    allocator.free  = Json_Free;
    allocator.alloc = Json_Alloc;

    return JsonParseEx(json, &allocator, outState);
}

/* @funcdef: JsonParseEx */
JsonValue* JsonParseEx(const char* json, const JsonAllocator* allocator, JsonState** outState)
{
    JsonState* state = outState && *outState ? JsonState_Reuse(*outState, json, allocator) : JsonState_Make(json, allocator);
    JsonValue* value = Json_ParseTopLevel(state);

    if (value)
    {
        if (state)
        {
            state->root = value;
        }

        if (outState)
        {
            *outState = state;
        }
        else
        {
            if (state)
            {
                state->next = rootState;
                rootState   = state;
            }
        }
    }
    else
    {
        if (outState)
        {
            *outState = state;
        }
        else
        {
            JsonState_Free(state);
        }
    }

    return value;
}

/* @funcdef: JsonRelease */
void JsonRelease(JsonState* state)
{
    if (state)
    {
        JsonState_Free(state);
    }
    else
    {
        JsonState_Free(rootState);
        rootState = NULL;
    }
}

/* @funcdef: JsonGetError */
JsonError JsonGetError(const JsonState* state)
{
    if (state)
    {
        return state->errnum;
    }
    else
    {
        return JSON_ERROR_NONE;
    }
}

/* @funcdef: JsonGetErrorString */
const char* JsonGetErrorString(const JsonState* state)
{
    if (state)
    {
        return state->errmsg;
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
            return JsonArray_GetCount(x->array);

        case JSON_STRING:
            return x->string ? (int)strlen(x->string) : 0;

        case JSON_OBJECT:
            return JsonArray_GetCount(x->object);

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
        int hash = JsonHash((void*)name, (int)strlen(name));
        for (i = 0, n = JsonLength(obj); i < n; i++)
        {
            JsonObjectEntry* entry = &obj->object[i];
            if (hash == entry->hash)
            {
                return entry->value;
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
