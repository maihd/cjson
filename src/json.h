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

#if !defined(NDEBUG) || !defined(JSON_OBJECT_NO_KEYNAME)
#define JSON_OBJECT_KEYNAME
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

#ifdef __cplusplus
extern "C" {
#endif

struct FILE;

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

typedef struct JsonParser JsonParser;
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
    JSON_TRUE = 1,
    JSON_FALSE = 0,
} JsonBoolean;
#endif

typedef struct
{
    void* data;
    void* (*alloc)(void* data, int size);
    void  (*free)(void* data, void* ptr);
} JsonAllocator;

JSON_API extern const JsonValue JSON_VALUE_NONE;

JSON_API JsonValue*    JsonParse(const char* json, JsonParser** parser);
JSON_API JsonValue*    JsonParseEx(const char* json, const JsonAllocator* allocator, JsonParser** parser);

JSON_API void          JsonRelease(JsonParser* parser);

JSON_API JsonError     JsonGetError(const JsonParser* parser);
JSON_API const char*   JsonGetErrorString(const JsonParser* parser);

JSON_API void          JsonPrint(const JsonValue* value, FILE* out);
JSON_API void          JsonWrite(const JsonValue* value, FILE* out);

JSON_API int           JsonLength(const JsonValue* x);
JSON_API JsonBoolean   JsonEquals(const JsonValue* a, const JsonValue* b);

JSON_API int           JsonHash(const void* buffer, int length);

JSON_API JsonValue*    JsonFind(const JsonValue* x, const char* name);
JSON_API JsonValue*    JsonFindWithHash(const JsonValue* x, int hash);

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
            int               hash;
            struct JsonValue* value;

#ifdef JSON_OBJECT_KEYNAME
            const char*       name;
#endif
        }*object;
    };

#ifdef __cplusplus
public: // @region: Constructors
    JSON_INLINE JsonValue()
        : type(JSON_NONE)
    {
    }

    JSON_INLINE ~JsonValue()
    {
        // SHOULD BE EMPTY
        // Memory are managed by json_state_t
    }

public: // @region: Indexor
    JSON_INLINE const JsonValue& operator[] (int index) const
    {
        if (type != JSON_ARRAY || index < 0 || index > JsonLength(this))
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
        JsonValue* value = JsonFind(this, name);
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

    JSON_INLINE operator double() const
    {
        return number;
    }

    JSON_INLINE operator bool() const
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