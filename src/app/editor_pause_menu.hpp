#pragma once

#include "editor_pause_flow.hpp"
#include "../ui/neon.hpp"

#include <raylib.h>

class EditorPauseMenu {
public:
    EditorPauseMenu() : flow_(), open_(false) {}

    void Open(bool canStartTrial, bool hasSavedDraft) {
        flow_.Reset(canStartTrial, hasSavedDraft);
        open_ = true;
    }

    void Close() { open_ = false; }
    bool IsOpen() const { return open_; }

    void Update() {
        if (!open_) return;

        if (flow_.IsConfirmingQuit()) {
            const Vector2 mouse = GetMousePosition();
            const bool okHovered = CheckCollisionPointRec(mouse, AlertOkBounds());
            const bool cancelHovered = CheckCollisionPointRec(mouse, AlertCancelBounds());
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (okHovered) {
                    flow_.ActivateSelectedChoice();
                    return;
                }
                if (cancelHovered) {
                    flow_.CancelQuitAlert();
                    return;
                }
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                flow_.ActivateSelectedChoice();
                return;
            }
            if (IsKeyPressed(KEY_ESCAPE) ||
                IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
                flow_.CancelQuitAlert();
            }
            return;
        }

        EditorPauseChoice hovered = flow_.SelectedChoice();
        const bool hasHoveredChoice = ChoiceAt(GetMousePosition(), hovered);
        if (hasHoveredChoice) flow_.Select(hovered);

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
            flow_.SelectPrevious();
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
            flow_.SelectNext();
        }

        if (IsKeyPressed(KEY_ESCAPE) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            flow_.CancelAlertOrResume();
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

    void Draw() const {
        if (!open_) return;

        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.52f));
        const Rectangle panel{430.0f, 86.0f, 420.0f, 548.0f};
        Neon::DrawOverlayPanel(panel, 0.97f);
        DrawRectangleLinesEx(panel, 2.0f, Neon::Cyan);
        Neon::DrawCenteredText("EDITOR PAUSED", GetScreenWidth(), 112, 32, Neon::Cyan);
        Neon::DrawCenteredText("Choose what to do with this track", GetScreenWidth(), 150, 15,
                               Fade(RAYWHITE, 0.72f));

        for (int index = 0; index < 4; ++index) {
            const EditorPauseChoice choice = ChoiceAtIndex(index);
            const Rectangle bounds = ChoiceBounds(index);
            const bool enabled = flow_.IsChoiceEnabled(choice);
            const bool selected = flow_.SelectedChoice() == choice;
            const Color accent = ChoiceColor(choice);
            const Neon::ButtonState state = Neon::GetButtonState(bounds, enabled, selected);
            Neon::DrawButton(bounds, accent, state, choice == EditorPauseChoice::StartTrial);
            const char* title = ChoiceTitle(choice, flow_.HasSavedDraft());
            const int width = MeasureText(title, 18);
            DrawText(title, static_cast<int>(bounds.x + bounds.width * 0.5f) - width / 2,
                     static_cast<int>(bounds.y) + 14, 18,
                     enabled ? (Neon::IsButtonHighlighted(state) ? RAYWHITE : accent)
                             : Fade(RAYWHITE, 0.32f));
            const char* detail = ChoiceDetail(choice, flow_.HasSavedDraft());
            const int detailWidth = MeasureText(detail, 12);
            DrawText(detail, static_cast<int>(bounds.x + bounds.width * 0.5f) - detailWidth / 2,
                     static_cast<int>(bounds.y) + 36, 12,
                     enabled ? Fade(RAYWHITE, 0.62f) : Fade(RAYWHITE, 0.26f));
        }

        Neon::DrawCenteredText("Up/Down or D-pad: select   Enter/A: confirm", GetScreenWidth(), 554,
                               14, Neon::Yellow);
        Neon::DrawCenteredText("Esc / Start / B: resume editing", GetScreenWidth(), 580, 13,
                               Fade(RAYWHITE, 0.68f));

        if (!flow_.CanStartTrial()) {
            Neon::DrawCenteredText("Start Trial unlocks when the track is race-ready.", GetScreenWidth(),
                                   610, 13, Neon::Orange);
        }

        if (flow_.IsConfirmingQuit()) DrawQuitAlert();
    }

    EditorPauseAction ConsumeAction() { return flow_.ConsumeAction(); }

