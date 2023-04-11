#ifndef __JSON_H__
#define __JSON_H__

#ifndef JSON_API
#define JSON_API
#endif

#ifdef __cplusplus
#define JSON_CONST constexpr
#else
#define JSON_CONST static const
#endif

/* START OF EXTERN "C" */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// -------------------------------------------------------------------
// Types
// -------------------------------------------------------------------

/// JSON type of json value
typedef enum JsonType
{
    JsonType_Null,
    JsonType_Array,
    JsonType_Object,
    JsonType_Number,
    JsonType_String,
    JsonType_Boolean,
} JsonType;

/// JSON error code
typedef enum JsonError
{
    JsonError_None,

    /* Parsing error */

    JsonError_WrongFormat,
    JsonError_UnmatchToken,
    JsonError_UnknownToken,
    JsonError_UnexpectedToken,
    JsonError_UnsupportedToken,

    /* Finding error */
    JsonError_MissingField,
    JsonError_WrongType,

    /* Runtime error */

    JsonError_OutOfMemory,
    JsonError_InvalidValue,
    JsonError_InternalFatal,

} JsonError;

/// JSON error information
typedef struct JsonResult
{
    JsonError       error;
    const char*     message;

    //JsonParser*     parser;
    int32_t         memoryUsage;
} JsonResult;

/// Json parse flags
typedef enum JsonParseFlags
{
    JsonParseFlags_None             = 0,
    JsonParseFlags_SupportComment   = 1 << 0,
    JsonParseFlags_NoStrictTopLevel = 1 << 1,

    JsonParseFlags_Default          = JsonParseFlags_None,
} JsonParseFlags;

typedef struct Json             Json;
//typedef struct JsonParser       JsonParser;
typedef struct JsonObjectMember JsonObjectMember;

struct Json
{
    JsonType                type;       // Type of value: number, boolean, string, array, object
    int32_t                 length;     // Length of value, always 1 on primitive types, UTF8 string length
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
    const char*             name;
    Json                    value;
};

// -------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

JSON_CONST Json JSON_NULL     = { JsonType_Null   , 0             };
JSON_CONST Json JSON_TRUE     = { JsonType_Boolean, 0, { true  }  };
JSON_CONST Json JSON_FALSE    = { JsonType_Boolean, 0, { false }  };

#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wmissing-field-initializers"
#endif

// -------------------------------------------------------------------
// Main functions
// -------------------------------------------------------------------

JSON_API JsonResult JsonParse(const char* jsonCode, int32_t jsonCodeLength, JsonParseFlags flags, void* buffer, int32_t bufferSize, Json* outValue);
//JSON_API JsonResult JsonContinueParse(JsonParser* parser, Json* outValue);

JSON_API bool       JsonEquals(const Json a, const Json b);

JSON_API bool       JsonFind(const Json parent, const char* name, Json* outResult);
JSON_API JsonError  JsonFindWithType(const Json parent, const char* name, JsonType type, Json* outResult);

static inline bool JsonValidType(const Json json)
{
    return json.type >= JsonType_Null && json.type <= JsonType_Boolean;
}

/* END OF EXTERN "C" */
#ifdef __cplusplus
}
#endif
/* * */

#endif // __JSON_H__

//! LEAVE AN EMPTY LINE HERE, REQUIRE BY GCC/G++
