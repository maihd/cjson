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

typedef struct json_state json_state_t;
typedef struct json_value json_value_t;

/**
 * JSON boolean data type
 */
#ifdef __cplusplus
typedef bool json_bool_t;
#define JSON_TRUE  true
#define JSON_FALSE false
#else
typedef enum json_bool_t
{
	JSON_TRUE  = 1,
	JSON_FALSE = 0,
} json_bool_t;
#endif

typedef struct
{
    void* data;
    void* (*malloc)(void* data, size_t size);
    void  (*free)(void* data, void* pointer);
} json_settings_t;

JSON_API extern const json_value_t JSON_VALUE_NONE;

JSON_API json_value_t* json_parse(const char* json, json_state_t** state);
JSON_API json_value_t* json_parse_ex(const char* json, const json_settings_t* settings, json_state_t** state);

JSON_API void          json_release(json_state_t* state);

JSON_API json_error_t  json_get_errno(const json_state_t* state);
JSON_API const char*   json_get_error(const json_state_t* state);

JSON_API void          json_print(const json_value_t* value, FILE* out);
JSON_API void          json_write(const json_value_t* value, FILE* out);

JSON_API int           json_length(const json_value_t* x);
JSON_API json_bool_t   json_equals(const json_value_t* a, const json_value_t* b);
JSON_API json_value_t* json_find(const json_value_t* obj, const char* name);

/**
 * JSON value
 */
struct json_value
{
    json_type_t type;
    union
    {
		double      number;
		json_bool_t boolean;

		const char* string;

        struct json_value** array;

        struct
        {
            const char*        name;
            struct json_value* value;
        }* object;
    };

#ifdef __cplusplus
public: // @region: Constructors
    JSON_INLINE json_value()
	{	
        memset(this, 0, sizeof(*this));
	}

    JSON_INLINE ~json_value()
    {
        // SHOULD BE EMPTY
        // Memory are managed by json_state_t
    }

public: // @region: Indexor
	JSON_INLINE const json_value& operator[] (int index) const
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

	JSON_INLINE const json_value& operator[] (const char* name) const
	{
		json_value_t* value = json_find(this, name);
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
