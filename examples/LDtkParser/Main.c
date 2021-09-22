#include "raylib.h"
#include "LDtkParser.h"

#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "winmm.lib")

int main(void)
{
    InitWindow(800, 600, "LDtk parser example");

    SetTargetFPS(60);

    FILE* file;
    errno_t fileError = fopen_s(&file, "../../examples/LDtkParser/assets/sample.ldtk", "r"); // This path is hardcode, rewrite if needed 
    if (!file || fileError != 0)
    {
        fprintf(stderr, "Can not load sample file, maybe path is wrong!\n");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    int32_t fileSize = (int32_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json = (char*)malloc(fileSize + 1);
    fread(json, 1, fileSize, file);
    json[fileSize + 1] = 0;

    fclose(file);

    int32_t tempBufferSize = 1024 * 1024;
    void* tempBuffer = malloc((size_t)tempBufferSize);

    LDtkWorld world;
    LDtkError error = LDtkParse(json, fileSize, tempBuffer, tempBufferSize, &world);
    if (error.code != LDtkErrorCode_None)
    {
        fprintf(stderr, "Parse ldtk sample content failed!: %s\n", error.message);
        return -1;
    }

    while (!WindowShouldClose())
    {
        BeginDrawing();
        {
            ClearBackground(BLACK);
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}