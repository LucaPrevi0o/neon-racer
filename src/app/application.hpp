#pragma once

#include <raylib.h>

#include "../editor/editor.hpp"
#include "main_menu.hpp"
#include "playable_library.hpp"
#include "../race/time_trial.hpp"
#include "../render/time_trial_renderer.hpp"
#include "../playable/playable_export.hpp"

// The application owns only high-level flow. Track editing, validation and
// simulation live in dedicated modules as they are introduced in later phases.
enum class AppState {
    MainMenu,
    Editor,
    Race,
};

class RacerApplication {
public:
    RacerApplication();

    void Update(float frameTime);
    void Draw() const;

private:
    void ResetEditorCamera();
    void UpdateMainMenu();
    void UpdateEditor();
    void UpdateRace(float frameTime);
    void UpdatePlayableLibrary();
    void StartNewEditorSession();
    void LaunchPlayableTrack(const std::string& path);
    void ExportVerifiedPlayable(TrackMetadata metadata);
    void DrawMainMenu() const;
    void DrawEditor() const;
    void DrawRace() const;
    void DrawStateHint() const;

    AppState state_;
    // A playable package can be launched from either the menu or editor.
    // Tab returns the race to this source screen instead of assuming editor.
    AppState raceReturnState_;
    Camera3D editorCamera_;
    MainMenu mainMenu_;
    TrackEditor editor_;
    PlayableTrack playableTrack_;
    TimeTrial timeTrial_;
    TimeTrialRenderer timeTrialRenderer_;
    PlayableLibrary playableLibrary_;
};
