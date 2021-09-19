#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "Json.h"
#include "JsonEx.h"

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
    int allocatorCapacity = 1024 * 1024; // 1MB temp buffer
    void* allocatorBuffer = malloc(allocatorCapacity);
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
            fclose(file);

            double dt = gettime();
            Json* value;
            JsonError error = Json_parse(fileBuffer, filesize, JsonFlags_None, allocatorBuffer, allocatorCapacity, &value);
            if (error.code != JsonError_None)
            {
                fprintf(stderr, "Parsing file '%s' error: %s\n", filename, error.message);
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

            printf(
                "Parsed file '%s'\n"
                "\t- file size:\t%dB\n"
                "\t- times:\t%lfs\n\n", filename, filesize, dt);
        }
    }

    printf("Unit testing succeed.\n");
    free(allocatorBuffer);
    free(fileBuffer);

    return 0;    
}

