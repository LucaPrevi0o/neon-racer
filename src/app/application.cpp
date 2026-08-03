#include "application.hpp"
#include "app_settings.hpp"
#include "neon_racer/persistence/playable_track_io.hpp"
#include "raylib_race_input.hpp"

#include "../render/race_scene.hpp"
#include "../ui/neon.hpp"

#include <limits>

namespace {

bool GamepadBackPressed() {
    return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
}

bool GamepadPausePressed() {
    return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
}

} // namespace

RacerApplication::RacerApplication()
    : state_(AppState::MainMenu),
      raceSessionKind_(RaceSessionKind::EditorTimeTrial),
      activePlayablePath_(),
      editorSessionActive_(false),
      quitRequested_(false),
      editorCamera_{},
      mainMenu_(),
      editorPauseMenu_(),
      racePauseMenu_(),
      raceResultsMenu_(),
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

    if (raceResultsMenu_.IsOpen()) {
        UpdateRaceResultsMenu();
        return;
    }

    if (racePauseMenu_.IsOpen()) {
        UpdateRacePauseMenu();
        return;
    }

    if (editorPauseMenu_.IsOpen()) {
        UpdateEditorPauseMenu();
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

    if (state_ == AppState::Editor) {
        if (IsKeyPressed(KEY_ESCAPE) || GamepadPausePressed()) {
            editor_.CloseDraftLibrary();
            editorPauseMenu_.Open(editor_.IsRaceReady(), editor_.HasSavedDraft());
            return;
        }
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

    if (!editorPauseMenu_.IsOpen() && !racePauseMenu_.IsOpen() &&
        !raceResultsMenu_.IsOpen() && state_ != AppState::MainMenu) {
        DrawStateHint();
    }
    raceResultsMenu_.Draw();
    playableLibrary_.Draw();
    racePauseMenu_.Draw();
    editorPauseMenu_.Draw();
}

bool RacerApplication::QuitRequested() const { return quitRequested_; }

void RacerApplication::UpdateMainMenu() {
    mainMenu_.Update();
    const MainMenuAction action = mainMenu_.ConsumeAction();
    if (action == MainMenuAction::OpenPlayableLibrary) {
        playableLibrary_.Open();
    } else if (action == MainMenuAction::OpenTrackEditor) {
        EnterTrackEditor();
    }
}

void RacerApplication::UpdateEditor() {
    editor_.Update(editorCamera_);
}

void RacerApplication::UpdateEditorPauseMenu() {
    editorPauseMenu_.Update();
    const EditorPauseAction action = editorPauseMenu_.ConsumeAction();
    if (action == EditorPauseAction::None) return;

    if (action == EditorPauseAction::Resume) {
        editorPauseMenu_.Close();
    } else if (action == EditorPauseAction::StartTrial) {
        StartEditorTimeTrial();
    } else if (action == EditorPauseAction::SaveDraft) {
        editorPauseMenu_.Close();
        editor_.SaveDraft();
    } else if (action == EditorPauseAction::OpenDraft) {
        editorPauseMenu_.Close();
        editor_.OpenDraftLibrary();
    } else if (action == EditorPauseAction::QuitToMenu) {
        ReturnHome();
    }
}

void RacerApplication::UpdateRace(float frameTime) {
    RaceInput input = ReadRaylibRaceInput();
    const bool pauseRequested = input.pausePressed || IsKeyPressed(KEY_ESCAPE);
    if (pauseRequested) {
        if (!timeTrial_.IsPaused()) timeTrial_.TogglePause();
        racePauseMenu_.Open(true);
        return;
    }

    timeTrial_.Update(frameTime, input);
    if (input.resetPressed || input.recoverPressed) {
        timeTrialRenderer_.SnapTo(timeTrial_);
    } else {
        timeTrialRenderer_.Update(timeTrial_);
    }

    if (timeTrial_.IsFinished()) {
        const bool ghostRace = raceSessionKind_ == RaceSessionKind::GhostRace;
        const bool canSaveReplay = ghostRace
            ? timeTrial_.LastCompletedRunImprovedGhost()
            : timeTrial_.HasVerifiedGhost();
        raceResultsMenu_.Open(true,
                              canSaveReplay,
                              ghostRace,
                              timeTrial_.TotalTime(),
                              timeTrial_.BestLapTime());
    }
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
        if (timeTrial_.IsPaused()) timeTrial_.TogglePause();
        timeTrialRenderer_.SnapTo(timeTrial_);
        racePauseMenu_.Close();
    } else if (action == RacePauseAction::Restart) {
        timeTrial_.Reset();
        timeTrialRenderer_.SnapTo(timeTrial_);
        racePauseMenu_.Close();
    } else if (action == RacePauseAction::Return) {
        ReturnHome();
    } else if (action == RacePauseAction::Quit) {
        quitRequested_ = true;
    }
}

