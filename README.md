# Introduction [![Build Status](https://travis-ci.org/maihd/json.svg?branch=master)](https://travis-ci.org/maihd/json)
Simple JSON parser written in ANSI C++

## API
```C++
struct json::State; // Implicit structure, manage parsing context

struct json::Value
{
    json::Type type;
    union 
    {
        double          number;
        bool            boolean;   
        const char*     string;
        json::Value**   array;  
        struct {...}    object;
    };

    int length() const;
    // Get length of value, available [ string, array, object ]

    const json::Value& json::find(const char* name);
    // Find value with of member with name

    static bool json::equals(const json::Value& a, const json::Value& b);
    // Deep compare two JSON values
};

bool operator==(const json::Value& a, const json::Value& b);
bool operator!=(const json::Value& a, const json::Value& b);

struct json::Settings
{
    void* data;                                 // Your memory buffer
    void* (*malloc)(void* data, size_t size);   // Your memory allocate function
    void  (*free)(void* data, void* pointer);   // Your memory deallocate function
};

const json::Value& json::parse(const char* json_code, json::State** out_state);
const json::Value& json::parse(const char* json_code, json::Settings* settings, json::State** out_state);
// 1. json_code : the JSON content from JSON's source (from memory, from file)
// 2. out_state : the JSON state for parsing json code, contain usage memory (can be NULL, library will hold it), can be reuse by give it an valid state
// 3. settings  : the parsing settings, and only custom memory management by now

void json::release(json::State* state);
// Release state, when state is null, library will implicit remove states that it hold

json::Error json::get_errno(const json::State* state); // Get error number of [given state] or [last state] (when state = NULL) 
const char* json::get_error(const json::State* state); // Get error string of [given state] or [last state] (when state = NULL)

// Find more details, or helper functions in json.h
```

## Examples
Belove code from json_test.cc:
```C++
#include <signal.h>
#include <setjmp.h>

#define JSON_IMPL
#include "json.h"

static jmp_buf jmpenv;
static void _sighandler(int sig);
static char* strtrim_fast(char* str);

int main(int argc, char* argv[])
{
    signal(SIGINT, _sighandler);
    
    printf("JSON token testing prompt\n");
    printf("Type '.exit' to exit\n");
    
    char input[1024];
    json::State* state = NULL;
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
                auto value = json::parse(json, &state);
                if (json::get_errno(state) != json::Error::None)
                {
                    printf("[ERROR]: %s\n", json::get_error(state));
                }
                else
                {
                    json::print(value, stdout); printf("\n");
                }
	        }
	    }
    }

    /* json::release(NULL) for release all memory if you don't catch the json::State */
    json::release(state);
    
    return 0;
}
```

## Metadata
1. Author: MaiHD
2. License: Public domain
3. Copyright: MaiHD 2018 - 2019
3. Tools : Emacs, VSCode, Visual Studio 2017 Community
