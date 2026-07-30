#pragma once

#include "main_menu_flow.hpp"

// Raylib-facing startup-menu view. It gathers player input and presents the
// current choice, but leaves application-state changes to RacerApplication.
class MainMenu {
public:
    MainMenu();

    void Update();
    void Draw() const;
    MainMenuAction ConsumeAction();

private:
    MainMenuFlow flow_;
    bool hasHoveredChoice_;
    MainMenuChoice hoveredChoice_;
};
