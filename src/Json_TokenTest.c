#include <signal.h>
#include <setjmp.h>
#include <string.h>

#define JSON_IMPL
#include "../Json.h"

#define JSON_EX_IMPL
#include "./JsonEx.h"

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
    JsonParser* state = NULL;
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
                JsonValue* value = JsonParse(json, &state);
	    
                if (JsonGetError(state) != JSON_ERROR_NONE)
                {
                    value = NULL;
                    printf("[ERROR]: %s\n", JsonGetErrorString(state));
                }
                else
                {
                    JsonPrint(value, stdout); printf("\n");
                }
	        }
	    }
    }

    /* JsonRelease(NULL) for release all memory if you don't catch the json_state_t */
    JsonRelease(state);
    
    return 0;
}