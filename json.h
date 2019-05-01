/******************************************************
 * Simple json parser written in ANSI C
 *
 * @author: MaiHD
 * @license: Public domain
 * @copyright: MaiHD @ ${HOME}, 2018 - 2019
 ******************************************************/

#ifndef __JSON_H__
#define __JSON_H__

#define JSON_LIBNAME "libjson"
#define JSON_VERSION "v1.0.00"
#define JSON_VERCODE 10000

#ifndef JSON_API
#define JSON_API
#endif

#ifndef JSON_INLINE
#  if defined(_MSC_VER)
#     define JSON_INLINE __forceinline
#  elif defined(__cplusplus)
#     define JSON_INLINE inline
#  else
#     define JSON_INLINE 
#  endif
#endif

#include <stdio.h>

#ifdef __cplusplus
#include <string.h>
extern "C" {
#endif

/**
 * JSON type of json value
 */
typedef enum JsonType
{
    JSON_NONE,
    JSON_NULL,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_NUMBER,
    JSON_STRING,
    JSON_BOOLEAN,
} JsonType;

/**
 * JSON error code
 */
typedef enum JsonError
{
    JSON_ERROR_NONE,
    
    /* Parsing error */

    JSON_ERROR_FORMAT,
    JSON_ERROR_UNMATCH,
    JSON_ERROR_UNKNOWN,
    JSON_ERROR_UNEXPECTED,
    JSON_ERROR_UNSUPPORTED,

    /* Runtime error */

    JSON_ERROR_MEMORY,
    JSON_ERROR_INTERNAL,
} JsonError;

typedef struct JsonState JsonState;
typedef struct JsonValue JsonValue;

/**
 * JSON boolean data type
 */
#ifdef __cplusplus
typedef bool JsonBoolean;
#define JSON_TRUE  true
#define JSON_FALSE false
#else
typedef enum JsonBoolean
{
	JSON_TRUE  = 1,
	JSON_FALSE = 0,
} JsonBoolean;
#endif

typedef struct
{
    void* data;
    void* (*malloc)(void* data, size_t size);
    void  (*free)(void* data, void* pointer);
} JsonSettings;

JSON_API extern const JsonValue JSON_VALUE_NONE;

JSON_API JsonValue*    json_parse(const char* json, JsonState** state);
JSON_API JsonValue*    json_parse_ex(const char* json, const JsonSettings* settings, JsonState** state);

JSON_API void          json_release(JsonState* state);

JSON_API JsonError     json_get_errno(const JsonState* state);
JSON_API const char*   json_get_error(const JsonState* state);

JSON_API void          json_print(const JsonValue* value, FILE* out);
JSON_API void          json_write(const JsonValue* value, FILE* out);

JSON_API int           json_length(const JsonValue* x);
JSON_API JsonBoolean   json_equals(const JsonValue* a, const JsonValue* b);
JSON_API JsonValue*    json_find(const JsonValue* obj, const char* name);

/**
 * JSON value
 */
struct JsonValue
{
    JsonType type;
    union
    {
		double      number;
		JsonBoolean boolean;

		const char* string;

        struct JsonValue** array;

        struct
        {
            const char*        name;
            struct JsonValue* value;
        }* object;
    };

#ifdef __cplusplus
public: // @region: Constructors
    JSON_INLINE JsonValue()
	{	
        memset(this, 0, sizeof(*this));
	}

    JSON_INLINE ~JsonValue()
    {
        // SHOULD BE EMPTY
        // Memory are managed by json_state_t
    }

public: // @region: Indexor
	JSON_INLINE const JsonValue& operator[] (int index) const
	{
		if (type != JSON_ARRAY || index < 0 || index > json_length(this))
		{
			return JSON_VALUE_NONE;
		}
		else
		{
			return *array[index];
		}	
	}

	JSON_INLINE const JsonValue& operator[] (const char* name) const
	{
		JsonValue* value = json_find(this, name);
        return value ? *value : JSON_VALUE_NONE;
	}

public: // @region: Conversion
	JSON_INLINE operator const char* () const
	{
		if (type == JSON_STRING)
		{
			return string;
		}
		else
		{
			return "";
		}
	}

	JSON_INLINE operator double () const
	{
		return number;
	}

	JSON_INLINE operator bool () const
	{
        switch (type)
        {
        case JSON_NUMBER:
        case JSON_BOOLEAN:
        #ifdef NDEBUG
            return boolean;   // Faster, use when performance needed
        #else
            return !!boolean; // More precision, should use when debug
        #endif

        case JSON_ARRAY:
        case JSON_OBJECT:
        case JSON_STRING:
            return true;

        case JSON_NONE:
        case JSON_NULL:
        default: 
            return false;
        }

	}
#endif /* __cplusplus */
};

/* END OF EXTERN "C" */
#ifdef __cplusplus
}
#endif
/* * */

