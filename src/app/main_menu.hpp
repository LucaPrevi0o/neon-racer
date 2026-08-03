#pragma once

#include "main_menu_flow.hpp"

// Raylib-facing Home-menu view. It gathers player input and presents the
// current choice, but leaves application-state changes and editor-session
// lifetime to RacerApplication.
class MainMenu {
public:
    MainMenu();

    void Update();
    void Draw(bool hasEditorSession) const;
    MainMenuAction ConsumeAction();

private:
    MainMenuFlow flow_;
    bool hasHoveredChoice_;
    MainMenuChoice hoveredChoice_;
};
