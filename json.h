#ifndef __JSON_H__
#define __JSON_H__

#ifndef JSON_API
#define JSON_API
#endif

#ifdef __cplusplus
#include <string.h>
extern "C" {
#endif

typedef enum
{
    JSON_NONE,
    JSON_NULL,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_NUMBER,
    JSON_STRING,
    JSON_BOOLEAN,
} json_type_t;

typedef enum
{
    JSON_ERROR_NONE,
    JSON_ERROR_MEMORY,
    JSON_ERROR_UNMATCH,
    JSON_ERROR_UNKNOWN,
    JSON_ERROR_UNEXPECTED,
} json_error_t;

typedef enum
{
	JSON_TRUE  = 1,
	JSON_FALSE = 0,
} json_bool_t;

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
			int length;
			struct
			{
				struct json_value* name;
				struct json_value* value;
			}  *values;
		} object;

		struct
		{
			int                 length;
			struct json_value** values;
		} array;
    };

    #ifdef __cplusplus
public:
    inline json_value()
		: parent(0)
		, type(JSON_NONE)
		, boolean(JSON_FALSE)
	{	
	}

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
		if (type != JSON_OBJECT)
		{
			return JSON_VALUE_NONE;
		}
		else
		{
			for (int i = 0, n = object.length; i < n; i++)
			{
				if (strcmp(object.values[i].name, name) == 0)
				{
					return *object.values[i].value;
				}
			}

			return JSON_VALUE_NONE;
		}	
	}

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
		return !!boolean;
	}
    #endif /* __cplusplus */
} json_value_t;
    
static const json_value_t JSON_VALUE_NONE; /* auto fill with zero */

typedef struct json_state json_state_t;

json_value_t* json_parse(const char* json, json_state_t** state);
void          json_release(json_value_t* value, json_state_t* state);
json_error_t  json_get_errno(const json_state_t* state);
const char*   json_get_error(const json_state_t* state);

#ifdef __cplusplus
}
#endif
    
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
    struct json_pool* next;

    void** head;
} json_pool_t;

struct json_state
{
    struct json_state* next;
    json_pool_t* value_pool;

    //json_bucket_t* values;
    //json_bucket_t* string;
    
    int line;
    int column;
    int cursor;
    
    const char* buffer;
    
    json_error_t errnum;
    char*        errmsg;
    jmp_buf      errjmp;
};

static json_state_t* root_state = NULL;

#if __GNUC__
__attribute__((noreturn))
#elif defined(_MSC_VER)
__declspec(noreturn)
#endif
static void croak(json_state_t* state, json_error_t code, const char* fmt, ...)
{
    const int errmsg_size = 1024;
    
    if (state->errmsg == NULL)
    {
	state->errmsg = malloc(errmsg_size);
    }

    state->errnum = code;

    va_list varg;
    va_start(varg, fmt);
    sprintf(state->errmsg, fmt, varg);
    va_end(varg);
    
    longjmp(state->errjmp, code);
}

static json_pool_t* make_pool(json_pool_t* next, int count, int size)
{
    if (count <= 0 || size <= 0)
    {
	return NULL;
    }

    int pool_size = count * (sizeof(void*) + size);
    json_pool_t* pool = (json_pool_t*)malloc(sizeof(json_pool_t) + pool_size);
    if (pool)
    {
	pool->next = next;
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

static void free_pool(json_pool_t* pool)
{
    if (pool)
    {
	free_pool(pool->next);
 	free(pool);
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

static json_value_t* make_value(json_state_t* state, int type)
{
    if (!state->value_pool || !state->value_pool->head)
    {
	state->value_pool = make_pool(state->value_pool,
				      64, sizeof(json_value_t));
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

static void free_value(json_state_t* state, json_value_t* value)
{
    if (value)
    {
	int i, n;
	switch (value->type)
	{
	case JSON_ARRAY:
	    for (i = 0, n = value->array.length; i < n; i++)
	    {
		free_value(state, value->array.values[i]);
	    }
	    free(value->array.values);
	    break;

	case JSON_OBJECT:
	    for (i = 0, n = value->object.length; i < n; i++)
	    {
		free_value(state, value->object.values[i].name);
		free_value(state, value->object.values[i].value);
	    }
	    free(value->object.values);
	    break;
	    
	case JSON_STRING:
	    free(value->string.buffer);
	    break;
	}

	pool_collect(state->value_pool, value);
    }
}

static json_state_t* make_state(const char* json)
{
    json_state_t* state = (json_state_t*)malloc(sizeof(json_state_t));
    if (state)
    {
	state->next   = NULL;
	
	state->line   = 1;
	state->column = 1;
	state->cursor = 0;
	state->buffer = json;

	state->errmsg = NULL;
	state->errnum = JSON_ERROR_NONE;

	state->value_pool = NULL;
    }
    return state;
}

static void free_state(json_state_t* state)
{
    if (state)
    {
	json_state_t* next = state->next;

	free_pool(state->value_pool);
	free(state->errmsg);
	free(state);

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

static json_value_t* parse_single(json_state_t* state);
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

static json_value_t* parse_string(json_state_t* state)
{
    if (skip_space(state) < 0)
    {
	return NULL;
    }
    else
    {
	match_char(state, '"');

	int length = 0;
	while (!is_eof(state) && peek_char(state) != '"')
	{
	    length++;
	    next_char(state);
	}
	
	match_char(state, '"');

	char* string = (char*)malloc(length + 1);
	string[length] = 0;
	memcpy(string, state->buffer + state->cursor - length - 1, length);

	json_value_t* value  = make_value(state, JSON_STRING);
	value->string.length = length;
	value->string.buffer = string;
	return value;
    }
}

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

	int            length = 0;
	json_value_t** values = 0;
	
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

	    root->object.values =
		realloc(root->object.values,
			(++length) * sizeof(root->object.values[0]));
	    
	    root->object.values[length - 1].name  = name;
	    root->object.values[length - 1].value = value;
	}

	root->object.length = length;

	skip_space(state);
	match_char(state, '}');
	return root;
    }
}

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
	    values = realloc(values, (++length) * sizeof(json_value_t*));
	    values[length - 1] = value;
	}

	skip_space(state);
	match_char(state, ']');

	root->array.length = length;
	root->array.values = values;
	return root;
    }
}

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

	case '+':
	case '-':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
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

json_value_t* json_parse(const char* json, json_state_t** out_state)
{
    json_state_t* state = make_state(json);

    if (skip_space(state) == '{')
    {
	if (setjmp(state->errjmp) == 0)
	{
	    json_value_t* value = parse_object(state);

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
	    
	    return value;
	}
    }
        
    if (out_state)
    {
	*out_state = state;
    }
    else
    {
	free_state(state);
    }

    return NULL;
}

void json_release(json_value_t* value, json_state_t* state)
{
    if (state)
    {
	free_value(state, value);
	free_state(state);
    }
    else
    {
	free_value(root_state, value);
	free_state(root_state);
	root_state = NULL;
    }
}

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
#endif