void RacerApplication::UpdateRaceResultsMenu() {
    raceResultsMenu_.Update();
    const RaceResultsAction action = raceResultsMenu_.ConsumeAction();
    if (action == RaceResultsAction::None) return;

    if (action == RaceResultsAction::RaceAgain) {
        timeTrial_.Reset();
        timeTrialRenderer_.SnapTo(timeTrial_);
        raceResultsMenu_.Close();
    } else if (action == RaceResultsAction::SavePlayable) {
        if (raceSessionKind_ == RaceSessionKind::GhostRace) {
            UpdateActivePlayableGhost();
        } else {
            playableLibrary_.BeginExport(playableTrack_.metadata);
        }
    } else if (action == RaceResultsAction::Return) {
        ReturnHome();
    } else if (action == RaceResultsAction::Quit) {
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

void RacerApplication::EnterTrackEditor() {
    if (!editorSessionActive_) {
        StartNewEditorSession();
        return;
    }
    editorPauseMenu_.Close();
    racePauseMenu_.Close();
    raceResultsMenu_.Close();
    playableLibrary_.Close();
    state_ = AppState::Editor;
}

void RacerApplication::StartNewEditorSession() {
    editor_.BeginNewTrack();
    ResetEditorCamera();
    playableTrack_ = PlayableTrack();
    timeTrial_ = TimeTrial();
    timeTrialRenderer_ = TimeTrialRenderer();
    raceSessionKind_ = RaceSessionKind::EditorTimeTrial;
    activePlayablePath_.clear();
    editorSessionActive_ = true;
    editorPauseMenu_.Close();
    racePauseMenu_.Close();
    raceResultsMenu_.Close();
    playableLibrary_.Close();
    state_ = AppState::Editor;
}

void RacerApplication::StartEditorTimeTrial() {
    PlayableTrack preview;
    std::string error;
    if (!PlayableExport::BuildPreview(editor_.GetTrack(), "Editor session", preview, error)) {
        editorPauseMenu_.Close();
        return;
    }

    playableTrack_ = preview;
    timeTrial_.Start(playableTrack_.layout);
    raceSessionKind_ = RaceSessionKind::EditorTimeTrial;
    activePlayablePath_.clear();
    editorPauseMenu_.Close();
    racePauseMenu_.Close();
    raceResultsMenu_.Close();
    timeTrialRenderer_.SnapTo(timeTrial_);
    state_ = AppState::Race;
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
    raceSessionKind_ = RaceSessionKind::GhostRace;
    activePlayablePath_ = path;
    editorPauseMenu_.Close();
    racePauseMenu_.Close();
    raceResultsMenu_.Close();
    timeTrialRenderer_.SnapTo(timeTrial_);
    state_ = AppState::Race;
    playableLibrary_.Close();
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
    const std::string path = PlayableTrackIO::CustomPlayablePath(metadata.name);
    if (!PlayableExport::BuildVerified(playableTrack_.layout, metadata, ghost, timeTrial_.Verification(), exported, error) ||
        !PlayableTrackIO::Save(exported, path, error)) {
        playableLibrary_.ReportMessage(error);
        return;
    }

    playableTrack_ = exported;
    playableLibrary_.FinishExport("Saved " + metadata.name + " as playable version " +
                                  std::to_string(metadata.playableExportVersion) + ".");
}

void RacerApplication::UpdateActivePlayableGhost() {
    if (raceSessionKind_ != RaceSessionKind::GhostRace || activePlayablePath_.empty() ||
        !timeTrial_.LastCompletedRunImprovedGhost()) {
        return;
    }

    VerifiedGhostData ghost;
    if (!timeTrial_.ExportVerifiedGhost(ghost)) {
        ShowPlayableLibraryMessage("The completed ghost race no longer has a verified replay.");
        return;
    }

    std::string error;
    PlayableTrack current;
    if (!PlayableTrackIO::Load(activePlayablePath_, current, error)) {
        ShowPlayableLibraryMessage("Could not update the saved ghost: " + error);
        return;
    }
    if (current.layoutFingerprint != playableTrack_.layoutFingerprint ||
        current.metadata.name != playableTrack_.metadata.name) {
        ShowPlayableLibraryMessage("The saved package changed while this ghost race was running.");
        return;
    }
    if (current.metadata.playableExportVersion == std::numeric_limits<std::uint32_t>::max()) {
        ShowPlayableLibraryMessage("Playable export version has reached its maximum value.");
        return;
    }

    current.verificationGhost = ghost;
    ++current.metadata.playableExportVersion;
    if (!PlayableTrackIO::Save(current, activePlayablePath_, error)) {
        ShowPlayableLibraryMessage("Could not update the saved ghost: " + error);
        return;
    }

    playableTrack_ = current;
    ShowPlayableLibraryMessage("Updated " + current.metadata.name + " to playable version " +
                               std::to_string(current.metadata.playableExportVersion) + ".");
}

void RacerApplication::ShowPlayableLibraryMessage(const std::string& message) {
    if (timeTrial_.IsPaused()) timeTrial_.TogglePause();
    editorPauseMenu_.Close();
    racePauseMenu_.Close();
    raceResultsMenu_.Close();
    state_ = AppState::MainMenu;
    playableLibrary_.Open();
    playableLibrary_.ReportMessage(message);
}

void RacerApplication::ReturnHome() {
    if (timeTrial_.IsPaused()) timeTrial_.TogglePause();
    editorPauseMenu_.Close();
    racePauseMenu_.Close();
    raceResultsMenu_.Close();
    playableLibrary_.Close();
    state_ = AppState::MainMenu;
}

void RacerApplication::DrawMainMenu() const {
    mainMenu_.Draw(editorSessionActive_);
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
    float width = 670.0f;
    if (state_ == AppState::Editor) {
        hint = "ESC: editor actions    Ctrl+S: save draft    Ctrl+O: open draft";
    } else if (state_ == AppState::Race) {
        width = 790.0f;
        hint = "ESC / START: pause    R/Y: recover    Shift+R/SELECT: restart";
    }
    if (hint == 0) return;
    Neon::DrawOverlayPanel(Rectangle{28.0f, static_cast<float>(AppSettings::kWindowHeight - 58), width, 34.0f}, 0.72f);
    DrawText(hint, 42, AppSettings::kWindowHeight - 49, 17, Neon::Yellow);
}
