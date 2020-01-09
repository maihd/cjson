# Introduction [![Build Status](https://travis-ci.org/maihd/json.svg?branch=master)](https://travis-ci.org/maihd/json)
Simple JSON parser written in C99

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
Belove code from json_test.cc:
```C
int main(int argc, char* argv[])
{
    signal(SIGINT, _sighandler);
    
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
                Json* value = JsonParse(json, strlen(json));
                if (JsonGetError(value) != JSON_ERROR_NONE)
                {
                    value = NULL;
                    printf("[ERROR]: %s\n", JsonGetErrorMessage(value));
                }
                else
                {
                    JsonPrint(value, stdout); printf("\n");
                }
                JsonRelease(value);
	        }
	    }
    }

    /* JsonRelease(NULL) for release all memory, if there is leak */
    JsonRelease(NULL);
    
    return 0;
}
```

## API
```C
enum JsonType
{
    JSON_NONE,
    JSON_NULL,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_NUMBER,
    JSON_STRING,
    JSON_BOOLEAN,
};

enum JsonError
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

struct JsonAllocator
{
    void* data;                                 // Your memory buffer
    void* (*malloc)(void* data, size_t size);   // Your memory allocate function
    void  (*free)(void* data, void* pointer);   // Your memory deallocate function
};

Json* JsonParse(const char* json_code, int jsonCodeLength);
Json* JsonParse(const char* json_code, int jsonCodeLength, JsonAllocator settings);
// return root value, save to get root memory

void JsonRelease(Json* root); // root = NULL to remove all leak memory

JsonError   JsonGetError(const Json* root); // Get error number of [given state] or [last state] (when state = NULL) 
const char* JsonGerErrorMessage(const Json* root); // Get error string of [given state] or [last state] (when state = NULL)

```

## Metadata
1. Author: MaiHD
2. License: Public domain
3. Copyright: MaiHD 2018 - 2020
3. Tools : Emacs, VSCode, GnuMake, Visual Studio 2017 Community
