#include "application.hpp"
#include "app_settings.hpp"
#include "../persistence/playable_track_io.hpp"
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
    if (playableLibrary_.IsOpen()) {
        UpdatePlayableLibrary();
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

    if (IsKeyPressed(KEY_TAB)) {
        const AppState nextState = state_ == AppState::Editor ? AppState::Race : AppState::Editor;
        if (nextState == AppState::Race) {
            std::string error;
            if (PlayableExport::BuildPreview(editor_.GetTrack(), "Editor session", playableTrack_, error)) {
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
    playableLibrary_.Draw();
}

void RacerApplication::UpdateEditor() {
    editor_.Update(editorCamera_);
}

void RacerApplication::UpdateRace(float frameTime) {
    timeTrial_.Update(frameTime, ReadRaylibRaceInput());
    timeTrialRenderer_.Update(timeTrial_);
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
    if (!PlayableExport::BuildVerified(playableTrack_.layout, metadata, ghost, timeTrial_.Verification(), exported, error) ||
        !PlayableTrackIO::Save(exported, PlayableTrackIO::CustomPlayablePath(metadata.name), error)) {
        playableLibrary_.ReportMessage(error);
        return;
    }

    playableTrack_ = exported;
    playableLibrary_.FinishExport("Saved " + metadata.name + " as playable version " +
                                  std::to_string(metadata.playableExportVersion) + ".");
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
    const char* hint = state_ == AppState::Editor
        ? "TAB: race preview    Ctrl+P/F6: playable library    ESC: quit"
        : "TAB: return to editor    ESC: quit";
    Neon::DrawOverlayPanel(Rectangle{28.0f, static_cast<float>(AppSettings::kWindowHeight - 58), 570.0f, 34.0f}, 0.72f);
    DrawText(hint, 42, AppSettings::kWindowHeight - 49, 17, Neon::Yellow);
}
