#include "../../src/app/app_settings.hpp"
#include "../../src/app/editor_pause_flow.hpp"
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
    Expect(std::string(AppSettings::kVersion) == "v0.2.2-alpha.5.3",
           "the application exposes the alpha.5.3 release identity");
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

void TestEditorPauseSkipsUnavailableTrial() {
    EditorPauseFlow flow;
    flow.Reset(false, false);
    Expect(!flow.CanStartTrial() && flow.SelectedChoice() == EditorPauseChoice::SaveDraft,
           "an incomplete track starts on the first enabled editor action");

    flow.Select(EditorPauseChoice::StartTrial);
    Expect(flow.SelectedChoice() == EditorPauseChoice::SaveDraft,
           "pointer selection cannot focus disabled Start Trial");

    flow.SelectPrevious();
    Expect(flow.SelectedChoice() == EditorPauseChoice::QuitToMenu,
           "reverse navigation skips disabled Start Trial");
    flow.SelectNext();
    Expect(flow.SelectedChoice() == EditorPauseChoice::SaveDraft,
           "forward navigation also skips disabled Start Trial");
}

void TestEditorPauseMapsImmediateActions() {
    EditorPauseFlow flow;
    flow.Reset(true, true);
    Expect(flow.SelectedChoice() == EditorPauseChoice::StartTrial,
           "a race-ready track defaults to Start Trial");
    flow.ActivateSelectedChoice();
    Expect(flow.ConsumeAction() == EditorPauseAction::StartTrial,
           "Start Trial emits its application action immediately");

    flow.Reset(true, true);
    flow.Select(EditorPauseChoice::SaveDraft);
    flow.ActivateSelectedChoice();
    Expect(flow.ConsumeAction() == EditorPauseAction::SaveDraft,
           "Save Draft emits its application action immediately");

    flow.Reset(true, true);
    flow.Select(EditorPauseChoice::OpenDraft);
    flow.ActivateSelectedChoice();
    Expect(flow.ConsumeAction() == EditorPauseAction::OpenDraft,
           "Open Draft emits its application action immediately");

    flow.Reset(true, true);
    flow.CancelAlertOrResume();
    Expect(flow.ConsumeAction() == EditorPauseAction::Resume,
           "back resumes editing when no alert is open");
}

void TestUnsavedQuitUsesOkCancelAlert() {
    EditorPauseFlow flow;
    flow.Reset(true, false);
    flow.Select(EditorPauseChoice::QuitToMenu);
    flow.ActivateSelectedChoice();
    Expect(flow.IsConfirmingQuit() && flow.ConsumeAction() == EditorPauseAction::None,
           "the first unsaved Quit activation opens the confirmation alert");

    flow.CancelQuitAlert();
    Expect(!flow.IsConfirmingQuit() && flow.ConsumeAction() == EditorPauseAction::None,
           "Cancel closes the unsaved-session alert without leaving");

    flow.ActivateSelectedChoice();
    flow.ActivateSelectedChoice();
    Expect(!flow.IsConfirmingQuit() && flow.ConsumeAction() == EditorPauseAction::QuitToMenu,
           "OK confirms discarding a never-saved editor session");

    flow.Reset(true, true);
    flow.Select(EditorPauseChoice::QuitToMenu);
    flow.ActivateSelectedChoice();
    Expect(!flow.IsConfirmingQuit() && flow.ConsumeAction() == EditorPauseAction::QuitToMenu,
           "a draft with an established save identity can quit immediately");
}

} // namespace

int main() {
    TestReleaseIdentity();
    TestDefaultSelectionAndNavigation();
    TestActionsAreMappedAndConsumedOnce();
    TestEditorPauseSkipsUnavailableTrial();
    TestEditorPauseMapsImmediateActions();
    TestUnsavedQuitUsesOkCancelAlert();
    if (failures == 0) std::cout << "Neon Racer main and editor menu flow tests passed.\n";
    return failures == 0 ? 0 : 1;
}
