#pragma once

// Raylib-free state for the startup menu. The view maps keyboard and mouse
// input to these operations, while RacerApplication owns the resulting screen
// transition and any session or persistence work.
enum class MainMenuChoice {
    PlayCompleteTrack,
    CreateNewTrack,
};

enum class MainMenuAction {
    None,
    OpenPlayableLibrary,
    BeginNewTrack,
};

class MainMenuFlow {
public:
    MainMenuFlow();

    MainMenuChoice SelectedChoice() const;
    void SelectPrevious();
    void SelectNext();
    void Select(MainMenuChoice choice);
    void ActivateSelectedChoice();
    MainMenuAction ConsumeAction();

private:
    MainMenuChoice selectedChoice_;
    MainMenuAction pendingAction_;
};
