#include <raylib.h>

#include "application.hpp"
#include "app_settings.hpp"

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(AppSettings::kWindowWidth, AppSettings::kWindowHeight, "Neon Racer");
    // Application state owns Escape/Back behavior. Keep the window close
    // button functional while preventing Raylib from terminating the process
    // before the active screen can handle Escape explicitly.
    SetExitKey(KEY_NULL);
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
