#pragma once

// Raylib-free state for the race pause menu. The view maps keyboard, pointer,
// and gamepad input to these operations while RacerApplication owns every
// simulation and application-state change.
enum class RacePauseChoice {
    Resume,
    Recover,
    Restart,
    Return,
    Quit,
};

enum class RacePauseAction {
    None,
    Resume,
    Recover,
    Restart,
    Return,
    Quit,
};

class RacePauseFlow {
public:
    RacePauseFlow();

    void Reset();
    RacePauseChoice SelectedChoice() const;
    void SelectPrevious();
    void SelectNext();
    void Select(RacePauseChoice choice);
    void ActivateSelectedChoice();
    void CancelOrResume();

    bool IsConfirming() const;
    RacePauseChoice ConfirmationChoice() const;
    RacePauseAction ConsumeAction();

private:
    static bool NeedsConfirmation(RacePauseChoice choice);
    static RacePauseAction ActionFor(RacePauseChoice choice);

    RacePauseChoice selectedChoice_;
    RacePauseChoice confirmationChoice_;
    RacePauseAction pendingAction_;
    bool confirming_;
};
