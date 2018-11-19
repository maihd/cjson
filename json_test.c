#if 0

#include <signal.h>
#include <setjmp.h>

#define JSON_IMPL
#include "json.h"

static jmp_buf jmpenv;

static void _sighandler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\n");
        printf("Type '.exit' to exit\n");
        longjmp(jmpenv, 1);
    }
}

static char* strtrim_fast(char* str)
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

#endif