#include "../../src/app/app_settings.hpp"
#include "../../src/app/main_menu_flow.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestReleaseIdentity() {
    Expect(std::string(AppSettings::kVersion) == "v0.2.2-alpha.4",
           "the application exposes the alpha.4 release identity");
}

void TestDefaultSelectionAndNavigation() {
    MainMenuFlow menu;
    Expect(menu.SelectedChoice() == MainMenuChoice::CreateNewTrack,
           "a first-run menu defaults to creating a new track");

    menu.SelectPrevious();
    Expect(menu.SelectedChoice() == MainMenuChoice::PlayCompleteTrack,
           "previous selection wraps to the playable-track option");

    menu.SelectNext();
    Expect(menu.SelectedChoice() == MainMenuChoice::CreateNewTrack,
           "next selection wraps back to the new-track option");

    menu.Select(MainMenuChoice::PlayCompleteTrack);
    Expect(menu.SelectedChoice() == MainMenuChoice::PlayCompleteTrack,
           "pointer selection can choose a menu card directly");
}

void TestActionsAreMappedAndConsumedOnce() {
    MainMenuFlow menu;
    Expect(menu.ConsumeAction() == MainMenuAction::None,
           "a menu does not produce an action before activation");

    menu.ActivateSelectedChoice();
    Expect(menu.ConsumeAction() == MainMenuAction::BeginNewTrack,
           "new-track selection requests a fresh editor session");
    Expect(menu.ConsumeAction() == MainMenuAction::None,
           "a consumed new-track action is not repeated on the next frame");

    menu.Select(MainMenuChoice::PlayCompleteTrack);
    menu.ActivateSelectedChoice();
    Expect(menu.ConsumeAction() == MainMenuAction::OpenPlayableLibrary,
           "play selection requests the saved playable-track library");
    Expect(menu.ConsumeAction() == MainMenuAction::None,
           "a consumed library action is not repeated on the next frame");
}

} // namespace

int main() {
    TestReleaseIdentity();
    TestDefaultSelectionAndNavigation();
    TestActionsAreMappedAndConsumedOnce();
    if (failures == 0) std::cout << "Neon Racer main-menu flow tests passed.\n";
    return failures == 0 ? 0 : 1;
}
