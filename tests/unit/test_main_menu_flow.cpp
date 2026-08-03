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
    Expect(std::string(AppSettings::kVersion) == "v0.2.2-alpha.5",
           "the application exposes the alpha.5 release identity");
}

void TestDefaultSelectionAndNavigation() {
    MainMenuFlow menu;
    Expect(menu.SelectedChoice() == MainMenuChoice::OpenTrackEditor,
           "Home defaults to the track-editor destination");

    menu.SelectPrevious();
    Expect(menu.SelectedChoice() == MainMenuChoice::PlayCompleteTrack,
           "previous selection wraps to the playable-track destination");

    menu.SelectNext();
    Expect(menu.SelectedChoice() == MainMenuChoice::OpenTrackEditor,
           "next selection wraps back to the editor destination");

    menu.Select(MainMenuChoice::PlayCompleteTrack);
    Expect(menu.SelectedChoice() == MainMenuChoice::PlayCompleteTrack,
           "pointer selection can choose a Home card directly");
}

void TestActionsAreMappedAndConsumedOnce() {
    MainMenuFlow menu;
    Expect(menu.ConsumeAction() == MainMenuAction::None,
           "Home does not produce an action before activation");

    menu.ActivateSelectedChoice();
    Expect(menu.ConsumeAction() == MainMenuAction::OpenTrackEditor,
           "the editor destination requests entry without deciding session lifetime");
    Expect(menu.ConsumeAction() == MainMenuAction::None,
           "a consumed editor action is not repeated on the next frame");

    menu.Select(MainMenuChoice::PlayCompleteTrack);
    menu.ActivateSelectedChoice();
    Expect(menu.ConsumeAction() == MainMenuAction::OpenPlayableLibrary,
           "the play destination requests the saved playable-track library");
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
