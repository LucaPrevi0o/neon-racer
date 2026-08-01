#pragma once

// Raylib-free state for the completed-race results menu. The view maps
// keyboard, pointer, and gamepad input to these operations while
// RacerApplication owns race resets, export, navigation, and quit policy.
enum class RaceResultsChoice {
    RaceAgain,
    SavePlayable,
    Return,
    Quit,
};

enum class RaceResultsAction {
    None,
    RaceAgain,
    SavePlayable,
    Return,
    Quit,
};

class RaceResultsFlow {
public:
    RaceResultsFlow();

    void Reset(bool canSavePlayable);
    RaceResultsChoice SelectedChoice() const;
    void SelectPrevious();
    void SelectNext();
    void Select(RaceResultsChoice choice);
    void ActivateSelectedChoice();
    void Back();

    bool CanSavePlayable() const;
    bool IsChoiceEnabled(RaceResultsChoice choice) const;
    bool IsConfirming() const;
    RaceResultsChoice ConfirmationChoice() const;
    RaceResultsAction ConsumeAction();

private:
    static bool NeedsConfirmation(RaceResultsChoice choice);
    static RaceResultsAction ActionFor(RaceResultsChoice choice);
    void MoveSelection(int direction);

    RaceResultsChoice selectedChoice_;
    RaceResultsChoice confirmationChoice_;
    RaceResultsAction pendingAction_;
    bool canSavePlayable_;
    bool confirming_;
};
