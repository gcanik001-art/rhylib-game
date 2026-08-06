#include "raylib.h"

int main(void)
{
    InitWindow(800, 450, "Hello Raylib");

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello Raylib!", 250, 200, 30, RED);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}