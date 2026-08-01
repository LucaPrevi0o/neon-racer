#include "../../src/app/race_pause_flow.hpp"

#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestNavigationWrapsAndCancelsConfirmation() {
    RacePauseFlow flow;
    Expect(flow.SelectedChoice() == RacePauseChoice::Resume,
           "pause menu starts on the safe resume action");

    flow.SelectPrevious();
    Expect(flow.SelectedChoice() == RacePauseChoice::Quit,
           "previous selection wraps from resume to quit");
    flow.SelectNext();
    Expect(flow.SelectedChoice() == RacePauseChoice::Resume,
           "next selection wraps from quit to resume");

    flow.Select(RacePauseChoice::Restart);
    flow.ActivateSelectedChoice();
    Expect(flow.IsConfirming() && flow.ConsumeAction() == RacePauseAction::None,
           "restart requires a second confirmation");
    flow.SelectNext();
    Expect(!flow.IsConfirming() && flow.SelectedChoice() == RacePauseChoice::Return,
           "moving away from a destructive action cancels its confirmation");
}

void TestImmediateActionsAndBack() {
    RacePauseFlow flow;
    flow.Select(RacePauseChoice::Recover);
    flow.ActivateSelectedChoice();
    Expect(flow.ConsumeAction() == RacePauseAction::Recover,
           "checkpoint recovery activates immediately");
    Expect(flow.ConsumeAction() == RacePauseAction::None,
           "a pause action is consumed once");

    flow.Reset();
    flow.CancelOrResume();
    Expect(flow.ConsumeAction() == RacePauseAction::Resume,
           "back resumes when no confirmation is active");
}

void TestDestructiveActionsRequireTwoActivations() {
    const RacePauseChoice choices[] = {
        RacePauseChoice::Restart,
        RacePauseChoice::Return,
        RacePauseChoice::Quit,
    };
    const RacePauseAction actions[] = {
        RacePauseAction::Restart,
        RacePauseAction::Return,
        RacePauseAction::Quit,
    };

    for (int index = 0; index < 3; ++index) {
        RacePauseFlow flow;
        flow.Select(choices[index]);
        flow.ActivateSelectedChoice();
        Expect(flow.IsConfirming() && flow.ConfirmationChoice() == choices[index] &&
                   flow.ConsumeAction() == RacePauseAction::None,
               "first destructive activation arms confirmation without acting");
        flow.ActivateSelectedChoice();
        Expect(!flow.IsConfirming() && flow.ConsumeAction() == actions[index],
               "second destructive activation emits the selected action");
    }
}

void TestBackCancelsConfirmationBeforeResuming() {
    RacePauseFlow flow;
    flow.Select(RacePauseChoice::Quit);
    flow.ActivateSelectedChoice();
    flow.CancelOrResume();
    Expect(!flow.IsConfirming() && flow.ConsumeAction() == RacePauseAction::None,
           "back cancels destructive confirmation without closing the menu");
    flow.CancelOrResume();
    Expect(flow.ConsumeAction() == RacePauseAction::Resume,
           "a second back request resumes after confirmation was cancelled");
}

} // namespace

int main() {
    TestNavigationWrapsAndCancelsConfirmation();
    TestImmediateActionsAndBack();
    TestDestructiveActionsRequireTwoActivations();
    TestBackCancelsConfirmationBeforeResuming();
    if (failures == 0) std::cout << "Neon Racer race-pause flow tests passed.\n";
    return failures == 0 ? 0 : 1;
}
