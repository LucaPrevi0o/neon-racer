#pragma once

// Raylib-free state for the editor action menu. Device input and drawing live
// in EditorPauseMenu, while RacerApplication owns the resulting transitions.
enum class EditorPauseChoice {
    StartTrial,
    SaveDraft,
    OpenDraft,
    QuitToMenu,
};

enum class EditorPauseAction {
    None,
    Resume,
    StartTrial,
    SaveDraft,
    OpenDraft,
    QuitToMenu,
};

class EditorPauseFlow {
public:
    EditorPauseFlow()
        : selectedChoice_(EditorPauseChoice::SaveDraft),
          pendingAction_(EditorPauseAction::None),
          canStartTrial_(false),
          hasSavedDraft_(false),
          confirmingQuit_(false) {}

    void Reset(bool canStartTrial, bool hasSavedDraft) {
        canStartTrial_ = canStartTrial;
        hasSavedDraft_ = hasSavedDraft;
        selectedChoice_ = canStartTrial_ ? EditorPauseChoice::StartTrial
                                         : EditorPauseChoice::SaveDraft;
        pendingAction_ = EditorPauseAction::None;
        confirmingQuit_ = false;
    }

    EditorPauseChoice SelectedChoice() const { return selectedChoice_; }
    bool CanStartTrial() const { return canStartTrial_; }
    bool HasSavedDraft() const { return hasSavedDraft_; }
    bool IsConfirmingQuit() const { return confirmingQuit_; }

    bool IsChoiceEnabled(EditorPauseChoice choice) const {
        return choice != EditorPauseChoice::StartTrial || canStartTrial_;
    }

    void Select(EditorPauseChoice choice) {
        if (!IsChoiceEnabled(choice)) return;
        if (choice != selectedChoice_) confirmingQuit_ = false;
        selectedChoice_ = choice;
    }

    void SelectPrevious() { MoveSelection(-1); }
    void SelectNext() { MoveSelection(1); }

    void ActivateSelectedChoice() {
        if (!IsChoiceEnabled(selectedChoice_)) return;
        if (selectedChoice_ == EditorPauseChoice::QuitToMenu && !hasSavedDraft_) {
            if (!confirmingQuit_) {
                confirmingQuit_ = true;
                return;
            }
        }
        pendingAction_ = ActionFor(selectedChoice_);
        confirmingQuit_ = false;
    }

    void CancelAlertOrResume() {
        if (confirmingQuit_) {
            confirmingQuit_ = false;
            return;
        }
        pendingAction_ = EditorPauseAction::Resume;
    }

    void CancelQuitAlert() { confirmingQuit_ = false; }

    EditorPauseAction ConsumeAction() {
        const EditorPauseAction action = pendingAction_;
        pendingAction_ = EditorPauseAction::None;
        return action;
    }

private:
    static int ChoiceIndex(EditorPauseChoice choice) {
        switch (choice) {
        case EditorPauseChoice::StartTrial: return 0;
        case EditorPauseChoice::SaveDraft: return 1;
        case EditorPauseChoice::OpenDraft: return 2;
        case EditorPauseChoice::QuitToMenu: return 3;
        }
        return 0;
    }

    static EditorPauseChoice ChoiceAt(int index) {
        switch (index) {
        case 0: return EditorPauseChoice::StartTrial;
        case 1: return EditorPauseChoice::SaveDraft;
        case 2: return EditorPauseChoice::OpenDraft;
        case 3: return EditorPauseChoice::QuitToMenu;
        }
        return EditorPauseChoice::SaveDraft;
    }

    static EditorPauseAction ActionFor(EditorPauseChoice choice) {
        switch (choice) {
        case EditorPauseChoice::StartTrial: return EditorPauseAction::StartTrial;
        case EditorPauseChoice::SaveDraft: return EditorPauseAction::SaveDraft;
        case EditorPauseChoice::OpenDraft: return EditorPauseAction::OpenDraft;
        case EditorPauseChoice::QuitToMenu: return EditorPauseAction::QuitToMenu;
        }
        return EditorPauseAction::None;
    }

    void MoveSelection(int direction) {
        const int start = ChoiceIndex(selectedChoice_);
        for (int offset = 1; offset <= 4; ++offset) {
            int index = (start + direction * offset) % 4;
            if (index < 0) index += 4;
            const EditorPauseChoice choice = ChoiceAt(index);
            if (IsChoiceEnabled(choice)) {
                Select(choice);
                return;
            }
        }
    }

    EditorPauseChoice selectedChoice_;
    EditorPauseAction pendingAction_;
    bool canStartTrial_;
    bool hasSavedDraft_;
    bool confirmingQuit_;
};
