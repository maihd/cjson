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
typedef enum json_type
{
    JSON_NONE,
    JSON_NULL,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_NUMBER,
    JSON_STRING,
    JSON_BOOLEAN,
} json_type_t;

/**
 * JSON error code
 */
typedef enum json_error
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
} json_error_t;

/**
 * JSON boolean data type
 */
#ifdef __cplusplus
typedef bool json_bool_t;
#define JSON_TRUE  true
#define JSON_FALSE false
#else
typedef enum
{
	JSON_TRUE  = 1,
	JSON_FALSE = 0,
} json_bool_t;
#endif

JSON_API extern const struct json_value JSON_VALUE_NONE;

/**
 * JSON value
 */
typedef struct json_value
{
    json_type_t type;
    union
    {
		double      number;
		json_bool_t boolean;

		struct
		{
			int   length;
			char* buffer;
		} string;

		struct
		{
			int                 length;
			struct json_value** values;
		} array;

		struct
		{
			int length;
			struct
			{
				struct json_value* name;
				struct json_value* value;
			}*  values;
		} object;
    };

#ifdef __cplusplus
public: // @region: Constructors
    inline json_value()
	{	
        memset(this, 0, sizeof(*this));
	}

    inline ~json_value()
    {
        // SHOULD BE EMPTY
        // Memory are managed by json_state_t
    }

public: // @region: Indexor
	inline const json_value& operator[] (int index) const
	{
		if (type != JSON_ARRAY || index < 0 || index > array.length)
		{
			return JSON_VALUE_NONE;
		}
		else
		{
			return *array.values[index];
		}	
	}

	inline const json_value& operator[] (const char* name) const
	{
		if (type == JSON_OBJECT)
		{
			for (int i = 0, n = object.length; i < n; i++)
			{
				if (strcmp(object.values[i].name->string.buffer, name) == 0)
				{
					return *object.values[i].value;
				}
			}
		}	

        return JSON_VALUE_NONE;
	}

public: // @region: Conversion
	inline operator const char* () const
	{
		if (type == JSON_STRING)
		{
			return string.buffer;
		}
		else
		{
			return "";
		}
	}

	inline operator double () const
	{
		return number;
	}

	inline operator bool () const
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
} json_value_t;

typedef struct json_state json_state_t;
typedef struct
{
    void* data;
    void* (*malloc)(void* data, size_t size);
    void  (*free)(void* data, void* pointer);
} json_settings_t;

JSON_API json_value_t* json_parse(const char* json, json_state_t** state);
JSON_API json_value_t* json_parse_ex(const char* json, const json_settings_t* settings, json_state_t** state);

JSON_API void          json_release(json_state_t* state);

JSON_API json_value_t* json_get_value(const json_value_t* obj, const char* name);

JSON_API json_error_t  json_get_errno(const json_state_t* state);
JSON_API const char*   json_get_error(const json_state_t* state);

JSON_API void          json_print(const json_value_t* value, FILE* out);
JSON_API void          json_write(const json_value_t* value, FILE* out);

JSON_API json_bool_t   json_equals(const json_value_t* a, const json_value_t* b);

/* END OF EXTERN "C" */
#ifdef __cplusplus
}
#endif
/* * */

/* END OF __JSON_H__ */
#endif /* __JSON_H__ */

#ifdef JSON_IMPL

#ifdef _MSC_VER
#  ifndef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS
#  endif
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

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

    int size;
    int count;
    int capacity;
} json_bucket_t;

struct json_state
{
    struct json_state* next;
    json_pool_t* value_pool;

    json_bucket_t* values_bucket;
    json_bucket_t* string_bucket;
    
    int line;
    int column;
    int cursor;
    
    const char* buffer;
    
    json_error_t errnum;
    char*        errmsg;
    jmp_buf      errjmp;

    json_settings_t settings; /* Runtime settings */
};

static json_state_t* root_state = NULL;
const struct json_value JSON_VALUE_NONE;

static void* def_malloc(void* data, size_t size)
{
    (void)data;
    return malloc(size);
}

