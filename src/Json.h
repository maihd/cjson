/******************************************************
 * Simple json state written in ANSI C
 *
 * @author: MaiHD
 * @license: Public domain
 * @copyright: MaiHD @ ${HOME}, 2018 - 2019
 ******************************************************/

#pragma once

#ifndef JSON_API
#define JSON_API
#endif

#ifdef __cplusplus
extern "C" {
#elif !defined(__bool_true_false_are_defined)
typedef int bool;
enum
{
    true  = 1,
    false = 0,
};
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

typedef struct JsonState        JsonState;
typedef struct JsonValue        JsonValue;
typedef struct JsonAllocator    JsonAllocator;
typedef struct JsonObjectEntry  JsonObjectEntry;

JSON_API JsonValue*    JsonParse(const char* json, int jsonLength, JsonState** outState);
JSON_API JsonValue*    JsonParseEx(const char* json, int jsonLength, const JsonAllocator* allocator, JsonState** outState);

JSON_API void          JsonRelease(JsonState* state);

JSON_API JsonError     JsonGetError(const JsonState* state);
JSON_API const char*   JsonGetErrorString(const JsonState* state);

JSON_API bool          JsonEquals(const JsonValue* a, const JsonValue* b);

JSON_API int           JsonHash(const void* buffer, int length);

JSON_API JsonValue*    JsonFind(const JsonValue* x, const char* name);
JSON_API JsonValue*    JsonFindWithHash(const JsonValue* x, int hash);

struct JsonValue
{
    JsonType type;                      /* Type of value: number, boolean, string, array, object    */
    int      length;                    /* Length of value, should use for literal type only        */
    union
    {
        double              number;
        bool                boolean;

        const char*         string;

        JsonValue*          array;

        JsonObjectEntry*    object;
    };
};

struct JsonObjectEntry
{
    const char*       name;
    int               nameLength;

    int               hash;
    struct JsonValue  value;
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
