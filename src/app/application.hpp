#pragma once

#include <raylib.h>

#include "../editor/editor.hpp"
#include "../race/time_trial.hpp"
#include "../track/playable_export.hpp"

// The application owns only high-level flow. Track editing, validation and
// simulation live in dedicated modules as they are introduced in later phases.
enum class AppState {
    Editor,
    Race,
};

class RacerApplication {
public:
    RacerApplication();

    void Update(float frameTime);
    void Draw() const;

private:
    void UpdateEditor();
    void UpdateRace(float frameTime);
    void DrawEditor() const;
    void DrawRace() const;
    void DrawStateHint() const;

    AppState state_;
    Camera3D editorCamera_;
    Camera3D raceCamera_;
    TrackEditor editor_;
    PlayableTrack playableTrack_;
    TimeTrial timeTrial_;
};
