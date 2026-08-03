#include "main_menu_flow.hpp"

MainMenuFlow::MainMenuFlow()
    : selectedChoice_(MainMenuChoice::OpenTrackEditor), pendingAction_(MainMenuAction::None) {
}

MainMenuChoice MainMenuFlow::SelectedChoice() const { return selectedChoice_; }

void MainMenuFlow::SelectPrevious() {
    selectedChoice_ = selectedChoice_ == MainMenuChoice::PlayCompleteTrack
        ? MainMenuChoice::OpenTrackEditor : MainMenuChoice::PlayCompleteTrack;
}

void MainMenuFlow::SelectNext() {
    selectedChoice_ = selectedChoice_ == MainMenuChoice::PlayCompleteTrack
        ? MainMenuChoice::OpenTrackEditor : MainMenuChoice::PlayCompleteTrack;
}

void MainMenuFlow::Select(MainMenuChoice choice) { selectedChoice_ = choice; }

void MainMenuFlow::ActivateSelectedChoice() {
    pendingAction_ = selectedChoice_ == MainMenuChoice::PlayCompleteTrack
        ? MainMenuAction::OpenPlayableLibrary : MainMenuAction::OpenTrackEditor;
}

MainMenuAction MainMenuFlow::ConsumeAction() {
    const MainMenuAction action = pendingAction_;
    pendingAction_ = MainMenuAction::None;
    return action;
}
