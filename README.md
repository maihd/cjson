# Introduction [![Build Status](https://travis-ci.org/maihd/json.svg?branch=master)](https://travis-ci.org/maihd/json) [![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](http://unlicense.org/)
Simple JSON parser written in C99

> This projects is out-of-maintain, reasons:<br/>
> 1. The problem with the API design, the functions Json_getErrorXXX required root value of json, but when parsing failed, the Json_parse return null.<br/>
> 2. Internal parsing routines use many dynamic allocations, which should only use a simple linear allocator.<br/>
> 3. In C, we just only need json parser only, but the beginning I thought this library use should dynamic create of json values, support JSON.stringify in C version. The JSON.stringify is easily implements with just some `sprintf` of a conrete target data structure.<br/>
> 4. DevOps problems: Travis CI stop support open source, no static analytics, no FAGs, no TDD in the first place.<br/>
> 5. Start new project: [cjson_parser](https://githut.com/maihd/cjson_parser). I development my new style to use C as an scripting language, tools, automation.

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
                Json* value = Json_parse(json, strlen(json));
                if (Json_getError(value) != JsonError_None)
                {
                    value = NULL;
                    printf("[ERROR]: %s\n", Json_getErrorMessage(value));
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

enum JsonError
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
};

struct Json
{
    JsonType type;
    int      length;
    union 
    {
        double          number;
        bool            boolean;   
        const char*     string;
        Json*           array;  
        struct {
            const char* name;
            Json        value;
        } object;
    };
};

// Memory alignment is always alignof(Json)
struct JsonAllocator
{
    void* data;                                 // Your memory buffer
    void* (*alloc)(void* data, int size);       // Your memory allocate function
    void  (*free)(void* data, void* pointer);   // Your memory deallocate function
};

Json* Json_parse(const char* jsonCode, int jsonCodeLength);
Json* Json_parseEx(const char* jsonCode, int jsonCodeLength, JsonAllocator allocator);
// return root value, save to get root memory

void Json_release(Json* root); // root = NULL to remove all leak memory

JsonError   Json_getError(const Json* root); // Get error number of [given state] or [last state] (when state = NULL) 
const char* Json_getErrorMessage(const Json* root); // Get error string of [given state] or [last state] (when state = NULL)

```