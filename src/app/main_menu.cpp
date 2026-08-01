#include "main_menu.hpp"

#include "app_settings.hpp"
#include "../ui/neon.hpp"

#include <raylib.h>

namespace {

const float kOptionX = 120.0f;
const float kOptionWidth = 520.0f;
const float kOptionHeight = 86.0f;
const float kGuideX = 690.0f;
const float kGuideY = 238.0f;
const float kGuideWidth = 470.0f;
const float kGuideHeight = 294.0f;

Rectangle OptionBounds(MainMenuChoice choice) {
    const float y = choice == MainMenuChoice::PlayCompleteTrack ? 272.0f : 378.0f;
    return Rectangle{kOptionX, y, kOptionWidth, kOptionHeight};
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

void DrawChoice(MainMenuChoice choice, bool selected) {
    const Rectangle bounds = OptionBounds(choice);
    const Neon::ButtonState state = Neon::GetButtonState(bounds, true, selected);
    const bool highlighted = Neon::IsButtonHighlighted(state);
    const Color color = highlighted ? Neon::Yellow : ChoiceColor(choice);
    Neon::DrawButton(bounds, color, state);

    DrawText(ChoiceTitle(choice), static_cast<int>(bounds.x) + 28, static_cast<int>(bounds.y) + 18,
             24, color);
    DrawText(ChoiceDetail(choice), static_cast<int>(bounds.x) + 29, static_cast<int>(bounds.y) + 51,
             16, highlighted ? RAYWHITE : Fade(RAYWHITE, 0.72f));
}

void DrawGuideStep(int number, int y, Color accent, const char* title, const char* detail) {
    const int markerX = static_cast<int>(kGuideX) + 32;
    const int markerY = y + 13;
    DrawCircle(markerX, markerY, 14.0f, Fade(accent, 0.18f));
    DrawCircleLines(markerX, markerY, 14.0f, accent);
    const char* numberText = TextFormat("%i", number);
    DrawText(numberText, markerX - MeasureText(numberText, 14) / 2, markerY - 7, 14, accent);
    DrawText(title, static_cast<int>(kGuideX) + 58, y, 15, accent);
    DrawText(detail, static_cast<int>(kGuideX) + 58, y + 18, 13, Fade(RAYWHITE, 0.78f));
}

void DrawHowToPlay() {
    const Rectangle bounds{kGuideX, kGuideY, kGuideWidth, kGuideHeight};
    Neon::DrawOverlayPanel(bounds, 0.88f);
    DrawRectangleLinesEx(bounds, 1.5f, Fade(Neon::Cyan, 0.62f));
    DrawText("HOW TO PLAY", static_cast<int>(kGuideX) + 20, static_cast<int>(kGuideY) + 18, 22, Neon::Cyan);
    DrawText("From an empty grid to a finished lap.", static_cast<int>(kGuideX) + 20,
             static_cast<int>(kGuideY) + 47, 14, Fade(RAYWHITE, 0.70f));
    DrawLine(static_cast<int>(kGuideX) + 20, static_cast<int>(kGuideY) + 70,
             static_cast<int>(kGuideX + kGuideWidth) - 20, static_cast<int>(kGuideY) + 70,
             Fade(Neon::Cyan, 0.34f));

    DrawGuideStep(1, static_cast<int>(kGuideY) + 86, Neon::Pink, "BUILD",
                  "Choose a piece, click the grid; wheel/R rotates.");
    DrawGuideStep(2, static_cast<int>(kGuideY) + 130, Neon::Yellow, "SHAPE",
                  "Select with Shift+click; use arrows to adjust it.");
    DrawGuideStep(3, static_cast<int>(kGuideY) + 174, Neon::Cyan, "CONNECT",
                  "Match cyan IN to pink OUT, then close the loop.");
    DrawGuideStep(4, static_cast<int>(kGuideY) + 218, Neon::Green, "RACE",
                  "Set start/finish on a straight, then press Tab.");

    DrawText("Camera: WASD/QE move  |  right-drag orbit  |  Shift+wheel zoom",
             static_cast<int>(kGuideX) + 20, static_cast<int>(kGuideY) + 270, 12,
             Fade(RAYWHITE, 0.62f));
}

} // namespace

MainMenu::MainMenu()
    : flow_(), hasHoveredChoice_(false), hoveredChoice_(MainMenuChoice::CreateNewTrack) {
}

void MainMenu::Update() {
    const Vector2 mouse = GetMousePosition();
    hasHoveredChoice_ = ChoiceAt(mouse, hoveredChoice_);

    // Hovering a card should move the active choice as well as brighten the
    // card, so Enter/Space follows the mouse without requiring a click.
    if (hasHoveredChoice_) flow_.Select(hoveredChoice_);

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
    Neon::DrawCenteredText("NEON RACER", AppSettings::kWindowWidth, 76, 46, Neon::Cyan);
    Neon::DrawCenteredText("BUILD | RACE | REPLAY", AppSettings::kWindowWidth, 136, 18,
                           Fade(RAYWHITE, 0.80f));
    Neon::DrawCenteredText("CHOOSE YOUR STARTING POINT", AppSettings::kWindowWidth, 198, 16,
                           Neon::Pink);

    DrawText("START HERE", static_cast<int>(kOptionX), 242, 15, Neon::Pink);

    const MainMenuChoice selectedChoice = flow_.SelectedChoice();
    DrawChoice(MainMenuChoice::PlayCompleteTrack, selectedChoice == MainMenuChoice::PlayCompleteTrack);
    DrawChoice(MainMenuChoice::CreateNewTrack, selectedChoice == MainMenuChoice::CreateNewTrack);
    DrawHowToPlay();

    Neon::DrawCenteredText("Click a card to begin  |  W/S or Up/Down: select  |  Enter/Space: choose",
                           AppSettings::kWindowWidth, 584, 15, Neon::Yellow);
    Neon::DrawCenteredText("ESC: quit", AppSettings::kWindowWidth, 616, 14, Fade(RAYWHITE, 0.66f));
    DrawText(AppSettings::kVersion, 20, AppSettings::kWindowHeight - 28, 12,
             Fade(RAYWHITE, 0.48f));
}

MainMenuAction MainMenu::ConsumeAction() { return flow_.ConsumeAction(); }