private:
    static Rectangle ChoiceBounds(int index) {
        return Rectangle{474.0f, 190.0f + static_cast<float>(index) * 78.0f, 332.0f, 64.0f};
    }

    static Rectangle AlertOkBounds() { return Rectangle{492.0f, 438.0f, 132.0f, 42.0f}; }
    static Rectangle AlertCancelBounds() { return Rectangle{656.0f, 438.0f, 132.0f, 42.0f}; }

    static EditorPauseChoice ChoiceAtIndex(int index) {
        switch (index) {
        case 0: return EditorPauseChoice::StartTrial;
        case 1: return EditorPauseChoice::SaveDraft;
        case 2: return EditorPauseChoice::OpenDraft;
        case 3: return EditorPauseChoice::QuitToMenu;
        }
        return EditorPauseChoice::SaveDraft;
    }

    static bool ChoiceAt(Vector2 point, EditorPauseChoice& choice) {
        for (int index = 0; index < 4; ++index) {
            if (CheckCollisionPointRec(point, ChoiceBounds(index))) {
                choice = ChoiceAtIndex(index);
                return true;
            }
        }
        return false;
    }

    static const char* ChoiceTitle(EditorPauseChoice choice, bool hasSavedDraft) {
        switch (choice) {
        case EditorPauseChoice::StartTrial: return "START TRIAL";
        case EditorPauseChoice::SaveDraft: return hasSavedDraft ? "SAVE DRAFT" : "SAVE NEW DRAFT";
        case EditorPauseChoice::OpenDraft: return "OPEN DRAFT";
        case EditorPauseChoice::QuitToMenu: return "QUIT TO MENU";
        }
        return "SAVE DRAFT";
    }

    static const char* ChoiceDetail(EditorPauseChoice choice, bool hasSavedDraft) {
        switch (choice) {
        case EditorPauseChoice::StartTrial: return "Run the current track and verify a playable export.";
        case EditorPauseChoice::SaveDraft:
            return hasSavedDraft ? "Update the current draft in the latest format."
                                 : "Choose a name and create an editable draft.";
        case EditorPauseChoice::OpenDraft: return "Browse and load your saved editable tracks.";
        case EditorPauseChoice::QuitToMenu: return "Leave the editor and return to the Home menu.";
        }
        return "";
    }

    static Color ChoiceColor(EditorPauseChoice choice) {
        switch (choice) {
        case EditorPauseChoice::StartTrial: return Neon::Green;
        case EditorPauseChoice::SaveDraft: return Neon::Cyan;
        case EditorPauseChoice::OpenDraft: return Neon::Yellow;
        case EditorPauseChoice::QuitToMenu: return Neon::Pink;
        }
        return Neon::Cyan;
    }

    void DrawQuitAlert() const {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.42f));
        const Rectangle alert{448.0f, 286.0f, 384.0f, 222.0f};
        Neon::DrawOverlayPanel(alert, 0.99f);
        DrawRectangleLinesEx(alert, 2.0f, Neon::Orange);
        Neon::DrawCenteredText("UNSAVED TRACK", GetScreenWidth(), 314, 24, Neon::Orange);
        Neon::DrawCenteredText("This track has never been saved as a draft.", GetScreenWidth(), 354,
                               14, RAYWHITE);
        Neon::DrawCenteredText("Quit to the menu and discard this editor session?", GetScreenWidth(),
                               378, 13, Fade(RAYWHITE, 0.72f));

        const Rectangle ok = AlertOkBounds();
        const Rectangle cancel = AlertCancelBounds();
        const Neon::ButtonState okState = Neon::GetButtonState(ok);
        const Neon::ButtonState cancelState = Neon::GetButtonState(cancel);
        Neon::DrawButton(ok, Neon::Orange, okState);
        Neon::DrawButton(cancel, Neon::Cyan, cancelState, true);
        DrawText("OK", static_cast<int>(ok.x) + 54, static_cast<int>(ok.y) + 12, 17,
                 Neon::IsButtonHighlighted(okState) ? RAYWHITE : Neon::Orange);
        DrawText("CANCEL", static_cast<int>(cancel.x) + 33, static_cast<int>(cancel.y) + 12, 17,
                 Neon::IsButtonHighlighted(cancelState) ? RAYWHITE : Neon::Cyan);
        Neon::DrawCenteredText("Enter/A: OK   Esc/B: Cancel", GetScreenWidth(), 488, 12,
                               Fade(RAYWHITE, 0.62f));
    }

    EditorPauseFlow flow_;
    bool open_;
};
