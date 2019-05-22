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

#ifdef __cplusplus
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

typedef struct JsonParser    JsonParser;
typedef struct JsonValue     JsonValue;
typedef struct JsonAllocator JsonAllocator;

/**
 * JSON boolean data type
 */
typedef enum JsonBoolean
{
    JSON_TRUE  = 1,
    JSON_FALSE = 0,
} JsonBoolean;

JSON_API JsonValue*    JsonParse(const char* json, JsonParser** parser);
JSON_API JsonValue*    JsonParseEx(const char* json, const JsonAllocator* allocator, JsonParser** parser);

JSON_API void          JsonRelease(JsonParser* parser);

JSON_API JsonError     JsonGetError(const JsonParser* parser);
JSON_API const char*   JsonGetErrorString(const JsonParser* parser);

JSON_API int           JsonLength(const JsonValue* x);
JSON_API JsonBoolean   JsonEquals(const JsonValue* a, const JsonValue* b);

JSON_API int           JsonHash(const void* buffer, int length);

JSON_API JsonValue*    JsonFind(const JsonValue* x, const char* name);
JSON_API JsonValue*    JsonFindWithHash(const JsonValue* x, int hash);

typedef struct JsonObjectEntry
{
    int               hash;
    struct JsonValue* value;

#ifdef JSON_OBJECT_KEYNAME
    const char*       name;
#endif
} JsonObjectEntry;

/**
 * JSON value
 */
struct JsonValue
{
    JsonType type;
    union
    {
        double              number;
        JsonBoolean         boolean;

        const char*         string;

        JsonValue**         array;

        JsonObjectEntry*    object;
    };
};

struct JsonAllocator
{
    void* data;
    void* (*alloc)(void* data, int size);
    void(*free)(void* data, void* ptr);
};

/* END OF EXTERN "C" */
#ifdef __cplusplus
}
#endif
/* * */

/* END OF __JSON_H__ */
#endif /* __JSON_H__ */