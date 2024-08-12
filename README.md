<img align="left" src="https://upload.wikimedia.org/wikipedia/commons/thumb/c/c9/JSON_vector_logo.svg/1024px-JSON_vector_logo.svg.png" width="60" height="60" />

# cjson ![Build Status](https://github.com/maihd/cjson/actions/workflows/unit-tests.yml/badge.svg) [![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](http://unlicense.org/)
Simple JSON parser written in C99


## Table of Contents
- [Features](#features)
- [Examples](#examples)
- [API](#api)
- [Build Instructions](#build-instructions)
- [FAQs](#faqs)


## Features
- Simple, small and easy to use, integration.
- Writing in C99: simple, small, portability.
- Robust error handling, no pointer-as-object.
- Single responsibility: when work with json in C, we only need parse the json string to C value. Json stringify is easy to implement in C.
- No memory allocations, linear memory layout, cache friendly.
- Well-tested with some real-world examples.
- Visual Studio Natvis.
- String as UTF8.


## Limits
- No scientific number
- Not use state machine for parsing
- Not the best, the fastest json parser
- Parsing require a preallocated large buffers (cannot detect buffer size requirement), buffer not an allocator (I dont often store the json value as persitent, just temp for parsing levels, game data)
- Use longjmp to handling error, jump from error point to parse function call, which is have 2 disadvantages:
    - longjmp may works different over platforms
    - longjmp depends on stdlib, not work on freestanding platforms


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

## Build Instructions
This project is a single-header-only library, user is free to choose build system. This instructions are for development only.
```
# Run REPL test
make test

# Run unit tests
make unit_test

# Make single header libary (when you want to make sure Json.h/JsonEx.h is the newest version)
make lib
```

## FAQs
### Why another json parser?
When I first read an article about common mistake of c libraries are do much dynamic allocations and have no custom allocators. So I create this projects to learn to make good library in C.

### You said custom allocators, but I donot find one?
In the first version there is a custom allocator interface. But after the long run, I found the memory of Json was throw away at one, so dynamic allocators are expensive for that. Now we just given a temporary buffer to parser, and there is a linear allocator in internal. So no dynamic allocations.

### Where stringify/serialize functions?
It easy to write an JsonStringify version, but the real problem in C is not that simple. You need to create Json value, create Json may need memory, so we need to care about memory allocation. That headache! Fortunately, C is static type language, so we can easily convert out data structure/object to json easily base on its types. See example below:
```C
typedef struct Entity
{
    uint32_t    id;
    const char* name;
} Entity;

const char* JsonifyEntity(Entity entity)
{
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), 
        "{\"id\":%u,\"name\":\"%s\"}", entity.id, entity.name
    );
    return buffer;
}
```

### I don't like CamelCase!!!
Just rename, update, change what you not like with your code editor.

### What about licenses
This repo use UNLICENSE but the source code you generate from `make` can be your license of choice.
