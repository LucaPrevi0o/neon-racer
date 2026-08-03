#include "race_results_menu.hpp"

#include "../race/time_trial.hpp"
#include "../ui/neon.hpp"

#include <raylib.h>

namespace {

const RaceResultsChoice kChoices[] = {
    RaceResultsChoice::RaceAgain,
    RaceResultsChoice::SavePlayable,
    RaceResultsChoice::Return,
    RaceResultsChoice::Quit,
};

const float kPanelX = 400.0f;
const float kPanelY = 64.0f;
const float kPanelWidth = 480.0f;
const float kPanelHeight = 590.0f;
const float kButtonX = 454.0f;
const float kButtonY = 326.0f;
const float kButtonWidth = 372.0f;
const float kButtonHeight = 54.0f;
const float kButtonGap = 12.0f;

Rectangle ChoiceBounds(int index) {
    return Rectangle{kButtonX, kButtonY + static_cast<float>(index) * (kButtonHeight + kButtonGap),
                     kButtonWidth, kButtonHeight};
}

bool ChoiceAt(Vector2 point, RaceResultsChoice& choice) {
    for (int index = 0; index < 4; ++index) {
        if (CheckCollisionPointRec(point, ChoiceBounds(index))) {
            choice = kChoices[index];
            return true;
        }
    }
    return false;
}

const char* ChoiceTitle(RaceResultsChoice choice, bool returnsToMainMenu, bool updatesExistingGhost) {
    switch (choice) {
    case RaceResultsChoice::RaceAgain: return "RACE AGAIN";
    case RaceResultsChoice::SavePlayable:
        return updatesExistingGhost ? "UPDATE SAVED GHOST" : "SAVE PLAYABLE TRACK";
    case RaceResultsChoice::Return: return returnsToMainMenu ? "RETURN TO MAIN MENU" : "RETURN TO EDITOR";
    case RaceResultsChoice::Quit: return "QUIT NEON RACER";
    }
    return "RACE AGAIN";
}

Color ChoiceColor(RaceResultsChoice choice) {
    switch (choice) {
    case RaceResultsChoice::RaceAgain: return Neon::Green;
    case RaceResultsChoice::SavePlayable: return Neon::Cyan;
    case RaceResultsChoice::Return: return Neon::Pink;
    case RaceResultsChoice::Quit: return Neon::Orange;
    }
    return Neon::Cyan;
}

const char* ConfirmationPrompt(RaceResultsChoice choice, bool returnsToMainMenu) {
    if (choice == RaceResultsChoice::Return) {
        return returnsToMainMenu
            ? "Press confirm again to return to the main menu."
            : "Press confirm again to return to the editor.";
    }
    if (choice == RaceResultsChoice::Quit) return "Press confirm again to quit Neon Racer.";
    return "";
}

} // namespace

RaceResultsMenu::RaceResultsMenu()
    : flow_(), open_(false), returnsToMainMenu_(true), updatesExistingGhost_(false),
      totalTime_(0.0f), bestLapTime_(0.0f) {
}

void RaceResultsMenu::Open(bool returnsToMainMenu, bool canSaveReplay, bool updatesExistingGhost,
                           float totalTime, float bestLapTime) {
    returnsToMainMenu_ = returnsToMainMenu;
    updatesExistingGhost_ = updatesExistingGhost;
    totalTime_ = totalTime;
    bestLapTime_ = bestLapTime;
    flow_.Reset(canSaveReplay);
    open_ = true;
}

void RaceResultsMenu::Close() { open_ = false; }
bool RaceResultsMenu::IsOpen() const { return open_; }

void RaceResultsMenu::Update() {
    if (!open_) return;

    RaceResultsChoice hovered = flow_.SelectedChoice();
    const bool hasHoveredChoice = ChoiceAt(GetMousePosition(), hovered) && flow_.IsChoiceEnabled(hovered);
    if (hasHoveredChoice) flow_.Select(hovered);

    const bool previousPressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP);
    const bool nextPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    if (previousPressed) flow_.SelectPrevious();
    if (nextPressed) flow_.SelectNext();

    if (IsKeyPressed(KEY_ESCAPE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
        flow_.Back();
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

void RaceResultsMenu::Draw() const {
    if (!open_) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.60f));
    const Rectangle panel{kPanelX, kPanelY, kPanelWidth, kPanelHeight};
    Neon::DrawOverlayPanel(panel, 0.98f);
    DrawRectangleLinesEx(panel, 2.0f, Neon::Green);

    Neon::DrawCenteredText(updatesExistingGhost_ ? "GHOST RACE COMPLETE" : "TIME TRIAL COMPLETE",
                           GetScreenWidth(), 90, 32, Neon::Green);
    Neon::DrawCenteredText(updatesExistingGhost_
                               ? "Beat the saved replay to replace it"
                               : "Three laps verified",
                           GetScreenWidth(), 132, 15, Fade(RAYWHITE, 0.72f));

    Neon::DrawOverlayPanel(Rectangle{454.0f, 174.0f, 372.0f, 116.0f}, 0.78f);
    DrawText("TOTAL", 484, 194, 17, Neon::Cyan);
    DrawText(FormatRaceTime(totalTime_), 638, 190, 25, RAYWHITE);
    DrawText("BEST LAP", 484, 244, 17, Neon::Pink);
    DrawText(bestLapTime_ > 0.0f ? FormatRaceTime(bestLapTime_) : "--:--.--", 638, 240, 25,
             Neon::Green);

    for (int index = 0; index < 4; ++index) {
        const RaceResultsChoice choice = kChoices[index];
        const Rectangle bounds = ChoiceBounds(index);
        const bool enabled = flow_.IsChoiceEnabled(choice);
        const bool selected = flow_.SelectedChoice() == choice;
        const bool confirming = flow_.IsConfirming() && flow_.ConfirmationChoice() == choice;
        const Color accent = confirming ? Neon::Orange : ChoiceColor(choice);
        const Neon::ButtonState state = Neon::GetButtonState(bounds, enabled, selected);
        Neon::DrawButton(bounds, accent, state, choice == RaceResultsChoice::RaceAgain);
        const char* title = ChoiceTitle(choice, returnsToMainMenu_, updatesExistingGhost_);
        const int textWidth = MeasureText(title, 18);
        const Color textColor = enabled
            ? (Neon::IsButtonHighlighted(state) ? RAYWHITE : accent)
            : Fade(RAYWHITE, 0.34f);
        DrawText(title, static_cast<int>(bounds.x + bounds.width * 0.5f) - textWidth / 2,
                 static_cast<int>(bounds.y) + 18, 18, textColor);
    }

    if (!flow_.CanSavePlayable()) {
        Neon::DrawCenteredText(updatesExistingGhost_
                                   ? "The saved ghost remains faster; no file will be changed."
                                   : "Playable export is unavailable for this result.",
                               GetScreenWidth(), 602, 13, Fade(RAYWHITE, 0.52f));
    }

    if (flow_.IsConfirming()) {
        Neon::DrawCenteredText(ConfirmationPrompt(flow_.ConfirmationChoice(), returnsToMainMenu_),
                               GetScreenWidth(), 624, 14, Neon::Orange);
    } else {
        Neon::DrawCenteredText("Up/Down or D-pad: select   Enter/A: confirm   Esc/B: back",
                               GetScreenWidth(), 624, 13, Fade(RAYWHITE, 0.66f));
    }
}

RaceResultsAction RaceResultsMenu::ConsumeAction() { return flow_.ConsumeAction(); }
