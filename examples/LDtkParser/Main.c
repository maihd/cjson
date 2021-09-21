#include "raylib.h"
#include "LDtkParser.h"

#pragma comment(lib, "winmm.lib")

int main(void)
{
    InitWindow(800, 600, "LDtk parser example");

    SetTargetFPS(60);

    LDtkWorld world;
    LDtkError error = LDtkParse("", 0, NULL, 0, &world);

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