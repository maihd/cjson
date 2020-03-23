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

/* START OF EXTERN "C" */
#ifdef __cplusplus
extern "C" {
#endif

/* Define boolean type if needed */
#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
typedef char bool;
enum { false = 0, true = 1 };
#endif

/**
 * JSON type of json value
 */
typedef enum JsonType
{
    JsonType_Null,
    JsonType_Array,
    JsonType_Object,
    JsonType_Number,
    JsonType_String,
    JsonType_Boolean,
} JsonType;

/**
 * JSON error code
 */
typedef enum JsonError
{
    JsonError_None,

    JsonError_NoValue,

    /* Parsing error */

    JsonError_WrongFormat,
    JsonError_UnmatchToken,
    JsonError_UnknownToken,
    JsonError_UnexpectedToken,
    JsonError_UnsupportedToken,

    /* Runtime error */

    JsonError_OutOfMemory,
    JsonError_InternalFatal,
} JsonError;

typedef struct Json             Json;
typedef struct JsonAllocator    JsonAllocator;
typedef struct JsonObjectEntry  JsonObjectEntry;

JSON_API Json*          Json_parse(const char* jsonCode, int jsonCodeLength);
JSON_API Json*          Json_parseEx(const char* jsonCode, int jsonCodeLength, JsonAllocator allocator);

JSON_API void           Json_release(Json* rootValue);

JSON_API JsonError      Json_getError(const Json* rootValue);
JSON_API const char*    Json_getErrorMessage(const Json* rootValue);

JSON_API bool           Json_equals(const Json* a, const Json* b);

JSON_API Json*          Json_find(const Json* x, const char* name);

struct Json
{
    JsonType type;                      /* Type of value: number, boolean, string, array, object    */
    int      length;                    /* Length of value, always 1 on primitive types             */
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
    const char* name;
    Json        value;
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
