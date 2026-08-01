#include "application.hpp"
#include "app_settings.hpp"
#include "neon_racer/persistence/playable_track_io.hpp"
#include "raylib_race_input.hpp"

#include "../render/race_scene.hpp"
#include "../ui/neon.hpp"

namespace {

bool GamepadBackPressed() {
    return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
}

} // namespace

RacerApplication::RacerApplication()
    : state_(AppState::MainMenu),
      raceReturnState_(AppState::MainMenu),
      quitRequested_(false),
      editorCamera_{},
      mainMenu_(),
      racePauseMenu_(),
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
    if (quitRequested_) return;

    if (playableLibrary_.IsOpen()) {
        UpdatePlayableLibrary();
        return;
    }

    if (racePauseMenu_.IsOpen()) {
        UpdateRacePauseMenu();
        return;
    }

    if (state_ == AppState::MainMenu) {
        if (IsKeyPressed(KEY_ESCAPE) || GamepadBackPressed()) {
            quitRequested_ = true;
            return;
        }
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

    if (state_ == AppState::Editor && IsKeyPressed(KEY_ESCAPE)) {
        quitRequested_ = true;
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
        ReturnFromRace();
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

    if (!racePauseMenu_.IsOpen() && state_ != AppState::MainMenu) DrawStateHint();
    playableLibrary_.Draw();
    racePauseMenu_.Draw();
}

bool RacerApplication::QuitRequested() const { return quitRequested_; }

void RacerApplication::UpdateMainMenu() {
    mainMenu_.Update();
    const MainMenuAction action = mainMenu_.ConsumeAction();
    if (action == MainMenuAction::OpenPlayableLibrary) {
        playableLibrary_.Open();
    } else if (action == MainMenuAction::BeginNewTrack) {
        StartNewEditorSession();
    }
}

void RacerApplication::UpdateEditor() {
    editor_.Update(editorCamera_);
}

void RacerApplication::UpdateRace(float frameTime) {
    RaceInput input = ReadRaylibRaceInput();
    const bool pauseRequested = input.pausePressed || IsKeyPressed(KEY_ESCAPE) || GamepadBackPressed();
    if (pauseRequested) {
        if (!timeTrial_.IsPaused()) timeTrial_.TogglePause();
        racePauseMenu_.Open(raceReturnState_ == AppState::MainMenu);
        return;
    }

    timeTrial_.Update(frameTime, input);
    timeTrialRenderer_.Update(timeTrial_);
}

void RacerApplication::UpdateRacePauseMenu() {
    racePauseMenu_.Update();
    const RacePauseAction action = racePauseMenu_.ConsumeAction();
    if (action == RacePauseAction::None) return;

    if (action == RacePauseAction::Resume) {
        if (timeTrial_.IsPaused()) timeTrial_.TogglePause();
        racePauseMenu_.Close();
    } else if (action == RacePauseAction::Recover) {
        timeTrial_.Recover();
        timeTrialRenderer_.Update(timeTrial_);
    } else if (action == RacePauseAction::Restart) {
        timeTrial_.Reset();
        timeTrialRenderer_.Update(timeTrial_);
        racePauseMenu_.Close();
    } else if (action == RacePauseAction::Return) {
        ReturnFromRace();
    } else if (action == RacePauseAction::Quit) {
        quitRequested_ = true;
    }
}

void RacerApplication::UpdatePlayableLibrary() {
    playableLibrary_.Update();

    std::string playablePath;
    if (playableLibrary_.ConsumeLaunchRequest(playablePath)) LaunchPlayableTrack(playablePath);

    TrackMetadata metadata;
    if (playableLibrary_.ConsumeExportRequest(metadata)) ExportVerifiedPlayable(metadata);
}

void RacerApplication::LaunchPlayableTrack(const std::string& path) {
    PlayableTrack loaded;
    std::string error;
    if (!PlayableTrackIO::Load(path, loaded, error)) {
        playableLibrary_.ReportMessage(error);
        return;
    }

    // Validate the package against a temporary time trial before changing the
    // application's current editor/race state. A malformed package therefore
    // cannot partially replace the active session.
    TimeTrial candidate;
    candidate.Start(loaded.layout);
    if (!candidate.IsReady() || !candidate.ImportVerifiedGhost(loaded.verificationGhost)) {
        playableLibrary_.ReportMessage("Playable package verification does not match its track.");
        return;
    }

    playableTrack_ = loaded;
    timeTrial_.Start(playableTrack_.layout);
    if (!timeTrial_.ImportVerifiedGhost(playableTrack_.verificationGhost)) {
        playableLibrary_.ReportMessage("Could not activate the playable package ghost.");
        return;
    }
    // The package list can also be opened from a completed race's export
    // flow. Preserve that race's original source instead of creating a
    // Race -> Race Tab return loop when another package is launched there.
    raceReturnState_ = state_ == AppState::Race ? raceReturnState_ : state_;
    state_ = AppState::Race;
    playableLibrary_.Close();
}

void RacerApplication::StartNewEditorSession() {
    editor_.BeginNewTrack();
    ResetEditorCamera();
    playableTrack_ = PlayableTrack();
    timeTrial_ = TimeTrial();
    timeTrialRenderer_ = TimeTrialRenderer();
    racePauseMenu_.Close();
    state_ = AppState::Editor;
}

void RacerApplication::ExportVerifiedPlayable(TrackMetadata metadata) {
    VerifiedGhostData ghost;
    if (!timeTrial_.ExportVerifiedGhost(ghost)) {
        playableLibrary_.ReportMessage("The active run is no longer verified for this exact layout.");
        return;
    }

    std::string error;
    std::uint32_t nextVersion = 0;
    if (!PlayableTrackIO::NextExportVersion(metadata.name, nextVersion, error)) {
        playableLibrary_.ReportMessage(error);
        return;
    }
    metadata.playableExportVersion = nextVersion;

    PlayableTrack exported;
    if (!PlayableExport::BuildVerified(playableTrack_.layout, metadata, ghost, timeTrial_.Verification(), exported, error) ||
        !PlayableTrackIO::Save(exported, PlayableTrackIO::CustomPlayablePath(metadata.name), error)) {
        playableLibrary_.ReportMessage(error);
        return;
    }

    playableTrack_ = exported;
    playableLibrary_.FinishExport("Saved " + metadata.name + " as playable version " +
                                  std::to_string(metadata.playableExportVersion) + ".");
}

void RacerApplication::ReturnFromRace() {
    if (timeTrial_.IsPaused()) timeTrial_.TogglePause();
    racePauseMenu_.Close();
    state_ = raceReturnState_;
}

void RacerApplication::DrawMainMenu() const {
    mainMenu_.Draw();
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
    const char* hint = 0;
    float width = 570.0f;
    if (state_ == AppState::Editor) {
        hint = "TAB: race preview    Ctrl+P/F6: playable library    ESC: quit";
    } else if (state_ == AppState::Race) {
        width = 930.0f;
        hint = raceReturnState_ == AppState::MainMenu
            ? "P/START/ESC: pause    R/Y: recover    Shift+R/SELECT: restart    TAB: quick return to menu"
            : "P/START/ESC: pause    R/Y: recover    Shift+R/SELECT: restart    TAB: quick return to editor";
    }
    if (hint == 0) return;
    Neon::DrawOverlayPanel(Rectangle{28.0f, static_cast<float>(AppSettings::kWindowHeight - 58), width, 34.0f}, 0.72f);
    DrawText(hint, 42, AppSettings::kWindowHeight - 49, 17, Neon::Yellow);
}
