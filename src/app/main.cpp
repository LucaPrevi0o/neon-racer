#include <raylib.h>

#include "application.hpp"
#include "app_settings.hpp"

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(AppSettings::kWindowWidth, AppSettings::kWindowHeight, "Neon Racer");
    SetTargetFPS(60);

    RacerApplication application;
    while (!WindowShouldClose() && !application.QuitRequested()) {
        application.Update(GetFrameTime());

        BeginDrawing();
        ClearBackground(BLACK);
        application.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
