#include "../../src/app/race_results_flow.hpp"

#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestSaveChoiceIsSkippedWhenUnavailable() {
    RaceResultsFlow flow;
    flow.Reset(false);
    Expect(flow.SelectedChoice() == RaceResultsChoice::RaceAgain,
           "results start on the safe race-again action");

    flow.SelectNext();
    Expect(flow.SelectedChoice() == RaceResultsChoice::Return,
           "navigation skips unavailable playable export");

    flow.SelectPrevious();
    Expect(flow.SelectedChoice() == RaceResultsChoice::RaceAgain,
           "reverse navigation also skips unavailable export");

    flow.Select(RaceResultsChoice::SavePlayable);
    Expect(flow.SelectedChoice() == RaceResultsChoice::RaceAgain,
           "pointer selection cannot focus a disabled export action");
}

void TestImmediateActionsAreConsumedOnce() {
    RaceResultsFlow flow;
    flow.Reset(true);

    flow.ActivateSelectedChoice();
    Expect(flow.ConsumeAction() == RaceResultsAction::RaceAgain,
           "race again activates immediately");
    Expect(flow.ConsumeAction() == RaceResultsAction::None,
           "a consumed race-again action is not repeated");

    flow.Select(RaceResultsChoice::SavePlayable);
    flow.ActivateSelectedChoice();
    Expect(flow.ConsumeAction() == RaceResultsAction::SavePlayable,
           "verified playable export activates immediately");
}

void TestDestructiveActionsRequireConfirmation() {
    const RaceResultsChoice choices[] = {
        RaceResultsChoice::Return,
        RaceResultsChoice::Quit,
    };
    const RaceResultsAction actions[] = {
        RaceResultsAction::Return,
        RaceResultsAction::Quit,
    };

    for (int index = 0; index < 2; ++index) {
        RaceResultsFlow flow;
        flow.Reset(true);
        flow.Select(choices[index]);
        flow.ActivateSelectedChoice();
        Expect(flow.IsConfirming() && flow.ConfirmationChoice() == choices[index] &&
                   flow.ConsumeAction() == RaceResultsAction::None,
               "first destructive activation only arms confirmation");
        flow.ActivateSelectedChoice();
        Expect(!flow.IsConfirming() && flow.ConsumeAction() == actions[index],
               "second destructive activation emits the action");
    }
}

void TestBackSafelyReturns() {
    RaceResultsFlow flow;
    flow.Reset(true);
    flow.Back();
    Expect(flow.SelectedChoice() == RaceResultsChoice::Return && flow.IsConfirming() &&
               flow.ConsumeAction() == RaceResultsAction::None,
           "first back request arms contextual return");
    flow.Back();
    Expect(!flow.IsConfirming() && flow.ConsumeAction() == RaceResultsAction::None,
           "back cancels an active confirmation");
    flow.Back();
    flow.ActivateSelectedChoice();
    Expect(flow.ConsumeAction() == RaceResultsAction::Return,
           "confirming the armed return leaves the race results");
}

} // namespace

int main() {
    TestSaveChoiceIsSkippedWhenUnavailable();
    TestImmediateActionsAreConsumedOnce();
    TestDestructiveActionsRequireConfirmation();
    TestBackSafelyReturns();
    if (failures == 0) std::cout << "Neon Racer race-results flow tests passed.\n";
    return failures == 0 ? 0 : 1;
}
