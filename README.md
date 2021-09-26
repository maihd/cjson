# Introduction ![Build Status](https://github.com/maihd/cjson/actions/workflows/unit-tests.yml/badge.svg) [![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](http://unlicense.org/)
Simple JSON parser written in C99

## Category
1. [Features](#features)
2. [Build Instructions](#build)
3. [Examples](#examples)
4. [API](#api)

## Features
- Simple, small and easy to use, integration.
- C99 for portability.
- Single responsibility: when work with json in C, we only need parse the json string to C value. Json stringify is easy to implement in C.
- No memory allocations, linear memory layout, cache friendly.
- Well-tested eith some real-world examples.

## Build
```
# Run REPL test
make test

# Run unit tests
make unit_test

# Make single header libary (when you want to make sure Json.h/JsonEx.h is the newest version)
make lib
```

## Examples
Belove code from Json_TokenTest.c:
```C
#include "Json.h"
#include "JsonUtils.h" // for JsonPrint

int main(int argc, char* argv[])
{
    signal(SIGINT, _sighandler);
    
    printf("JSON token testing prompt\n");
    printf("Type '.exit' to exit\n");
    
    char input[1024];
    int allocatorCapacity = 1024 * 1024; // 1MB temp buffer
    void* allocatorBuffer = malloc(allocatorCapacity);

    while (1)
    {
	    if (setjmp(jmpenv) == 0)
	    {
	        printf("> ");
	        fgets(input, sizeof(input), stdin);

	        const char* json = strtrim_fast(input);
	        if (strcmp(json, ".exit") == 0)
	        {
                break;
	        }
	        else
            {
                Json* value;
                JsonError error = JsonParse(json, strlen(json), JsonFlags_NoStrictTopLevel, allocatorBuffer, allocatorCapacity, &value);
                if (error.code != JsonError_None)
                {
                    value = NULL;
                    printf("[ERROR]: %s\n", error.message);
                }
                else
                {
                    JsonPrint(value, stdout); printf("\n");
                }
	        }
	    }
    }

    free(allocatorBuffer);
    return 0;
}
```

## API
```C
enum JsonType
{
    JsonType_Null,
    JsonType_Array,
    JsonType_Object,
    JsonType_Number,
    JsonType_String,
    JsonType_Boolean,
};

typedef enum JsonErrorCode
{
    JsonError_None,

    JsonError_WrongFormat,
    JsonError_UnmatchToken,
    JsonError_UnknownToken,
    JsonError_UnexpectedToken,
    JsonError_UnsupportedToken,

    JsonError_OutOfMemory,
    JsonError_InvalidValue,
    JsonError_InternalFatal,
} JsonErrorCode;

typedef struct JsonError
{
    JsonErrorCode   code;
    const char*     message;
} JsonError;

typedef enum JsonFlags
{
    JsonFlags_None              = 0,
    JsonFlags_SupportComment    = 1 << 0,
    JsonFlags_NoStrictTopLevel  = 1 << 1,
} JsonFlags;

struct Json
{
    JsonType                type;
    int                     length;
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

static const Json JSON_NULL     = { JsonType_Null   , 0        };
static const Json JSON_TRUE     = { JsonType_Boolean, 0, true  };
static const Json JSON_FALSE    = { JsonType_Boolean, 0, false };

JSON_API JsonError  JsonParse(const char* jsonCode, int32_t jsonCodeLength, JsonFlags flags, void* buffer, int32_t bufferSize, Json* result);
JSON_API bool       JsonEquals(const Json a, const Json b);
JSON_API bool       JsonFind(const Json parent, const char* name, Json* result);
```