/* END OF __JSON_H__ */
#endif /* __JSON_H__ */

#ifdef JSON_IMPL

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

typedef struct json_pool
{
    struct json_pool* prev;
    struct json_pool* next;

    void** head;
} json_pool_t;

typedef struct json_bucket
{
    struct json_bucket* prev;
    struct json_bucket* next;

    size_t size;
    size_t count;
    size_t capacity;
} json_bucket_t;

struct JsonState
{
    struct JsonState* next;
    json_pool_t* value_pool;

    json_bucket_t* values_bucket;
    json_bucket_t* string_bucket;
    
    size_t line;
    size_t column;
    size_t cursor;
    //json_type_t parsing_value_type;
    
    size_t      length;
    const char* buffer;
    
    JsonError errnum;
    char*        errmsg;
    jmp_buf      errjmp;

    JsonSettings settings; /* Runtime settings */
};

static JsonState* root_state = NULL;
const struct JsonValue JSON_VALUE_NONE;

/* @funcdef: json__malloc */
static void* json__malloc(void* data, size_t size)
{
    (void)data;
    return malloc(size);
}

/* @funcdef: json__free */
static void json__free(void* data, void* pointer)
{
    (void)data;
    free(pointer);
}

/* @funcdef: json__set_error_valist */
static void json__set_error_valist(JsonState* state, JsonType type, JsonError code, const char* fmt, va_list valist)
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
        state->errmsg = (char*)state->settings.malloc(state->settings.data, errmsg_size);
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

/* @funcdef: json__set_error */
static void json__set_error(JsonState* state, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    json__set_error_valist(state, type, code, fmt, varg);
    va_end(varg);
}

/* funcdef: json__panic */
static void json__panic(JsonState* state, JsonType type, JsonError code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    json__set_error_valist(state, type, code, fmt, varg);
    va_end(varg);

    longjmp(state->errjmp, code);
}