static void def_free(void* data, void* pointer)
{
    (void)data;
    free(pointer);
}

/* @funcdef: set_error_valist */
static void set_error_valist(json_state_t* state, json_error_t code, const char* fmt, va_list valist)
{
    const int errmsg_size = 1024;

    state->errnum = code;
    if (state->errmsg == NULL)
    {
        state->errmsg = (char*)state->settings.malloc(state->settings.data, errmsg_size);
    }

#if defined(_MSC_VER) && _MSC_VER >= 1200
    sprintf_s(state->errmsg, errmsg_size, fmt, valist);
#else
    sprintf(state->errmsg, fmt, valist);
#endif
}

/* @funcdef: set_error */
static void set_error(json_state_t* state, json_error_t code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    set_error_valist(state, code, fmt, varg);
    va_end(varg);
}

/* funcdef: croak */
static void croak(json_state_t* state, json_error_t code, const char* fmt, ...)
{
    va_list varg;
    va_start(varg, fmt);
    set_error_valist(state, code, fmt, varg);
    va_end(varg);

    longjmp(state->errjmp, code);
}

static json_pool_t* make_pool(json_state_t* state, json_pool_t* prev, int count, int size)
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

static void free_pool(json_state_t* state, json_pool_t* pool)
{
    if (pool)
    {
		free_pool(state, pool->prev);
		state->settings.free(state->settings.data, pool);
    }
}

static void* pool_extract(json_pool_t* pool)
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

static void pool_collect(json_pool_t* pool, void* ptr)
{
    if (ptr)
    {
		void** node = (void**)((char*)ptr - sizeof(void*));
		*node = pool->head;
		pool->head = node;
    }
}

