# Introduction [![Build Status](https://travis-ci.org/maihd/json.svg?branch=master)](https://travis-ci.org/maihd/json)
Simple JSON parser written in ANSI C/C++

## API
```C
struct json_state_t; // Implicit structure, manage parsing context

struct json_value_t
{
    json_type_t type;
    union 
    {
        double          number;
        json_bool_t     boolean;   
        const char*     string;
        json_value_t**  array;  
        struct {...}    object;
    };
};

struct json_settings_t
{
    void* data;                                 // Your memory buffer
    void* (*malloc)(void* data, size_t size);   // Your memory allocate function
    void  (*free)(void* data, void* pointer);   // Your memory deallocate function
};

json_value_t* json_parse(const char* json_code, json_state_t** out_state);
json_value_t* json_parse_ex(const char* json_code, json_settings_t* settings, json_state_t** out_state);
// 1. json_code : the JSON content from JSON's source (from memory, from file)
// 2. out_state : the JSON state for parsing json code, contain usage memory (can be NULL, library will hold it), can be reuse by give it an valid state
// 3. settings  : the parsing settings, and only custom memory management by now

void json_release(json_state_t* state);
// Release state, when state is null, library will implicit remove states that it hold

json_error_t json_get_errno(const json_state_t* state); // Get error number of [given state] or [last state] (when state = NULL) 
const char*  json_get_error(const json_state_t* state); // Get error string of [given state] or [last state] (when state = NULL)

void json_print(const json_value_t* value, FILE* out); 
// Stringify output to file, more readable + size bigger  than json_write

void json_write(const json_value_t* value, FILE* out); 
// Stringify output to file, less readable + size smaller than json_print

json_bool_t json_equals(const json_value_t* a, const json_value_t* b);
// Depth compare two JSON values

json_value_t* json_find(const json_value_t* obj, const char* name);
// Get value that is contained by `obj`

int json_length(const json_value_t* value);
// Get length of json value, value must be string, array or object

// Find more details, or helper functions in json.h
```

## Examples
Belove code from json_test.c:
```C
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
    json_state_t* state = NULL;
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
                json_value_t* value = json_parse(json, &state);
	    
                if (json_get_errno(state) != JSON_ERROR_NONE)
                {
                    value = NULL;
                    printf("[ERROR]: %s\n", json_get_error(state));
                }
                else
                {
                    json_print(value, stdout); printf("\n");
                }
	        }
	    }
    }

    /* json_release(NULL) for release all memory if you don't catch the json_state_t */
    json_release(state);
    
    return 0;
}
```

## Metadata
1. Author: MaiHD
2. License: Public domain
3. Copyright: MaiHD 2018 - 2019
3. Tools : Emacs, VSCode, Visual Studio 2017 Community
