#include "race_pause_flow.hpp"

namespace {

const int kChoiceCount = 5;

int ChoiceIndex(RacePauseChoice choice) {
    switch (choice) {
    case RacePauseChoice::Resume: return 0;
    case RacePauseChoice::Recover: return 1;
    case RacePauseChoice::Restart: return 2;
    case RacePauseChoice::Return: return 3;
    case RacePauseChoice::Quit: return 4;
    }
    return 0;
}

RacePauseChoice ChoiceAt(int index) {
    switch (index) {
    case 0: return RacePauseChoice::Resume;
    case 1: return RacePauseChoice::Recover;
    case 2: return RacePauseChoice::Restart;
    case 3: return RacePauseChoice::Return;
    case 4: return RacePauseChoice::Quit;
    }
    return RacePauseChoice::Resume;
}

} // namespace

RacePauseFlow::RacePauseFlow()
    : selectedChoice_(RacePauseChoice::Resume),
      confirmationChoice_(RacePauseChoice::Resume),
      pendingAction_(RacePauseAction::None),
      confirming_(false) {
}

void RacePauseFlow::Reset() {
    selectedChoice_ = RacePauseChoice::Resume;
    confirmationChoice_ = RacePauseChoice::Resume;
    pendingAction_ = RacePauseAction::None;
    confirming_ = false;
}

RacePauseChoice RacePauseFlow::SelectedChoice() const { return selectedChoice_; }

void RacePauseFlow::SelectPrevious() {
    const int index = (ChoiceIndex(selectedChoice_) + kChoiceCount - 1) % kChoiceCount;
    Select(ChoiceAt(index));
}

void RacePauseFlow::SelectNext() {
    const int index = (ChoiceIndex(selectedChoice_) + 1) % kChoiceCount;
    Select(ChoiceAt(index));
}

void RacePauseFlow::Select(RacePauseChoice choice) {
    if (choice != selectedChoice_) confirming_ = false;
    selectedChoice_ = choice;
}

void RacePauseFlow::ActivateSelectedChoice() {
    if (!NeedsConfirmation(selectedChoice_)) {
        pendingAction_ = ActionFor(selectedChoice_);
        confirming_ = false;
        return;
    }

    if (confirming_ && confirmationChoice_ == selectedChoice_) {
        pendingAction_ = ActionFor(selectedChoice_);
        confirming_ = false;
        return;
    }

    confirmationChoice_ = selectedChoice_;
    confirming_ = true;
}

void RacePauseFlow::CancelOrResume() {
    if (confirming_) {
        confirming_ = false;
        return;
    }
    pendingAction_ = RacePauseAction::Resume;
}

bool RacePauseFlow::IsConfirming() const { return confirming_; }
RacePauseChoice RacePauseFlow::ConfirmationChoice() const { return confirmationChoice_; }

RacePauseAction RacePauseFlow::ConsumeAction() {
    const RacePauseAction action = pendingAction_;
    pendingAction_ = RacePauseAction::None;
    return action;
}

bool RacePauseFlow::NeedsConfirmation(RacePauseChoice choice) {
    return choice == RacePauseChoice::Restart || choice == RacePauseChoice::Return ||
           choice == RacePauseChoice::Quit;
}

RacePauseAction RacePauseFlow::ActionFor(RacePauseChoice choice) {
    switch (choice) {
    case RacePauseChoice::Resume: return RacePauseAction::Resume;
    case RacePauseChoice::Recover: return RacePauseAction::Recover;
    case RacePauseChoice::Restart: return RacePauseAction::Restart;
    case RacePauseChoice::Return: return RacePauseAction::Return;
    case RacePauseChoice::Quit: return RacePauseAction::Quit;
    }
    return RacePauseAction::None;
}
