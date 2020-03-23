#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "Json.h"
#include "JsonEx.h"

typedef struct
{
    int alloced;
} JsonDebugAllocator;

static void* JsonDebugAllocator_alloc(void* data, int size)
{
    assert(size > 0 && "Internal error: attempt alloc with size < 0");

    JsonDebugAllocator* debug = (JsonDebugAllocator*)data;
    debug->alloced += size;

    //printf("Allocate: %d - %d\n", size, debug->alloced);

    return malloc((size_t)size);
}

static void JsonDebugAllocator_free(void* data, void* ptr)
{
    //assert(ptr && "Internal error: attempt free with nullptr");

    //JsonDebugAllocator* debug = (JsonDebugAllocator*)data;
    //debug->alloced -= size;
    (void)data;
    free(ptr);
}

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__cygwin__) || defined(_WIN32)
#include <Windows.h>

double gettime(void)
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return (double)t.QuadPart / (double)f.QuadPart;
}
#elif defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/time.h>

double gettime(void)
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
    char* fileBuffer = NULL;
    void* allocatorBuffer = malloc(1024 * 1024);
    for (i = 1, n = argc; i < n; i++)
    {
        const char* filename = argv[i];

        FILE* file = fopen(filename, "r");
        if (file)
        {
            int filesize;

            fseek(file, 0, SEEK_END);
            filesize = (int)ftell(file);
            fseek(file, 0, SEEK_SET);

            fileBuffer = (char*)realloc(fileBuffer, (filesize + 1) * sizeof(char));
            fileBuffer[filesize] = 0;
            fread(fileBuffer, filesize, sizeof(char), file);

            //JsonDebugAllocator debug;
            //memset(&debug, 0, sizeof(debug));
            //
            //JsonAllocator allocator;
            //allocator.data  = &debug;
            //allocator.free  = JsonDebugAllocator_free;
            //allocator.alloc = JsonDebugAllocator_alloc;

            JsonTempAllocator allocator;
            JsonTempAllocator_init(&allocator, allocatorBuffer, 1024 * 1024);

            double dt = gettime();
            Json* value = Json_parseEx(fileBuffer, filesize, allocator.super, JsonFlags_None);
            if (Json_getError(value) != JsonError_None)
            {
                fprintf(stderr, "Parsing file '%s' error: %s\n", filename, Json_getErrorMessage(value));
                return 1;
            }
            dt = gettime() - dt;

            int length = value->length;
            Json* firstObject = value && length > 0 ? &value->array[0] : NULL;
            //
            Json* idValue = Json_find(firstObject, "_id");
            if (idValue)
            {
                printf("idValue: ");
                Json_print(idValue, stdout);
                printf("\n");
            }

            // When use temp allocator, donot Json_release
            //Json_release(state);
            fclose(file);

            printf("Parsed file '%s'\n\t- file size:\t%dB\n\t- memory usage:\t%dB\n\t- times:\t%lfs\n\n", filename, filesize, allocator.marker, dt);
        }
    }

    printf("Unit testing succeed.\n");
    free(allocatorBuffer);
    free(fileBuffer);

#if defined(_MSC_VER) && !defined(NDEBUG)
    getchar();
#endif
    return 0;    
}

