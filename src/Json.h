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

#ifdef __cplusplus
extern "C" {
#elif !defined(__bool_true_false_are_defined)
typedef enum { false, true } bool;
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

    JSON_ERROR_NO_VALUE,

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

typedef struct Json             Json;
typedef struct JsonAllocator    JsonAllocator;
typedef struct JsonObjectEntry  JsonObjectEntry;

JSON_API Json*          JsonParse(const char* jsonCode, int jsonCodeLength);
JSON_API Json*          JsonParseEx(const char* jsonCode, int jsonCodeLength, JsonAllocator allocator);

JSON_API void           JsonRelease(Json* rootValue);

JSON_API JsonError      JsonGetError(const Json* rootValue);
JSON_API const char*    JsonGetErrorMessage(const Json* rootValue);

JSON_API bool           JsonEquals(const Json* a, const Json* b);

JSON_API Json*          JsonFind(const Json* x, const char* name);

struct Json
{
    JsonType type;                      /* Type of value: number, boolean, string, array, object    */
    int      length;                    /* Length of value, should use for literal type only        */
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
    const char*         name;
    struct Json         value;
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
