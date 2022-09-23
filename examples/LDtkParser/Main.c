#define _CRT_SECURE_NO_WARNINGS

#include "raylib.h"
#include "LDtkParser.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

int main(void)
{
    InitWindow(800, 600, "LDtk parser example");

    SetTargetFPS(60);

	const char* ldtkPath = "../../examples/LDtkParser/assets/sample.ldtk";

    int32_t tempBufferSize = 20 * 1024 * 1024;
    void* tempBuffer = malloc((size_t)tempBufferSize);

#ifdef _WIN32
	LDtkContext context = LDtkContextWindows(tempBuffer, tempBufferSize);
#else
	LDtkContext context = LDtkContextStdC(tempBuffer, tempBufferSize);
#endif

    LDtkWorld world;
    LDtkError error = LDtkParse(ldtkPath, context, &world);
    if (error.code != LDtkErrorCode_None)
    {
        fprintf(stderr, "Parse ldtk sample content failed!: %s\n", error.message);
        return -1;
    }

    Texture texture = LoadTexture("../../examples/LDtkParser/assets/N2D - SpaceWallpaper1280x448.png");
    SetWindowSize(texture.width * 2, texture.height * 2);

    Texture levelBgTextures[32];
    for (int32_t i = 0; i < world.levelCount; i++)
    {
        char texturePath[2048];
        sprintf(texturePath, "../../examples/LDtkParser/assets/%s", world.levels[i].bgPath);
        printf("texturePath: %s\n", texturePath);
        levelBgTextures[i] = LoadTexture(texturePath);
    }

    int32_t currentLevelIndex = 2;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        {
            ClearBackground(BLACK);

            DrawTextureEx(levelBgTextures[currentLevelIndex], (Vector2){0, 0}, 0.0f, 2.0f, WHITE);
        }
        EndDrawing();
    }

    free(tempBuffer);

    CloseWindow();
    return 0;
}