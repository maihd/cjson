#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "./Json.h"

typedef struct
{
    int alloced;
} JsonDebugAllocator;

static void* JsonDebug_Alloc(void* data, int size)
{
    assert(size > 0 && "Internal error: attempt alloc with size < 0");

    JsonDebugAllocator* debug = (JsonDebugAllocator*)data;
    debug->alloced += size;

    //printf("Allocate: %d - %d\n", size, debug->alloced);

    return malloc((size_t)size);
}

static void JsonDebug_Free(void* data, void* ptr)
{
    //assert(ptr && "Internal error: attempt free with nullptr");

    //JsonDebugAllocator* debug = (JsonDebugAllocator*)data;
    //debug->alloced -= size;
    (void)data;
    free(ptr);
}

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__cygwin__)
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

            JsonDebugAllocator debug;
            memset(&debug, 0, sizeof(debug));

            JsonAllocator settings;
            settings.data  = &debug;
            settings.free  = JsonDebug_Free;
            settings.alloc = JsonDebug_Alloc;

            double dt = get_time();
            JsonState* state = NULL;
            JsonValue* value = JsonParseEx(buffer, &settings, &state);
            if (JsonGetError(state) != JSON_ERROR_NONE || !value)
            {
                fprintf(stderr, "Parsing file '%s' error: %s\n", filename, JsonGetErrorString(state));
                return 1;
            }
            dt = get_time() - dt;

            //int length = JsonLength(value);
            //JsonValue* firstObject = value && JsonLength(value) > 0 ? &value->array[0] : NULL;
            //
            //JsonValue* idValue = JsonFind(firstObject, "_id");

            JsonRelease(state);
            fclose(file);

            printf("Parsed file '%s'\n\t- file size:\t%zuB\n\t- memory usage:\t%zuB\n\t- times:\t%lfs\n\n", filename, filesize, debug.alloced, dt);
        }
    }

    printf("Unit testing succeed.\n");
#if defined(_MSC_VER) && !defined(NDEBUG)
    getchar();
#endif
    return 0;    
}