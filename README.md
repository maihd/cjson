# Introduction [![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](http://unlicense.org/)
Simple JSON parser written in C99

> Problems:<br/>
> 1. The API should be more useful, the implement should be more simple, but now it's complicated and hard to changed.</br>
> 2. The problem with the API design, the functions Json_getErrorXXX required root value of json, but when parsing failed, the Json_parse return null. (Fixed now)<br/>
> 3. Internal parsing routines use many dynamic allocations, which should only use a simple linear allocator.<br/>
> 4. In C, we just only need json parser only, but the beginning I thought this library use should dynamic create of json values, support JSON.stringify in C version. The JSON.stringify is easily implements with just some `sprintf` of a conrete target data structure.<br/>
> 5. DevOps problems: Travis CI stop support open source, no static analytics, no FAGs, no TDD in the first place.

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
Belove code from json_test.c:
```C
#include "Json.h"
#include "JsonEx.h" // for Json_print

int main(int argc, char* argv[])
{
    //signal(SIGINT, _sighandler);
    
    printf("JSON token testing prompt\n");
    printf("Type '.exit' to exit\n");
    
    char input[1024];
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
                JsonError error = Json_parse(json, strlen(json));
                if (error.code != JsonError_None)
                {
                    value = NULL;
                    printf("[ERROR]: %s\n", error.message);
                }
                else
                {
                    Json_print(value, stdout); printf("\n");
                }
                Json_release(value);
	        }
	    }
    }

    /* Json_release(NULL) for release all memory, if there is leak */
    Json_release(NULL);
    
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

// Memory alignment is always alignof(Json)
struct JsonAllocator
{
    void* data;                                 // Your memory buffer
    void* (*alloc)(void* data, int size);       // Your memory allocate function
    void  (*free)(void* data, void* pointer);   // Your memory deallocate function
};

JsonError               Json_parse(const char* jsonCode, int jsonCodeLength, JsonFlags flags, Json** result);
JsonError               Json_parseEx(const char* jsonCode, int jsonCodeLength, JsonAllocator allocator, JsonFlags flags, Json** result);

void                    Json_release(Json* root); // root = NULL to remove all leak memory

JSON_API bool           Json_equals(const Json* a, const Json* b);

JSON_API const Json*    Json_find(const Json* x, const char* name);
```
