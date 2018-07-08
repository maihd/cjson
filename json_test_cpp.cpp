#if 1

#include <signal.h>
#include <setjmp.h>

#define JSON_IMPL
#include "json.h"

static jmp_buf jmpenv;

void _sighandler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\n");
        printf("Type '.exit' to exit\n");
        longjmp(jmpenv, 1);
    }
}

char* strtrim_fast(char* str)
{
    while (isspace(*str))
    {
        str++;
    }

    char* ptr = str;
    while (*ptr > 0)      { ptr++; }; ptr--;
    while (isspace(*ptr)) { ptr--; }; ptr[1] = 0;

    return str;
}

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
                json::state_t* state;
                json::value_t* value = json::parse(json, &state);
	    
                if (json::get_errno(state) != JSON_ERROR_NONE)
                {
                    value = NULL;
                    printf("[ERROR]: %s\n", json::get_error(state));
                }
                else
                {
                    json::print(value, stdout); printf("\n");
                }

                /* json::release(NULL) for release all memory if you don't catch the json_state_t */
                json::release(state);
	        }
	    }
    }
    
    return 0;
}

#endif 0