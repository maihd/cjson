#include "raylib.h"

#pragma comment(lib, "winmm.lib")

int main(void)
{
    InitWindow(800, 600, "LDtk parser example");

    SetTargetFPS(60);

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