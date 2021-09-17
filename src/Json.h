#pragma once

#ifndef JSON_API
#define JSON_API
#endif

/* START OF EXTERN "C" */
#ifdef __cplusplus
extern "C" {
#endif

/* Define boolean type if needed */
#if !defined(__cplusplus)
#include <stdbool.h>
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
typedef enum JsonErrorCode
{
    JsonError_None,

    /* Parsing error */

    JsonError_WrongFormat,
    JsonError_UnmatchToken,
    JsonError_UnknownToken,
    JsonError_UnexpectedToken,
    JsonError_UnsupportedToken,

    /* Runtime error */

    JsonError_OutOfMemory,
    JsonError_InvalidValue,
    JsonError_InternalFatal,

} JsonErrorCode;

typedef struct JsonError
{
    JsonErrorCode   code;
    const char*     message;
} JsonError;

/**
 * Json parse flags
 */
typedef enum JsonFlags
{
    JsonFlags_None              = 0,
    JsonFlags_SupportComment    = 1 << 0,
    JsonFlags_NoStrictTopLevel  = 1 << 1,
} JsonFlags;

typedef struct Json             Json;
typedef struct JsonAllocator    JsonAllocator;
typedef struct JsonObjectMember JsonObjectMember;

JSON_API JsonError      Json_parse(const char* jsonCode, int jsonCodeLength, JsonFlags flags, Json** result);
JSON_API JsonError      Json_parseEx(const char* jsonCode, int jsonCodeLength, JsonAllocator allocator, JsonFlags flags, Json** result);

JSON_API void           Json_release(Json* rootValue);

JSON_API bool           Json_equals(const Json* a, const Json* b);

JSON_API Json*          Json_find(const Json* x, const char* name);

struct Json
{
    JsonType                type;       /* Type of value: number, boolean, string, array, object    */
    int                     length;     /* Length of value, always 1 on primitive types             */
    union
    {
        double              number;
        bool                boolean;

        const char*         string;

        Json*               array;

        JsonObjectMember*   object;
    };
};

struct JsonObjectMember
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
