#pragma once

#include "race_results_flow.hpp"

// Raylib-facing modal shown after the complete three-lap run. It presents the
// final timing summary and gathers player intent without changing race or
// application state directly.
class RaceResultsMenu {
public:
    RaceResultsMenu();

    void Open(bool returnsToMainMenu, bool canSaveReplay, bool updatesExistingGhost,
              float totalTime, float bestLapTime);
    void Close();
    bool IsOpen() const;
    void Update();
    void Draw() const;
    RaceResultsAction ConsumeAction();

private:
    RaceResultsFlow flow_;
    bool open_;
    bool returnsToMainMenu_;
    bool updatesExistingGhost_;
    float totalTime_;
    float bestLapTime_;
};
