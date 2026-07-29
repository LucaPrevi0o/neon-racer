#include <raylib.h>

#include "application.hpp"

int main() {
    constexpr int kScreenWidth = 1280;
    constexpr int kScreenHeight = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(kScreenWidth, kScreenHeight, "Neon Racer");
    SetTargetFPS(60);

    RacerApplication application;
    while (!WindowShouldClose() && !IsKeyPressed(KEY_ESCAPE)) {
        application.Update(GetFrameTime());

        BeginDrawing();
        ClearBackground(BLACK);
        application.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
