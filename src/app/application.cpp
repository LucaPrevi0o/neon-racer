#include "application.hpp"
#include "app_settings.hpp"
#include "raylib_race_input.hpp"

#include "../render/race_scene.hpp"
#include "../ui/neon.hpp"

RacerApplication::RacerApplication()
    : state_(AppState::Editor),
      editorCamera_{},
      editor_() {
    editorCamera_.position = Vector3{18.0f, 18.0f, 18.0f};
    editorCamera_.target = Vector3{0.0f, 0.0f, 0.0f};
    editorCamera_.up = Vector3{0.0f, 1.0f, 0.0f};
    editorCamera_.fovy = 45.0f;
    editorCamera_.projection = CAMERA_PERSPECTIVE;

}

void RacerApplication::Update(float frameTime) {
    if (IsKeyPressed(KEY_TAB)) {
        const AppState nextState = state_ == AppState::Editor ? AppState::Race : AppState::Editor;
        if (nextState == AppState::Race) {
            std::string error;
            if (PlayableExport::Build(editor_.GetTrack(), "Editor session", playableTrack_, error)) {
                timeTrial_.Start(playableTrack_.layout);
            } else {
                // Start preserves its existing, clear validation feedback when an
                // export cannot be made yet.
                timeTrial_.Start(editor_.GetTrack());
            }
        }
        state_ = nextState;
    }

    if (state_ == AppState::Editor) {
        UpdateEditor();
    } else {
        UpdateRace(frameTime);
    }
}

void RacerApplication::Draw() const {
    Neon::DrawBackground(AppSettings::kWindowWidth, AppSettings::kWindowHeight);

    if (state_ == AppState::Editor) {
        DrawEditor();
    } else {
        DrawRace();
    }

    DrawStateHint();
}

void RacerApplication::UpdateEditor() {
    editor_.Update(editorCamera_);
}

void RacerApplication::UpdateRace(float frameTime) {
    timeTrial_.Update(frameTime, ReadRaylibRaceInput());
    timeTrialRenderer_.Update(timeTrial_);
}

void RacerApplication::DrawEditor() const {
    BeginMode3D(editorCamera_);
    DrawGridFloor(24, 1.0f, Fade(Neon::Cyan, 0.20f));
    editor_.DrawTrack3D();
    EndMode3D();

    Neon::DrawHeader("NEON RACER  //  TRACK EDITOR", AppSettings::kWindowWidth);
    editor_.DrawInterface();
}

void RacerApplication::DrawRace() const {
    const Track& displayedTrack = timeTrial_.IsReady() ? playableTrack_.layout : editor_.GetTrack();
    timeTrialRenderer_.Draw(displayedTrack, timeTrial_);
}

void RacerApplication::DrawStateHint() const {
    Neon::DrawOverlayPanel(Rectangle{28.0f, static_cast<float>(AppSettings::kWindowHeight - 58), 460.0f, 34.0f}, 0.72f);
    DrawText("TAB: switch editor / race preview    ESC: quit", 42, AppSettings::kWindowHeight - 49, 17, Neon::Yellow);
}