static json_bucket_t* make_bucket(json_state_t* state, json_bucket_t* prev, int count, int size)
{
    if (count <= 0 || size <= 0)
    {
        return 0;
    }

    json_bucket_t* bucket = 
    (json_bucket_t*)state->settings.malloc(state->settings.data,
                                           sizeof(json_bucket_t) + count * size);
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

static void free_bucket(json_state_t* state, json_bucket_t* bucket)
{
    if (bucket)
    {
        free_bucket(state, bucket->next);
        state->settings.free(state->settings.data, bucket);
    }
}

static void* bucket_extract(json_bucket_t* bucket, int count)
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

static void* bucket_resize(json_bucket_t* bucket, void* ptr, int old_count, int new_count)
{
    if (!bucket || old_count <= 0 || new_count <= 0)
    {
        return NULL;
    }

    char* begin = (char*)bucket + sizeof(json_bucket_t);
    char* end   = begin + bucket->count * bucket->count;
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

static json_value_t* make_value(json_state_t* state, json_type_t type)
{
    if (!state->value_pool || !state->value_pool->head)
    {
        if (state->value_pool && state->value_pool->prev)
        {
            state->value_pool = state->value_pool->prev;
        }
        else
        {
            state->value_pool = make_pool(state, state->value_pool, 64, sizeof(json_value_t));
        }

		if (!state->value_pool)
		{
			croak(state, JSON_ERROR_MEMORY, "Out of memory");
		}
    }
    
    json_value_t* value = (json_value_t*)pool_extract(state->value_pool);
    if (value)
    {
		memset(value, 0, sizeof(json_value_t));
		value->type    = type;
		value->boolean = JSON_FALSE;
    }
    else
    {
		croak(state, JSON_ERROR_MEMORY, "Out of memory");
    }
    return value;
}

static json_state_t* make_state(const char* json, const json_settings_t* settings)
{
    json_state_t* state = (json_state_t*)settings->malloc(settings->data, sizeof(json_state_t));
    if (state)
    {
		state->next   = NULL;
		
		state->line   = 1;
		state->column = 1;
		state->cursor = 0;
		state->buffer = json;

		state->errmsg = NULL;
		state->errnum = JSON_ERROR_NONE;

		state->value_pool    = NULL;
        state->values_bucket = NULL;
        state->string_bucket = NULL;

        state->settings = *settings;
    }
    return state;
}

static json_state_t* reuse_state(json_state_t* state, const char* json, const json_settings_t* settings)
{
    if (state)
    {
        if (state == root_state)
        {
            root_state = state->next;
        }
        else
        {
            json_state_t* list = root_state;
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
            free_pool(state, state->value_pool);
            free_bucket(state, state->values_bucket);
            free_bucket(state, state->string_bucket);

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

static void free_state(json_state_t* state)
{
    if (state)
    {
		json_state_t* next = state->next;

        free_bucket(state, state->values_bucket);
        free_bucket(state, state->string_bucket);
		free_pool(state, state->value_pool);

		state->settings.free(state->settings.data, state->errmsg);
		state->settings.free(state->settings.data, state);

		free_state(next);
    }
}

static int is_eof(json_state_t* state)
{
    return state->buffer[state->cursor] <= 0;
}

static int peek_char(json_state_t* state)
{
    return state->buffer[state->cursor];
}

static int next_char(json_state_t* state)
{
    if (is_eof(state))
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

static int next_line(json_state_t* state)
{
    int c = peek_char(state);
    while (c > 0 && c != '\n')
    {
		c = next_char(state);
    }
    return next_char(state);
}

static int skip_space(json_state_t* state)
{
    int c = peek_char(state);
    while (c > 0 && isspace(c))
    {
		c = next_char(state);
    }
    return c;
}

static int match_char(json_state_t* state, int c)
{
    if (peek_char(state) == c)
    {
		return next_char(state);
    }
    else
    {
		croak(state, JSON_ERROR_UNMATCH, "Expected '%c'", c);
		return -1;
    }
}

/* All parse functions declaration */

static json_value_t* parse_array(json_state_t* state);
static json_value_t* parse_single(json_state_t* state);
static json_value_t* parse_object(json_state_t* state);
static json_value_t* parse_number(json_state_t* state);
static json_value_t* parse_string(json_state_t* state);

/* @funcdef: parse_number */
static json_value_t* parse_number(json_state_t* state)
{
    if (skip_space(state) < 0)
    {
		return NULL;
    }
    else
    {
		int c = peek_char(state);
		int sign = 1;
		
		if (c == '+')
		{
			c = next_char(state);
			croak(state, JSON_ERROR_UNEXPECTED,
				  "JSON does not support number start with '+'");
		}
		else if (c == '-')
		{
			sign = -1;
			c = next_char(state);
		}
		else if (c == '0')
		{
			c = next_char(state);
			if (!isspace(c) && !ispunct(c))
			{
				croak(state, JSON_ERROR_UNEXPECTED,
					  "JSON does not support number start with '0'"
					  " (only standalone '0' is accepted)");
			}
		}
		else if (!isdigit(c))
		{
			croak(state, JSON_ERROR_UNEXPECTED, "Unexpected '%c'", c);
		}

		int    dot    = 0;
		int    dotchk = 1;
		int    numpow = 1;
		double number = 0;

		while (c > 0)
		{
			if (c == '.')
			{
				if (dot)
				{
					croak(state, JSON_ERROR_UNEXPECTED,
					      "Too many '.' are presented");
				}
				if (!dotchk)
				{
					croak(state, JSON_ERROR_UNEXPECTED, "Unexpected '%c'", c);
				}
				else
				{
					dot    = 1;
					dotchk = 0;
					numpow = 1;
				}
			}
			else if (!isdigit(c))
			{
				break;
			}
			else
			{
				dotchk = 1;
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

			c = next_char(state);
		}

		if (dot && !dotchk)
		{
			croak(state, JSON_ERROR_UNEXPECTED,
                  "'.' is presented in number token, "
			      "but require a digit after '.' ('%c')", c);
			return NULL;
		}
		else
		{
			json_value_t* value = make_value(state, JSON_NUMBER);
			value->number = sign * number;
			return value;
		}
    }
}

/* @funcdef: parse_array */
static json_value_t* parse_array(json_state_t* state)
{
    if (skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
	    match_char(state, '[');
	
	    json_value_t* root = make_value(state, JSON_ARRAY);

	    int            length = 0;
	    json_value_t** values = 0;
	
	    while (skip_space(state) > 0 && peek_char(state) != ']')
	    {
	        if (length > 0)
	        {
                match_char(state, ',');
	        }
	    
	        json_value_t* value = parse_single(state);
            
            int old_length = length;
            void* new_values = bucket_resize(state->values_bucket, values, old_length, ++length);
            while (!new_values)
            {
                /* Get from unused buckets */
                while (state->values_bucket && state->values_bucket->prev)
                {
                    state->values_bucket = state->values_bucket->prev;
                    new_values = bucket_extract(state->values_bucket, length);
                    if (new_values)
                    {
                        break;
                    }
                }

                /* Create new buckets */
                state->values_bucket = make_bucket(state, state->values_bucket, 128, sizeof(json_value_t*));
                
                new_values = bucket_extract(state->values_bucket, length);
                if (!new_values)
                {
                    croak(state, JSON_ERROR_MEMORY, "Out of memory when create array");
                    return NULL;
                }
                else
                {
                    memcpy(new_values, values, (length - 1) * sizeof(json_value_t));
                }
                break;
            }

            values = (json_value_t**)new_values;
	        values[length - 1] = value;
	    }

	    skip_space(state);
	    match_char(state, ']');

	    root->array.length = length;
	    root->array.values = values;
	    return root;
    }
}

/* parse_single */
static json_value_t* parse_single(json_state_t* state)
{
    if (skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
	    int c = peek_char(state);
	
	    switch (c)
	    {
	    case '[':
	        return parse_array(state);
	    
	    case '{':
	        return parse_object(state);
	    
	    case '"':
	        return parse_string(state);

	    case '+': case '-': case '0': 
        case '1': case '2': case '3': 
        case '4': case '5': case '6': 
        case '7': case '8': case '9':
	        return parse_number(state);
	    
        default:
	    {
	        int length = 0;
	        while (c > 0 && isalpha(c))
	        {
                length++;
                c = next_char(state);
	        }

	        const char* token = state->buffer + state->cursor - length;
	        if (length == 4 && strncmp(token, "true", 4) == 0)
	        {
                json_value_t* value = make_value(state, JSON_BOOLEAN);
                value->boolean = JSON_TRUE;
	        }
	        else if (length == 4 && strncmp(token, "null", 4) == 0)
            {
                return make_value(state, JSON_NULL);
            }
	        else if (length == 5 && strncmp(token, "false", 5) == 0)
	        {
                return make_value(state, JSON_BOOLEAN);
	        }
	        else
	        {
                croak(state, JSON_ERROR_UNEXPECTED, "Unexpected token '%c'", c);
	        }
	    } break;
	    /* END OF SWITCH STATEMENT */
        }

        return NULL;
    }
}

/* @funcdef: parse_string */
static json_value_t* parse_string(json_state_t* state)
{
    if (skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
        match_char(state, '"');

        int i;
        int length = 0;
        int c0, c1;
        char temp_string[1024];
        while (!is_eof(state) && (c0 = peek_char(state)) != '"')
        {
            if (c0 == '\\')
            {
                c0 = next_char(state);
                switch (c0)
                {
                case 'n':
                    temp_string[length++] = '\n';
                    break;

                case 't':
                    temp_string[length++] = '\t';
                    break;

                case 'r':
                    temp_string[length++] = '\r';
                    break;

                case 'b':
                    temp_string[length++] = '\b';
                    break;

                case '\\':
                    temp_string[length++] = '\\';
                    break;

                case '"':
                    temp_string[length++] = '\"';
                    break;
                        
                case 'u':
                    c1 = 0;
                    for (i = 0; i < 4; i++)
                    {
                        if (isxdigit((c0 = next_char(state))))
                        {
                            c1 = c1 * 10 + (isdigit(c0) ? c0 - '0' : c0 < 'a' ? c0 - 'A' : c0 - 'a'); 
                        }   
                        else
                        {
                            croak(state, JSON_ERROR_UNKNOWN, "Expected hexa character in unicode character");
                        }
                    }

                    if (c1 <= 0x7F) 
                    {
                        temp_string[length++] = c1;
                    }
                    else if (c1 <= 0x7FF) 
                    {
                        temp_string[length++] = 0xC0 | (c1 >> 6);            /* 110xxxxx */
                        temp_string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    }
                    else if (c1 <= 0xFFFF) 
                    {
                        temp_string[length++] = 0xE0 | (c1 >> 12);           /* 1110xxxx */
                        temp_string[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                        temp_string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    }
                    else if (c1 <= 0x10FFFF) 
                    {
                        temp_string[length++] = 0xF0 | (c1 >> 18);           /* 11110xxx */
                        temp_string[length++] = 0x80 | ((c1 >> 12) & 0x3F);  /* 10xxxxxx */
                        temp_string[length++] = 0x80 | ((c1 >> 6) & 0x3F);   /* 10xxxxxx */
                        temp_string[length++] = 0x80 | (c1 & 0x3F);          /* 10xxxxxx */
                    }

                    break;

                default:
                    croak(state, JSON_ERROR_UNKNOWN, "Unknown escape character");
                }
            }
            else
            {
                temp_string[length++] = c0;
            }
            next_char(state);
        }
        temp_string[length] = 0;
        match_char(state, '"');

        char* string = (char*)bucket_extract(state->string_bucket, length + 1);
        if (!string)
        {
            /* Get from unused buckets */
            while (state->string_bucket && state->string_bucket->prev)
            {
                state->string_bucket = state->string_bucket->prev;
                string = (char*)bucket_extract(state->string_bucket, length);
                if (string)
                {
                    break;
                }
            }

            /* Create new bucket */
            state->string_bucket = make_bucket(state, state->string_bucket, 4096, sizeof(char)); /* 4096 equal default memory page size */
            string = (char*)bucket_extract(state->string_bucket, length + 1);
            if (!string)
            {
                croak(state, JSON_ERROR_MEMORY, "Out of memory when create new string");
                return NULL;
            }
        }
        string[length] = 0;
        memcpy(string, temp_string, length);

        json_value_t* value = make_value(state, JSON_STRING);
        value->string.length = length;
        value->string.buffer = string;
        return value;
    }
}

/* @funcdef: parse_object */
static json_value_t* parse_object(json_state_t* state)
{
    if (skip_space(state) < 0)
    {
        return NULL;
    }
    else
    {
        match_char(state, '{');

        json_value_t* root = make_value(state, JSON_OBJECT);

        int length = 0;
        while (skip_space(state) > 0 && peek_char(state) != '}')
        {
            if (length > 0)
            {
                match_char(state, ',');
            }

            json_value_t* name = NULL;
            if (skip_space(state) == '"')
            {
                name = parse_string(state);
            }
            else
            {
                croak(state, JSON_ERROR_UNEXPECTED,
                      "Expected string for name of field of object");
            }

            skip_space(state);
            match_char(state, ':');

            json_value_t* value = parse_single(state);

            /* Append new pair of value to container */
            int old_length = length;
            void* new_values = bucket_resize(state->values_bucket,
                                             root->object.values,
                                             old_length, ++length);
            if (!new_values)
            {
                /* Get from unused buckets */
                while (state->values_bucket && state->values_bucket->prev)
                {
                    state->values_bucket = state->values_bucket->prev;
                    new_values = bucket_extract(state->values_bucket, length);
                    if (new_values)
                    {
                        break;
                    }
                }

                /* Create new buffer */
                state->values_bucket = make_bucket(state, state->values_bucket, 128, sizeof(json_value_t*));
                
                /* Continue get new buffer for values */
                new_values = bucket_extract(state->values_bucket, length);
                if (!new_values)
                {
                    croak(state, JSON_ERROR_MEMORY, "Out of memory when create object");
                    return NULL;
                }
                else
                {
                    memcpy(new_values, root->object.values, (length - 1) * 2 * sizeof(json_value_t));
                    *((void**)&root->object.values) = new_values;
                }
            }

            /* When code reach here, new_values should not invalid */
            assert(new_values != NULL && "An error occurred but is not handled");

            /* Well done */
            *((void**)&root->object.values) = new_values;
            root->object.values[length - 1].name  = name;
            root->object.values[length - 1].value = value;
        }

        root->object.length = length;

        skip_space(state);
        match_char(state, '}');
        return root;
    }
}
         
/* @region: json_parse_in */
static json_value_t* json_parse_in(json_state_t* state)
{
    if (!state)
    {
        return NULL;
    }

    if (skip_space(state) == '{')
    {
        if (setjmp(state->errjmp) == 0)
        {
            json_value_t* value = parse_object(state);
            return value;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        set_error(state, JSON_ERROR_FORMAT, 
                  "JSON must be starting with '{', first character is '%c'", 
                  peek_char(state));
        return NULL;
    }
}

/* @funcdef: json_parse */
json_value_t* json_parse(const char* json, json_state_t** out_state)
{
    json_settings_t settings;
    settings.data   = NULL;
    settings.free   = def_free;
    settings.malloc = def_malloc;

    return json_parse_ex(json, &settings, out_state);
}

/* @funcdef: json_parse_ex */
json_value_t* json_parse_ex(const char* json, const json_settings_t* settings, json_state_t** out_state)
{
    json_state_t* state = out_state && *out_state ? reuse_state(*out_state, json, settings) : make_state(json, settings);
    json_value_t* value = json_parse_in(state);

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
            free_state(state);
        }
    }

    return value;
}

/* @funcdef: json_release */
void json_release(json_state_t* state)
{
    if (state)
    {
        free_state(state);
    }
    else
    {
        free_state(root_state);
        root_state = NULL;
    }
}

/* @funcdef: json_get_errno */
json_error_t json_get_errno(const json_state_t* state)
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
const char* json_get_error(const json_state_t* state)
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

json_bool_t json_equals(const json_value_t* a, const json_value_t* b)
{
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
        return a->array.values == b->array.values;

    case JSON_OBJECT:
        return a->object.values == b->object.values;

    case JSON_STRING:
        return a->string.buffer == b->string.buffer;
    }

    return JSON_FALSE;
}

json_value_t* json_get_value(const json_value_t* obj, const char* name)
{
    if (obj && obj->type == JSON_OBJECT)
    {
        int i, n;
        for (i = 0, n = obj->object.length; i < n; i++)
        {
            if (strcmp(obj->object.values[i].name->string.buffer, name) == 0)
            {
                return obj->object.values[i].value;
            }
        }
    }

    return NULL;
}

/* @funcdef: json_write */
void json_write(const json_value_t* value, FILE* out)
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
            fprintf(out, "\"%s\"", value->string.buffer);
            break;

        case JSON_ARRAY:
            fprintf(out, "[");
            for (i = 0, n = value->array.length; i < n; i++)
            {
                json_print(value->array.values[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JSON_OBJECT:
            fprintf(out, "{");
            for (i = 0, n = value->object.length; i < n; i++)
            {
                json_print(value->object.values[i].name, out);
                fprintf(out, " : ");
                json_print(value->object.values[i].value, out);
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
void json_print(const json_value_t* value, FILE* out)
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
            fprintf(out, "\"%s\"", value->string.buffer);
            break;

        case JSON_ARRAY:
            fprintf(out, "[\n");

            indent++;
            for (i = 0, n = value->array.length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fprintf(out, " ");
                }

                json_print(value->array.values[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }
            fprintf(out, "]");
            break;

        case JSON_OBJECT:
            fprintf(out, "{\n");

            indent++;
            for (i = 0, n = value->object.length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fprintf(out, " ");
                }

                json_print(value->object.values[i].name, out);
                fprintf(out, " : ");
                json_print(value->object.values[i].value, out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }
            fprintf(out, "}");
            break;

        case JSON_NONE:
        default:
            break;
        }
    }
}

/* END OF JSON_IMPL */
#endif /* JSON_IMPL */
