#include <stdio.h>
#include <stdlib.h>

#include "./json.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s [files...]\n", argv[0]);
        return 1;
    }

    int             i, n;
    char*           buffer = NULL;
    json_state_t*   state  = NULL;

    for (i = 1, n = argc; i < n; i++)
    {
        const char* filename = argv[i];

        FILE* file = fopen(filename, "r");
        if (file)
        {
            size_t filesize;

            fseek(file, 0, SEEK_END);
            filesize = ftell(file);
            fseek(file, 0, SEEK_SET);

            buffer = (char*)realloc(buffer, (filesize + 1) * sizeof(char));
            buffer[filesize] = 0;
            fread(buffer, filesize, sizeof(char), file);

            json_value_t* value = json_parse(buffer, &state);
            if (json_get_errno(state) != JSON_ERROR_NONE || !value)
            {
                fprintf(stderr, "Parsing file '%s' error: %s\n", filename, json_get_error(state));
                return 1;
            }

            fclose(file);
        }
    }

    return 0;    
}