/* funcdef: json__make_pool */
static json_pool_t* json__make_pool(JsonState* state, json_pool_t* prev, int count, int size)
{
    if (count <= 0 || size <= 0)
    {
		return NULL;
    }

    int pool_size = count * (sizeof(void*) + size);
    json_pool_t* pool = (json_pool_t*)state->settings.malloc(state->settings.data, sizeof(json_pool_t) + pool_size);
    if (pool)
    {
        if (prev)
        {
            prev->next = pool;
        }

		pool->prev = prev;
		pool->next = NULL;
		pool->head = (void**)((char*)pool + sizeof(json_pool_t));
		
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

/* funcdef: json__free_pool */
static void json__free_pool(JsonState* state, json_pool_t* pool)
{
    if (pool)
    {
		json__free_pool(state, pool->prev);
		state->settings.free(state->settings.data, pool);
    }
}

/* funcdef: json__pool_extract */
static void* json__pool_extract(json_pool_t* pool)
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

#if 0 /* UNUSED */
/* funcdef: json__pool_collect */
static void json__pool_collect(json_pool_t* pool, void* ptr)
{
    if (ptr)
    {
		void** node = (void**)((char*)ptr - sizeof(void*));
		*node = pool->head;
		pool->head = node;
    }
}
#endif

/* funcdef: json__make_bucket */
static json_bucket_t* json__make_bucket(JsonState* state, json_bucket_t* prev, size_t count, size_t size)
{
    if (count <= 0 || size <= 0)
    {
        return NULL;
    }

    json_bucket_t* bucket = (json_bucket_t*)state->settings.malloc(state->settings.data, sizeof(json_bucket_t) + count * size);
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

/* funcdef: json__free_bucket */
static void json__free_bucket(JsonState* state, json_bucket_t* bucket)
{
    if (bucket)
    {
        json__free_bucket(state, bucket->next);
        state->settings.free(state->settings.data, bucket);
    }
}

/* funcdef: json__bucket_extract */
static void* json__bucket_extract(json_bucket_t* bucket, int count)
{
    if (!bucket || count <= 0)
    {
        return NULL;
    }
    else if (bucket->count + count <= bucket->capacity)
    {
        void* res = (char*)bucket + sizeof(json_bucket_t) + bucket->size * bucket->count;
        bucket->count += count;
        return res;
    }
    else
    {
        return NULL;
    }
}

/* funcdef: json__bucket_resize */
static void* json__bucket_resize(json_bucket_t* bucket, void* ptr, int old_count, int new_count)
{
    if (!bucket || new_count <= 0)
    {
        return NULL;
    }

    if (!ptr)
    {
        return json__bucket_extract(bucket, new_count);
    }

    char* begin = (char*)bucket + sizeof(json_bucket_t);
    char* end   = begin + bucket->size * bucket->count;
    if ((char*)ptr + bucket->size * old_count == end && bucket->count + (new_count - old_count) <= bucket->capacity)
    {
        bucket->count += (new_count - old_count);
        return ptr;
    }
    else
    {
        return NULL;
    }
}

/* @funcdef: json__make_value */
static JsonValue* json__make_value(JsonState* state, JsonType type)
{
    if (!state->value_pool || !state->value_pool->head)
    {
        if (state->value_pool && state->value_pool->prev)
        {
            state->value_pool = state->value_pool->prev;
        }
        else
        {
            state->value_pool = json__make_pool(state, state->value_pool, JSON_VALUE_POOL_COUNT, sizeof(JsonValue));
        }

		if (!state->value_pool)
		{
			json__panic(state, type, JSON_ERROR_MEMORY, "Out of memory");
		}
    }
    
    JsonValue* value = (JsonValue*)json__pool_extract(state->value_pool);
    if (value)
    {
		memset(value, 0, sizeof(JsonValue));
		value->type    = type;
		value->boolean = JSON_FALSE;
    }
    else
    {
		json__panic(state, type, JSON_ERROR_MEMORY, "Out of memory");
    }
    return value;
}

/* @funcdef: json__make_state */
static JsonState* json__make_state(const char* json, const JsonSettings* settings)
{
    JsonState* state = (JsonState*)settings->malloc(settings->data, sizeof(JsonState));
    if (state)
    {
		state->next   = NULL;
		
		state->line   = 1;
		state->column = 1;
		state->cursor = 0;
		state->buffer = json;
		state->length = strlen(json);

		state->errmsg = NULL;
		state->errnum = JSON_ERROR_NONE;

		state->value_pool    = NULL;
        state->values_bucket = NULL;
        state->string_bucket = NULL;

        state->settings = *settings;
    }
    return state;
}

/* @funcdef: json__reuse_state */
static JsonState* json__reuse_state(JsonState* state, const char* json, const JsonSettings* settings)
{
    if (state)
    {
        if (state == root_state)
        {
            root_state = state->next;
        }
        else
        {
            JsonState* list = root_state;
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
		state->buffer = json;
		state->errnum = JSON_ERROR_NONE;

        if (state->settings.data != settings->data ||
            state->settings.free != settings->free ||
            state->settings.malloc != settings->malloc)
        {
            json__free_pool(state, state->value_pool);
            json__free_bucket(state, state->values_bucket);
            json__free_bucket(state, state->string_bucket);

		    state->value_pool    = NULL;
            state->values_bucket = NULL;
            state->string_bucket = NULL;

            state->settings.free(state->settings.data, state->errmsg); 
            state->errmsg = NULL;
        }
        else
        {
            if (state->errmsg) state->errmsg[0] = 0;

            while (state->value_pool)
            {
                state->value_pool->head = (void**)(state->value_pool + 1);
                if (state->value_pool->prev)
                {
                    break;
                }
                else
                {
                    state->value_pool = state->value_pool->prev;
                }
            }

            while (state->values_bucket)
            {
                state->values_bucket->count = 0;
                if (state->values_bucket->prev)
                {
                    state->values_bucket = state->values_bucket->prev;
                }
                else
                {
                    break;
                }
            }

            while (state->string_bucket)
            {
                state->string_bucket->count = 0;
                if (state->string_bucket->prev)
                {
                    state->string_bucket = state->string_bucket->prev;
                }
                else
                {
                    break;
                }
            }
        }
    }
    return state;
}

/* @funcdef: json__free_state */
static void json__free_state(JsonState* state)
{
    if (state)
    {
		JsonState* next = state->next;

        json__free_bucket(state, state->values_bucket);
        json__free_bucket(state, state->string_bucket);
		json__free_pool(state, state->value_pool);

		state->settings.free(state->settings.data, state->errmsg);
		state->settings.free(state->settings.data, state);

		json__free_state(next);
    }
}

/* @funcdef: json__is_eof */
static int json__is_eof(JsonState* state)
{
    return state->cursor >= state->length || state->buffer[state->cursor] <= 0;
}

/* @funcdef: json__peek_char */
static int json__peek_char(JsonState* state)
{
    return state->buffer[state->cursor];
}

/* @funcdef: json__next_char */
static int json__next_char(JsonState* state)
{
    if (json__is_eof(state))
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
/* @funcdef: json__make_value */
static int next_line(JsonState* state)
{
    int c = json__peek_char(state);
    while (c > 0 && c != '\n')
    {
		c = json__next_char(state);
    }
    return json__next_char(state);
}
#endif

/* @funcdef: json__skip_space */
static int json__skip_space(JsonState* state)
{
    int c = json__peek_char(state);
    while (c > 0 && isspace(c))
    {
		c = json__next_char(state);
    }
    return c;
}

/* @funcdef: json__match_char */
static int json__match_char(JsonState* state, JsonType type, int c)
{
    if (json__peek_char(state) == c)
    {
		return json__next_char(state);
    }
    else
    {
        json__panic(state, type, JSON_ERROR_UNMATCH, "Expected '%c'", c);
		return -1;
    }
}

/* @funcdef: json__hash */
static int json__hash(void* buf, size_t len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        size_t i = len >> 2;
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
        size_t i = len & 3;
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

static JsonValue* json__parse_array(JsonState* state, JsonValue* value);
static JsonValue* json__parse_single(JsonState* state, JsonValue* value);
static JsonValue* json__parse_object(JsonState* state, JsonValue* value);
static JsonValue* json__parse_number(JsonState* state, JsonValue* value);
static JsonValue* json__parse_string(JsonState* state, JsonValue* value);

/* @funcdef: json__parse_number */
static JsonValue* json__parse_number(JsonState* state, JsonValue* value)
{
    if (json__skip_space(state) < 0)
    {
		return NULL;
    }
    else
    {
		int c = json__peek_char(state);
		int sign = 1;
		
		if (c == '+')
		{
			c = json__next_char(state);
			json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED,
				        "JSON does not support number start with '+'");
		}
		else if (c == '-')
		{
			sign = -1;
			c = json__next_char(state);
		}
		else if (c == '0')
		{
			c = json__next_char(state);
			if (!isspace(c) && !ispunct(c))
			{
				json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED,
                            "JSON does not support number start with '0' (only standalone '0' is accepted)");
			}
		}
		else if (!isdigit(c))
		{
			json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Unexpected '%c'", c);
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
                    json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many 'e' are presented in a <number>");
                }
                else if (dot && numpow == 1)
                {
                    json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED,
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
                    json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Cannot has '.' after 'e' is presented in a <number>");
                }
				else if (dot)
				{
					json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many '.' are presented in a <number>");
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
                    json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "'%c' is presented after digits are presented of exponent part", c);
                }
                else if (expsgn)
                {
                    json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED, "Too many signed characters are presented after 'e'");
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

			c = json__next_char(state);
		}

        if (exp && !expchk)
        {
            json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED,
                        "'e' is presented in number token, but require a digit after 'e' ('%c')", c);
        }
		if (dot && numpow == 1)
		{
			json__panic(state, JSON_NUMBER, JSON_ERROR_UNEXPECTED,
                        "'.' is presented in number token, but require a digit after '.' ('%c')", c);
			return NULL;
		}
		else
		{
			if (!value)
            {
                value = json__make_value(state, JSON_NUMBER);
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

/* @funcdef: json__parse_array */
static JsonValue* json__parse_array(JsonState* state, JsonValue* root)
{
    if (json__skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
	    json__match_char(state, JSON_ARRAY, '[');
	
	    if (!root)
        {
            root = json__make_value(state, JSON_ARRAY);
        }
        else
        {
            root->type = JSON_ARRAY;
        }

	    int            length = 0;
	    JsonValue** values = NULL;
	
	    while (json__skip_space(state) > 0 && json__peek_char(state) != ']')
	    {
	        if (length > 0)
	        {
                json__match_char(state, JSON_ARRAY, ',');
	        }
	    
	        JsonValue* value = json__parse_single(state, NULL);
            
            int   old_size   = sizeof(int) + length * sizeof(JsonValue*);
            int   new_size   = sizeof(int) + (length + 1) * sizeof(JsonValue*);
            void* new_values = json__bucket_resize(state->values_bucket, 
                                             values ? (int*)values - 1 : NULL, 
                                             old_size, 
                                             new_size);

            if (!new_values)
            {
                /* Get from unused buckets (a.k.a reuse json_state_t) */
                while (state->values_bucket && state->values_bucket->prev)
                {
                    state->values_bucket = state->values_bucket->prev;
                    new_values = json__bucket_extract(state->values_bucket, new_size);
                    if (!new_values)
                    {
                        break;
                    }
                }

                if (!new_values)
                {
                    /* Create new buckets */
                    state->values_bucket = json__make_bucket(state, state->values_bucket, JSON_VALUE_BUCKETS, 1);
                    
                    new_values = json__bucket_extract(state->values_bucket, new_size);
                    if (!new_values)
                    {
                        json__panic(state, JSON_ARRAY, JSON_ERROR_MEMORY, "Out of memory when create <array>");
                    }
                    else if (values)
                    {
                        memcpy(new_values, (int*)values - 1, old_size);
                    }
                }
            }

            values = (JsonValue**)((int*)new_values + 1);
	        values[length++] = value;
	    }

	    json__skip_space(state);
	    json__match_char(state, JSON_ARRAY, ']');

        if (values)
        {
            *((int*)values - 1) = length;
        }

	    root->array = values;
	    return root;
    }
}

/* json__parse_single */
static JsonValue* json__parse_single(JsonState* state, JsonValue* value)
{
    if (json__skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
	    int c = json__peek_char(state);
	
	    switch (c)
	    {
	    case '[':
	        return json__parse_array(state, value);
	    
	    case '{':
	        return json__parse_object(state, value);
	    
	    case '"':
	        return json__parse_string(state, value);

	    case '+': case '-': case '0': 
        case '1': case '2': case '3': 
        case '4': case '5': case '6': 
        case '7': case '8': case '9':
	        return json__parse_number(state, value);
	    
        default:
	    {
	        int length = 0;
	        while (c > 0 && isalpha(c))
	        {
                length++;
                c = json__next_char(state);
	        }

	        const char* token = state->buffer + state->cursor - length;
	        if (length == 4 && strncmp(token, "true", 4) == 0)
	        {
                if (!value) value = json__make_value(state, JSON_BOOLEAN);
                else        value->type = JSON_BOOLEAN;
                value->boolean = JSON_TRUE;
                return value;
	        }
	        else if (length == 4 && strncmp(token, "null", 4) == 0)
            {
                return value ? (value->type = JSON_NULL, value) : json__make_value(state, JSON_NULL);
            }
	        else if (length == 5 && strncmp(token, "false", 5) == 0)
	        {
                return value ? (value->type = JSON_BOOLEAN, value->boolean = JSON_FALSE, value) : json__make_value(state, JSON_BOOLEAN);
	        }
	        else
	        {
                char tmp[256];
                tmp[length] = 0;
                while (length--)
                {
                    tmp[length] = token[length]; 
                }

                json__panic(state, JSON_NONE, JSON_ERROR_UNEXPECTED, "Unexpected token '%s'", tmp);
	        }
	    } break;
	    /* END OF SWITCH STATEMENT */
        }

        return NULL;
    }
}

/* @funcdef: json__parse_string */
static JsonValue* json__parse_string(JsonState* state, JsonValue* value)
{
    const int HEADER_SIZE = 2 * sizeof(int);

    if (json__skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
        json__match_char(state, JSON_STRING, '"');

        int   i;
        int   c0, c1;
        int   length = 0;
        char  tmp_buffer[1024];
        char* tmp_string = tmp_buffer;
        int   capacity = sizeof(tmp_buffer);
        while (!json__is_eof(state) && (c0 = json__peek_char(state)) != '"')
        {
            if (length > capacity)
            {
                capacity <<= 1;
                if (tmp_string != tmp_buffer)
                {
                    tmp_string = (char*)malloc(capacity);
                }
                else
                {
                    tmp_string = (char*)realloc(tmp_string, capacity);
                    if (!tmp_string)
                    {
                        json__panic(state, JSON_STRING, JSON_ERROR_MEMORY, "Out of memory when create new <string>");
                        return NULL;
                    }
                }
            }

            if (c0 == '\\')
            {
                c0 = json__next_char(state);
                switch (c0)
                {
                case 'n':
                    tmp_string[length++] = '\n';
                    break;

                case 't':
                    tmp_string[length++] = '\t';
                    break;

                case 'r':
                    tmp_string[length++] = '\r';
                    break;

                case 'b':
                    tmp_string[length++] = '\b';
                    break;

                case '\\':
                    tmp_string[length++] = '\\';
                    break;

                case '"':
                    tmp_string[length++] = '\"';
                    break;
                        
                case 'u':
                    c1 = 0;
                    for (i = 0; i < 4; i++)
                    {
                        if (isxdigit((c0 = json__next_char(state))))
                        {
                            c1 = c1 * 10 + (isdigit(c0) ? c0 - '0' : c0 < 'a' ? c0 - 'A' : c0 - 'a'); 
                        }   
                        else
                        {
                            json__panic(state, JSON_STRING, JSON_ERROR_UNKNOWN, "Expected hexa character in unicode character");
                        }
                    }

                    if (c1 <= 0x7F) 
                    {
                        tmp_string[length++] = c1;
                    }
                    else if (c1 <= 0x7FF) 
                    {
                        tmp_string[length++] = 0xC0 | (c1 >> 6);            /* 110xxxxx */
                        tmp_string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    }
                    else if (c1 <= 0xFFFF) 
                    {
                        tmp_string[length++] = 0xE0 | (c1 >> 12);           /* 1110xxxx */
                        tmp_string[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                        tmp_string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    }
                    else if (c1 <= 0x10FFFF) 
                    {
                        tmp_string[length++] = 0xF0 | (c1 >> 18);           /* 11110xxx */
                        tmp_string[length++] = 0x80 | ((c1 >> 12) & 0x3F);  /* 10xxxxxx */
                        tmp_string[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                        tmp_string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    }
                    break;

                default:
                    json__panic(state, JSON_STRING, JSON_ERROR_UNKNOWN, "Unknown escape character");
                    break;
                }
            }
            else
            {
                switch (c0)
                {
                case '\r':
                case '\n':
                    json__panic(state, JSON_STRING, JSON_ERROR_UNEXPECTED, "Unexpected newline characters '%c'", c0);
                    break;

                default:
                    tmp_string[length++] = c0;
                    break;
                }
            }

            json__next_char(state);
        }
        json__match_char(state, JSON_STRING, '"');

        if (!value)
        {
            value = json__make_value(state, JSON_STRING);
        }
        else        
        {
            value->type = JSON_STRING;
        }

        if (tmp_string)
        {
            tmp_string[length] = 0;

            size_t size   = HEADER_SIZE + ((size_t)length + 1);
            char*  string = (char*)json__bucket_extract(state->string_bucket, size);
            if (!string)
            {
                /* Get from unused buckets */
                while (state->string_bucket && state->string_bucket->prev)
                {
                    state->string_bucket = state->string_bucket->prev;
                    string = (char*)json__bucket_extract(state->string_bucket, capacity);
                    if (string)
                    {
                        break;
                    }
                }

                /* Create new bucket */
                state->string_bucket = json__make_bucket(state, state->string_bucket, JSON_STRING_BUCKETS, 1);
                string = (char*)json__bucket_extract(state->string_bucket, capacity);
                if (!string)
                {
                    json__panic(state, JSON_STRING, JSON_ERROR_MEMORY, "Out of memory when create new <string>");
                    return NULL;
                }
            }

            /* String header */
            ((int*)string)[0] = length;                  
            ((int*)string)[1] = json__hash(tmp_string, length);

            value->string = string + HEADER_SIZE;
#if defined(_MSC_VER) && _MSC_VER >= 1200
            strncpy_s((char*)value->string, length + 1, tmp_string, length);
#else
            strncpy((char*)value->string, tmp_string, length);
#endif

            if (tmp_string != tmp_buffer)
            {
                free(tmp_string);
            }
        }

        return value;
    }
}

/* @funcdef: json__parse_object */
static JsonValue* json__parse_object(JsonState* state, JsonValue* root)
{
    if (json__skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
        json__match_char(state, JSON_OBJECT, '{');

        if (!root)
        {
            root = json__make_value(state, JSON_OBJECT);
        }
        else
        {
            root->type   = JSON_OBJECT;
            root->object = NULL;
        }

        int length = 0;
        while (json__skip_space(state) > 0 && json__peek_char(state) != '}')
        {
            if (length > 0)
            {
                json__match_char(state, JSON_OBJECT, ',');
            }

            JsonValue* token = NULL;
            if (json__skip_space(state) == '"')
            {
                token = json__parse_string(state, NULL);
            }
            else
            {
                json__panic(state, JSON_OBJECT, JSON_ERROR_UNEXPECTED,
                      "Expected <string> for <member-name> of <object>");
            }
            const char* name = token->string;

            json__skip_space(state);
            json__match_char(state, JSON_OBJECT, ':');

            JsonValue* value = json__parse_single(state, token);

            /* Append new pair of value to container */
            int   old_length = length++;
            int   old_size   = sizeof(int) + old_length * sizeof(root->object[0]);
            int   new_size   = sizeof(int) + length * sizeof(root->object[0]);
            void* new_values = json__bucket_resize(state->values_bucket,
                                                   root->object ? (int*)root->object - 1 : NULL,
                                                   old_size, 
                                                   new_size);
            if (!new_values)
            {
                /* Get from unused buckets */
                while (state->values_bucket && state->values_bucket->prev)
                {
                    state->values_bucket = state->values_bucket->prev;
                    new_values = json__bucket_extract(state->values_bucket, length);
                    if (new_values)
                    {
                        break;
                    }
                }

                if (!new_values)
                {
                    /* Create new buffer */
                    state->values_bucket = json__make_bucket(state, state->values_bucket, JSON_VALUE_BUCKETS, 1);
                    
                    /* Continue get new buffer for values */
                    new_values = json__bucket_extract(state->values_bucket, length);
                    if (!new_values)
                    {
                        json__panic(state, JSON_OBJECT, JSON_ERROR_MEMORY, "Out of memory when create <object>");
                    }
                    else if (root->object)
                    {
                        memcpy(new_values, (int*)root->object - 1, old_size);
                    }
                }
            }

            /* When code reach here, new_values should not invalid */
            assert(new_values != NULL && "An error occurred but is not handled");

            /* Well done */
            *((void**)&root->object) = (int*)new_values + 1;
            root->object[old_length].name  = name;
            root->object[old_length].value = value;
        }

        if (root->object)
        {
            *((int*)root->object - 1) = length;
        }

        json__skip_space(state);
        json__match_char(state, JSON_OBJECT, '}');
        return root;
    }
}
         
/* @region: json_parse_in */
static JsonValue* json_parse_in(JsonState* state)
{
    if (!state)
    {
        return NULL;
    }

    if (json__skip_space(state) == '{')
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonValue* value = json__parse_object(state, NULL);

            json__skip_space(state);
            if (!json__is_eof(state))
            {
                json__panic(state, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <object>.");
            }

            return value;
        }
        else
        {
            return NULL;
        }
    }
    else if (json__skip_space(state) == '[')
    {
        if (setjmp(state->errjmp) == 0)
        {
            JsonValue* value = json__parse_array(state, NULL);

            json__skip_space(state);
            if (!json__is_eof(state))
            {
                json__panic(state, JSON_NONE, JSON_ERROR_FORMAT, "JSON is not well-formed. JSON is start with <array>.");
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
        json__set_error(state, JSON_NONE, JSON_ERROR_FORMAT, 
                        "JSON must be starting with '{' or '[', first character is '%c'", 
                        json__peek_char(state));
        return NULL;
    }
}

/* @funcdef: json_parse */
JsonValue* json_parse(const char* json, JsonState** out_state)
{
    JsonSettings settings;
    settings.data   = NULL;
    settings.free   = json__free;
    settings.malloc = json__malloc;

    return json_parse_ex(json, &settings, out_state);
}

/* @funcdef: json_parse_ex */
JsonValue* json_parse_ex(const char* json, const JsonSettings* settings, JsonState** out_state)
{
    JsonState* state = out_state && *out_state ? json__reuse_state(*out_state, json, settings) : json__make_state(json, settings);
    JsonValue* value = json_parse_in(state);

    if (value)
    {
        if (out_state)
        {
            *out_state = state;
        }
        else
        {
            if (state)
            {
                state->next = root_state;
                root_state  = state;
            }
        }
    }
    else
    {
        if (out_state)
        {
            *out_state = state;
        }
        else
        {
            json__free_state(state);
        }
    }

    return value;
}

/* @funcdef: json_release */
void json_release(JsonState* state)
{
    if (state)
    {
        json__free_state(state);
    }
    else
    {
        json__free_state(root_state);
        root_state = NULL;
    }
}

/* @funcdef: json_get_errno */
JsonError json_get_errno(const JsonState* state)
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

/* @funcdef: json_get_error */
const char* json_get_error(const JsonState* state)
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

/* @funcdef: json_length */
int json_length(const JsonValue* x)
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

/* @funcdef: json_equals */
JsonBoolean json_equals(const JsonValue* a, const JsonValue* b)
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
        if ((n = json_length(a)) == json_length(b))
        {
            for (i = 0; i < n; i++)
            {
                if (!json_equals(a->array[i], b->array[i]))
                {
                    return JSON_FALSE;
                }
            }
        }
        return JSON_TRUE;

    case JSON_OBJECT:
        if ((n = json_length(a)) == json_length(b))
        {
            for (i = 0; i < n; i++)
            {
                const char* str0 = a->object[i].name;
                const char* str1 = a->object[i].name;
                if (((int*)str0 - 2)[1] != ((int*)str1 - 2)[1] || strcmp(str0, str1) == 0)
                {
                    return JSON_FALSE;
                }

                if (!json_equals(a->object[i].value, b->object[i].value))
                {
                    return JSON_FALSE;
                }
            }
        }
        return JSON_TRUE;

    case JSON_STRING:
        return ((int*)a->string - 2)[1] == ((int*)b->string - 2)[1] && strcmp(a->string, b->string) == 0;
    }

    return JSON_FALSE;
}

/* @funcdef: json_find */
JsonValue* json_find(const JsonValue* obj, const char* name)
{
    if (obj && obj->type == JSON_OBJECT)
    {
        int i, n;
        int hash = json__hash((void*)name, strlen(name));
        for (i = 0, n = json_length(obj); i < n; i++)
        {
            const char* str = obj->object[i].name;
            if (hash == ((int*)str - 2)[1] && strcmp(str, name) == 0)
            {
                return obj->object[i].value;
            }
        }
    }

    return NULL;
}

/* @funcdef: json_write */
void json_write(const JsonValue* value, FILE* out)
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
            for (i = 0, n = json_length(value); i < n; i++)
            {
                json_write(value->array[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JSON_OBJECT:
            fprintf(out, "{");
            for (i = 0, n = json_length(value); i < n; i++)
            {
                fprintf(out, "\"%s\" : ", value->object[i].name);
                json_write(value->object[i].value, out);
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

/* @funcdef: json_print */
void json_print(const JsonValue* value, FILE* out)
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
            for (i = 0, n = json_length(value); i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                json_print(value->array[i], out);
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
            for (i = 0, n = json_length(value); i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                fprintf(out, "\"%s\" : ", value->object[i].name);
                json_print(value->object[i].value, out);
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


#endif /* JSON_IMPL */

