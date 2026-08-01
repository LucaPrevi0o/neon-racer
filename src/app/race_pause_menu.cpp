#include "race_pause_menu.hpp"

#include "../ui/neon.hpp"

#include <raylib.h>

namespace {

const RacePauseChoice kChoices[] = {
    RacePauseChoice::Resume,
    RacePauseChoice::Recover,
    RacePauseChoice::Restart,
    RacePauseChoice::Return,
    RacePauseChoice::Quit,
};

const float kPanelX = 430.0f;
const float kPanelY = 76.0f;
const float kPanelWidth = 420.0f;
const float kPanelHeight = 568.0f;
const float kButtonX = 474.0f;
const float kButtonY = 180.0f;
const float kButtonWidth = 332.0f;
const float kButtonHeight = 52.0f;
const float kButtonGap = 12.0f;

Rectangle ChoiceBounds(int index) {
    return Rectangle{kButtonX, kButtonY + static_cast<float>(index) * (kButtonHeight + kButtonGap),
                     kButtonWidth, kButtonHeight};
}

int ChoiceIndex(RacePauseChoice choice) {
    for (int index = 0; index < 5; ++index) {
        if (kChoices[index] == choice) return index;
    }
    return 0;
}

bool ChoiceAt(Vector2 point, RacePauseChoice& choice) {
    for (int index = 0; index < 5; ++index) {
        if (CheckCollisionPointRec(point, ChoiceBounds(index))) {
            choice = kChoices[index];
            return true;
        }
    }
    return false;
}

const char* ChoiceTitle(RacePauseChoice choice, bool returnsToMainMenu) {
    switch (choice) {
    case RacePauseChoice::Resume: return "RESUME RACE";
    case RacePauseChoice::Recover: return "RECOVER TO CHECKPOINT";
    case RacePauseChoice::Restart: return "RESTART RUN";
    case RacePauseChoice::Return: return returnsToMainMenu ? "RETURN TO MAIN MENU" : "RETURN TO EDITOR";
    case RacePauseChoice::Quit: return "QUIT NEON RACER";
    }
    return "RESUME RACE";
}

Color ChoiceColor(RacePauseChoice choice) {
    switch (choice) {
    case RacePauseChoice::Resume: return Neon::Green;
    case RacePauseChoice::Recover: return Neon::Cyan;
    case RacePauseChoice::Restart: return Neon::Yellow;
    case RacePauseChoice::Return: return Neon::Pink;
    case RacePauseChoice::Quit: return Neon::Orange;
    }
    return Neon::Cyan;
}

const char* ConfirmationPrompt(RacePauseChoice choice, bool returnsToMainMenu) {
    switch (choice) {
    case RacePauseChoice::Restart: return "Press confirm again to restart the complete run.";
    case RacePauseChoice::Return:
        return returnsToMainMenu
            ? "Press confirm again to abandon the run and return home."
            : "Press confirm again to abandon the run and return to the editor.";
    case RacePauseChoice::Quit: return "Press confirm again to quit Neon Racer.";
    default: return "";
    }
}

} // namespace

RacePauseMenu::RacePauseMenu() : flow_(), open_(false), returnsToMainMenu_(true) {}

void RacePauseMenu::Open(bool returnsToMainMenu) {
    returnsToMainMenu_ = returnsToMainMenu;
    flow_.Reset();
    open_ = true;
}

void RacePauseMenu::Close() { open_ = false; }
bool RacePauseMenu::IsOpen() const { return open_; }

void RacePauseMenu::Update() {
    if (!open_) return;

    RacePauseChoice hovered = flow_.SelectedChoice();
    const bool hasHoveredChoice = ChoiceAt(GetMousePosition(), hovered);
    if (hasHoveredChoice) flow_.Select(hovered);

    const bool previousPressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP);
    const bool nextPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    if (previousPressed) flow_.SelectPrevious();
    if (nextPressed) flow_.SelectNext();

    const bool backPressed = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    if (backPressed) {
        flow_.CancelOrResume();
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hasHoveredChoice) {
        flow_.Select(hovered);
        flow_.ActivateSelectedChoice();
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
        flow_.ActivateSelectedChoice();
    }
}

void RacePauseMenu::Draw() const {
    if (!open_) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.52f));
    const Rectangle panel{kPanelX, kPanelY, kPanelWidth, kPanelHeight};
    Neon::DrawOverlayPanel(panel, 0.97f);
    DrawRectangleLinesEx(panel, 2.0f, Neon::Cyan);
    Neon::DrawCenteredText("PAUSED", GetScreenWidth(), 104, 34, Neon::Cyan);
    Neon::DrawCenteredText("Choose an action", GetScreenWidth(), 144, 15, Fade(RAYWHITE, 0.72f));

    for (int index = 0; index < 5; ++index) {
        const RacePauseChoice choice = kChoices[index];
        const Rectangle bounds = ChoiceBounds(index);
        const bool selected = flow_.SelectedChoice() == choice;
        const bool confirming = flow_.IsConfirming() && flow_.ConfirmationChoice() == choice;
        const Color accent = confirming ? Neon::Orange : ChoiceColor(choice);
        const Neon::ButtonState state = Neon::GetButtonState(bounds, true, selected);
        Neon::DrawButton(bounds, accent, state, choice == RacePauseChoice::Resume);
        const char* title = ChoiceTitle(choice, returnsToMainMenu_);
        const int textWidth = MeasureText(title, 18);
        DrawText(title, static_cast<int>(bounds.x + bounds.width * 0.5f) - textWidth / 2,
                 static_cast<int>(bounds.y) + 17, 18,
                 Neon::IsButtonHighlighted(state) ? RAYWHITE : accent);
    }

    if (flow_.IsConfirming()) {
        const char* prompt = ConfirmationPrompt(flow_.ConfirmationChoice(), returnsToMainMenu_);
        Neon::DrawCenteredText(prompt, GetScreenWidth(), 520, 14, Neon::Orange);
        Neon::DrawCenteredText("Back / B cancels", GetScreenWidth(), 544, 13, Fade(RAYWHITE, 0.62f));
    } else {
        Neon::DrawCenteredText("Up/Down or D-pad: select   Enter/A: confirm", GetScreenWidth(), 520, 14,
                               Neon::Yellow);
        Neon::DrawCenteredText("P / Esc / Start / B: resume", GetScreenWidth(), 544, 13,
                               Fade(RAYWHITE, 0.68f));
    }

    const int selectedIndex = ChoiceIndex(flow_.SelectedChoice());
    const Rectangle selectedBounds = ChoiceBounds(selectedIndex);
    DrawRectangleLinesEx(Rectangle{selectedBounds.x - 4.0f, selectedBounds.y - 4.0f,
                                   selectedBounds.width + 8.0f, selectedBounds.height + 8.0f},
                         1.0f, Fade(Neon::Yellow, 0.42f));
}

RacePauseAction RacePauseMenu::ConsumeAction() { return flow_.ConsumeAction(); }
