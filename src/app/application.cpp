#include "application.hpp"
#include "app_settings.hpp"
#include "neon_racer/persistence/playable_track_io.hpp"
#include "raylib_race_input.hpp"

#include "../render/race_scene.hpp"
#include "../ui/neon.hpp"

RacerApplication::RacerApplication()
    : state_(AppState::MainMenu),
      raceReturnState_(AppState::MainMenu),
      editorCamera_{},
      mainMenu_(),
      editor_() {
    ResetEditorCamera();
}

void RacerApplication::ResetEditorCamera() {
    editorCamera_.position = Vector3{18.0f, 18.0f, 18.0f};
    editorCamera_.target = Vector3{0.0f, 0.0f, 0.0f};
    editorCamera_.up = Vector3{0.0f, 1.0f, 0.0f};
    editorCamera_.fovy = 45.0f;
    editorCamera_.projection = CAMERA_PERSPECTIVE;
}

void RacerApplication::Update(float frameTime) {
    if (playableLibrary_.IsOpen()) {
        UpdatePlayableLibrary();
        return;
    }

    if (state_ == AppState::MainMenu) {
        UpdateMainMenu();
        return;
    }

    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (state_ == AppState::Editor && ((control && IsKeyPressed(KEY_P)) || IsKeyPressed(KEY_F6))) {
        playableLibrary_.Open();
        return;
    }
    if (state_ == AppState::Race && timeTrial_.IsFinished() && timeTrial_.HasVerifiedGhost() && IsKeyPressed(KEY_E)) {
        playableLibrary_.BeginExport(playableTrack_.metadata);
        return;
    }

    if (state_ == AppState::Editor && IsKeyPressed(KEY_TAB)) {
        std::string error;
        if (PlayableExport::BuildPreview(editor_.GetTrack(), "Editor session", playableTrack_, error)) {
            timeTrial_.Start(playableTrack_.layout);
        } else {
            // Start preserves its existing, clear validation feedback when an
            // export cannot be made yet.
            timeTrial_.Start(editor_.GetTrack());
        }
        raceReturnState_ = AppState::Editor;
        state_ = AppState::Race;
    } else if (state_ == AppState::Race && IsKeyPressed(KEY_TAB)) {
        state_ = raceReturnState_;
        return;
    }

    if (state_ == AppState::Editor) {
        UpdateEditor();
    } else if (state_ == AppState::Race) {
        UpdateRace(frameTime);
    }
}

void RacerApplication::Draw() const {
    Neon::DrawBackground(AppSettings::kWindowWidth, AppSettings::kWindowHeight);

    if (state_ == AppState::MainMenu) {
        DrawMainMenu();
    } else if (state_ == AppState::Editor) {
        DrawEditor();
    } else {
        DrawRace();
    }
}
