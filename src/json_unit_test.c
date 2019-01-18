#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./json.h"
#include "./json.c"

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
    (void)data;
    free(ptr);
}

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <Windows.h>

double get_time(void)
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return (double)t.QuadPart / (double)f.QuadPart;
}
#elif defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/time.h>

double get_time(void)
{
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
#endif

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s [files...]\n", argv[0]);
        return 1;
    }

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

            json_debug_t debug;
            memset(&debug, 0, sizeof(debug));

            json_settings_t settings;
            settings.data   = &debug;
            settings.free   = json_debug_free;
            settings.malloc = json_debug_malloc;

            double dt = get_time();
            json_state_t* state = NULL;
            json_value_t* value = json_parse_ex(buffer, &settings, &state);
            if (json_get_errno(state) != JSON_ERROR_NONE || !value)
            {
                fprintf(stderr, "Parsing file '%s' error: %s\n", filename, json_get_error(state));
                return 1;
            }
            dt = get_time() - dt;

            json_release(state);
            fclose(file);

            printf("Parsed file '%s'\n\t- file size:\t%zuB\n\t- memory usage:\t%zuB\n\t- times: %lfs\n\n", filename, filesize, debug.alloced, dt);
        }
    }

    printf("Unit testing succeed.\n");
#if defined(_MSC_VER) && !defined(NDEBUG)
    getchar();
#endif
    return 0;    
}