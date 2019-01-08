#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./json.h"

typedef struct
{
    size_t alloced;
} json_debug_t;

static void* json_debug_malloc(void* data, size_t size)
{
    json_debug_t* debug = (json_debug_t*)data;
    debug->alloced += size;
    return malloc(size);
}

static void json_debug_free(void* data, void* ptr)
{
    (void*)data;
    free(ptr);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s [files...]\n", argv[0]);
        return 1;
    }

    json_debug_t debug;
    memset(&debug, 0, sizeof(debug));

    json_settings_t settings;
    settings.data   = &debug;
    settings.free   = json_debug_free;
    settings.malloc = json_debug_malloc;

    int   i, n;
    char* buffer = NULL;
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

            json_state_t* state = NULL;
            json_value_t* value = json_parse_ex(buffer, &settings, &state);
            if (json_get_errno(state) != JSON_ERROR_NONE || !value)
            {
                fprintf(stderr, "Parsing file '%s' error: %s\n", filename, json_get_error(state));
                return 1;
            }
            json_release(state);

            fclose(file);
        }
    }

    printf("Unit testing succeed.\n");
#if defined(_MSC_VER) && !defined(NDEBUG)
    getchar();
#endif
    return 0;    
}