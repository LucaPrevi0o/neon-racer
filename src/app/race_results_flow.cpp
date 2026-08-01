#include "race_results_flow.hpp"

namespace {

const RaceResultsChoice kChoices[] = {
    RaceResultsChoice::RaceAgain,
    RaceResultsChoice::SavePlayable,
    RaceResultsChoice::Return,
    RaceResultsChoice::Quit,
};

int ChoiceIndex(RaceResultsChoice choice) {
    for (int index = 0; index < 4; ++index) {
        if (kChoices[index] == choice) return index;
    }
    return 0;
}

} // namespace

RaceResultsFlow::RaceResultsFlow()
    : selectedChoice_(RaceResultsChoice::RaceAgain),
      confirmationChoice_(RaceResultsChoice::Return),
      pendingAction_(RaceResultsAction::None),
      canSavePlayable_(false),
      confirming_(false) {
}

void RaceResultsFlow::Reset(bool canSavePlayable) {
    selectedChoice_ = RaceResultsChoice::RaceAgain;
    confirmationChoice_ = RaceResultsChoice::Return;
    pendingAction_ = RaceResultsAction::None;
    canSavePlayable_ = canSavePlayable;
    confirming_ = false;
}

RaceResultsChoice RaceResultsFlow::SelectedChoice() const { return selectedChoice_; }

void RaceResultsFlow::SelectPrevious() { MoveSelection(-1); }
void RaceResultsFlow::SelectNext() { MoveSelection(1); }

void RaceResultsFlow::Select(RaceResultsChoice choice) {
    if (!IsChoiceEnabled(choice)) return;
    selectedChoice_ = choice;
    confirming_ = false;
}

void RaceResultsFlow::ActivateSelectedChoice() {
    if (!IsChoiceEnabled(selectedChoice_)) return;

    if (NeedsConfirmation(selectedChoice_)) {
        if (!confirming_ || confirmationChoice_ != selectedChoice_) {
            confirmationChoice_ = selectedChoice_;
            confirming_ = true;
            return;
        }
    }

    pendingAction_ = ActionFor(selectedChoice_);
    confirming_ = false;
}

void RaceResultsFlow::Back() {
    if (confirming_) {
        confirming_ = false;
        return;
    }

    selectedChoice_ = RaceResultsChoice::Return;
    ActivateSelectedChoice();
}

bool RaceResultsFlow::CanSavePlayable() const { return canSavePlayable_; }

bool RaceResultsFlow::IsChoiceEnabled(RaceResultsChoice choice) const {
    return choice != RaceResultsChoice::SavePlayable || canSavePlayable_;
}

bool RaceResultsFlow::IsConfirming() const { return confirming_; }
RaceResultsChoice RaceResultsFlow::ConfirmationChoice() const { return confirmationChoice_; }

RaceResultsAction RaceResultsFlow::ConsumeAction() {
    const RaceResultsAction action = pendingAction_;
    pendingAction_ = RaceResultsAction::None;
    return action;
}

bool RaceResultsFlow::NeedsConfirmation(RaceResultsChoice choice) {
    return choice == RaceResultsChoice::Return || choice == RaceResultsChoice::Quit;
}

RaceResultsAction RaceResultsFlow::ActionFor(RaceResultsChoice choice) {
    switch (choice) {
    case RaceResultsChoice::RaceAgain: return RaceResultsAction::RaceAgain;
    case RaceResultsChoice::SavePlayable: return RaceResultsAction::SavePlayable;
    case RaceResultsChoice::Return: return RaceResultsAction::Return;
    case RaceResultsChoice::Quit: return RaceResultsAction::Quit;
    }
    return RaceResultsAction::None;
}

void RaceResultsFlow::MoveSelection(int direction) {
    const int start = ChoiceIndex(selectedChoice_);
    for (int offset = 1; offset <= 4; ++offset) {
        int index = (start + direction * offset) % 4;
        if (index < 0) index += 4;
        if (IsChoiceEnabled(kChoices[index])) {
            selectedChoice_ = kChoices[index];
            confirming_ = false;
            return;
        }
    }
}
