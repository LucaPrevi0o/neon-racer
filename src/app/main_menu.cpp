#include "main_menu.hpp"

#include "app_settings.hpp"
#include "../ui/neon.hpp"

#include <raylib.h>

namespace {

const float kOptionWidth = 580.0f;
const float kOptionHeight = 86.0f;

Rectangle OptionBounds(MainMenuChoice choice) {
    const float x = (static_cast<float>(AppSettings::kWindowWidth) - kOptionWidth) * 0.5f;
    const float y = choice == MainMenuChoice::PlayCompleteTrack ? 280.0f : 386.0f;
    return Rectangle{x, y, kOptionWidth, kOptionHeight};
}

Color ChoiceColor(MainMenuChoice choice) {
    return choice == MainMenuChoice::PlayCompleteTrack ? Neon::Cyan : Neon::Green;
}

const char* ChoiceTitle(MainMenuChoice choice) {
    return choice == MainMenuChoice::PlayCompleteTrack ? "PLAY A COMPLETE TRACK" : "CREATE A NEW TRACK";
}

const char* ChoiceDetail(MainMenuChoice choice) {
    return choice == MainMenuChoice::PlayCompleteTrack
        ? "Choose a saved verified time trial."
        : "Start with an empty track editor.";
}

bool ChoiceAt(Vector2 point, MainMenuChoice& choice) {
    if (CheckCollisionPointRec(point, OptionBounds(MainMenuChoice::PlayCompleteTrack))) {
        choice = MainMenuChoice::PlayCompleteTrack;
        return true;
    }
    if (CheckCollisionPointRec(point, OptionBounds(MainMenuChoice::CreateNewTrack))) {
        choice = MainMenuChoice::CreateNewTrack;
        return true;
    }
    return false;
}

void DrawChoice(MainMenuChoice choice, bool selected, bool hovered) {
    const Rectangle bounds = OptionBounds(choice);
    const bool highlighted = selected || hovered;
    const Color color = highlighted ? Neon::Yellow : ChoiceColor(choice);
    Neon::DrawNeonBlock(bounds, color, highlighted ? 1.0f : 0.68f);

    DrawText(ChoiceTitle(choice), static_cast<int>(bounds.x) + 28, static_cast<int>(bounds.y) + 18,
             24, color);
    DrawText(ChoiceDetail(choice), static_cast<int>(bounds.x) + 29, static_cast<int>(bounds.y) + 51,
             16, highlighted ? RAYWHITE : Fade(RAYWHITE, 0.72f));
}

} // namespace

MainMenu::MainMenu()
    : flow_(), hasHoveredChoice_(false), hoveredChoice_(MainMenuChoice::CreateNewTrack) {
}

void MainMenu::Update() {
    const Vector2 mouse = GetMousePosition();
    hasHoveredChoice_ = ChoiceAt(mouse, hoveredChoice_);

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) flow_.SelectPrevious();
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) flow_.SelectNext();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hasHoveredChoice_) {
        flow_.Select(hoveredChoice_);
        flow_.ActivateSelectedChoice();
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) flow_.ActivateSelectedChoice();
}

void MainMenu::Draw() const {
    Neon::DrawCenteredText("NEON RACER", AppSettings::kWindowWidth, 104, 46, Neon::Cyan);
    Neon::DrawCenteredText("BUILD | RACE | REPLAY", AppSettings::kWindowWidth, 164, 18,
                           Fade(RAYWHITE, 0.80f));
    Neon::DrawCenteredText("CHOOSE YOUR STARTING POINT", AppSettings::kWindowWidth, 226, 16,
                           Neon::Pink);

    const MainMenuChoice selectedChoice = flow_.SelectedChoice();
    DrawChoice(MainMenuChoice::PlayCompleteTrack, selectedChoice == MainMenuChoice::PlayCompleteTrack,
               hasHoveredChoice_ && hoveredChoice_ == MainMenuChoice::PlayCompleteTrack);
    DrawChoice(MainMenuChoice::CreateNewTrack, selectedChoice == MainMenuChoice::CreateNewTrack,
               hasHoveredChoice_ && hoveredChoice_ == MainMenuChoice::CreateNewTrack);

    Neon::DrawCenteredText("UP/DOWN or W/S: select    ENTER/SPACE: choose", AppSettings::kWindowWidth,
                           542, 16, Neon::Yellow);
    Neon::DrawCenteredText("Mouse: hover and click    ESC: quit", AppSettings::kWindowWidth,
                           578, 15, Fade(RAYWHITE, 0.72f));
}

MainMenuAction MainMenu::ConsumeAction() { return flow_.ConsumeAction(); }
