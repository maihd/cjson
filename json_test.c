#ifdef _MSC_VER
#  ifndef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS
#  endif
#endif

#include <signal.h>

#define JSON_IMPL
#include "json.h"

void _sighandler(int sig)
{
    if (sig == SIGINT)
    {
	printf("\n");
	printf("Type '.exit' to exit\n");
	printf("> ");
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

void json_print(json_value_t* value)
{
    if (value)
    {
	int i, n;
	static int indent = 0;
	
	switch (value->type)
	{
	case JSON_NULL:
	    printf("null");
	    break;

	case JSON_NUMBER:
	    printf("%lf", value->number);
	    break;

	case JSON_BOOLEAN:
	    printf("%s", value->boolean ? "true" : "false");
	    break;

	case JSON_STRING:
	    printf("\"%s\"", value->string.buffer);
	    break;

	case JSON_ARRAY:
	    for (i = 0, n = indent * 4; i < n; i++)
	    {
		printf(" ");
	    }
	    printf("[\n");
	    
	    indent++;
	    for (i = 0, n = value->array.length; i < n; i++)
	    {
		int j, m;
		for (j = 0, m = indent * 4; j < m; j++)
		{
		    printf(" ");
		}
		
		json_print(value->array.values[i]);
		if (i < n - 1)
		{
		    printf(",");
		}
		printf("\n");
	    }
	    indent--;
	    
	    for (i = 0, n = indent * 4; i < n; i++)
	    {
		printf(" ");
	    }
	    printf("]");
	    break;

	case JSON_OBJECT:
	    for (i = 0, n = indent * 4; i < n; i++)
	    {
		printf(" ");
	    }
	    printf("{\n");
	    
	    indent++;
	    for (i = 0, n = value->object.length; i < n; i++)
	    {
		int j, m;
		for (j = 0, m = indent * 4; j < m; j++)
		{
		    printf(" ");
		}
		
		json_print(value->object.values[i].name);
		printf(" : ");
		json_print(value->object.values[i].value);
		if (i < n - 1)
		{
		    printf(",");
		}
		printf("\n");
	    }
	    indent--;

	    for (i = 0, n = indent * 4; i < n; i++)
	    {
		printf(" ");
	    }
	    printf("}");
	    break;
	    
	case JSON_NONE:
	default:
	    break;
	}
    }
}

int main(int argc, char* argv[])
{
    signal(SIGINT, _sighandler);
    
    printf("JSON token testing prompt\n");
    printf("Type '.exit' to exit\n");
    
    char input[1024];
    while (1)
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
	    json_state_t* state;
	    json_value_t* value = json_parse(json, &state);
	    
	    if (json_get_errno(state) != JSON_ERROR_NONE)
	    {
		value = NULL;
		printf("[ERROR]: %s\n", json_get_error(state));
	    }
	    else
	    {
		json_print(value); printf("\n");
	    }
	    
	    json_release(value, state);
	}
    }
    
    return 0;
}
