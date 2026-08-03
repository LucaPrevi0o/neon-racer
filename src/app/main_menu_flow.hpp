#pragma once

// Raylib-free state for the Home menu. The view maps keyboard and mouse input
// to these operations, while RacerApplication owns the resulting screen
// transition and preserves any active editor session.
enum class MainMenuChoice {
    PlayCompleteTrack,
    OpenTrackEditor,
};

enum class MainMenuAction {
    None,
    OpenPlayableLibrary,
    OpenTrackEditor,
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
