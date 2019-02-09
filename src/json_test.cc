#include <signal.h>
#include <setjmp.h>

#define JSON_IMPL
#include "../json.h"

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

namespace json
{
    /* @funcdef: write */
    void write(const Value& value, FILE* out)
    {
        if (value)
        {
            int i, n;

            switch (value.type)
            {
            case Type::Null:
                fprintf(out, "null");
                break;

            case Type::Number:
                fprintf(out, "%lf", value.number);
                break;

            case Type::Boolean:
                fprintf(out, "%s", value.boolean ? "true" : "false");
                break;

            case Type::String:
                fprintf(out, "\"%s\"", value.string);
                break;

            case Type::Array:
                fprintf(out, "[");
                for (i = 0, n = value.length(); i < n; i++)
                {
                    write(value[i], out);
                    if (i < n - 1)
                    {
                        fprintf(out, ",");
                    }
                }
                fprintf(out, "]");
                break;

            case Type::Object:
                fprintf(out, "{");
                for (i = 0, n = value.length(); i < n; i++)
                {
                    fprintf(out, "\"%s\" : ", value.object[i].name);
                    write(*value.object[i].value, out);
                    if (i < n - 1)
                    {
                        fprintf(out, ",");
                    }
                }
                fprintf(out, "}");
                break;

            default:
                break;
            }
        }
    }

    /* @funcdef: print */
    void print(const Value& value, FILE *out)
    {
        if (value)
        {
            int i, n;
            static int indent = 0;

            switch (value.type)
            {
            case Type::Null:
                fprintf(out, "null");
                break;

            case Type::Number:
                fprintf(out, "%lf", value.number);
                break;

            case Type::Boolean:
                fprintf(out, "%s", value.boolean ? "true" : "false");
                break;

            case Type::String:
                fprintf(out, "\"%s\"", value.string);
                break;

            case Type::Array:
                fprintf(out, "[\n");

                indent++;
                for (i = 0, n = value.length(); i < n; i++)
                {
                    int j, m;
                    for (j = 0, m = indent * 4; j < m; j++)
                    {
                        fputc(' ', out);
                    }

                    print(value[i], out);
                    if (i < n - 1)
                    {
                        fputc(',', out);
                    }
                    fprintf(out, "\n");
                }
                indent--;

                for (i = 0, n = indent * 4; i < n; i++)
                {
                    fprintf(out, " ");
                }
                fputc(']', out);
                break;

            case Type::Object:
                fprintf(out, "{\n");

                indent++;
                for (i = 0, n = value.length(); i < n; i++)
                {
                    int j, m;
                    for (j = 0, m = indent * 4; j < m; j++)
                    {
                        fputc(' ', out);
                    }

                    fprintf(out, "\"%s\" : ", value.object[i].name);
                    print(*value.object[i].value, out);
                    if (i < n - 1)
                    {
                        fputc(',', out);
                    }
                    fputc('\n', out);
                }
                indent--;

                for (i = 0, n = indent * 4; i < n; i++)
                {
                    fputc(' ', out);
                }
                fputc('}', out);
                break;

            default:
                break;
            }
        }
    }
}

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