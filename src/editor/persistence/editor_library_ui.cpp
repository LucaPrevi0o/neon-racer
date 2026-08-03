#include "neon_racer/editor/editor.hpp"

#include "ui/neon.hpp"

namespace {

const float kRowStart = 222.0f;
const float kRowStep = 27.0f;

Rectangle TrackLibraryBounds() {
    return Rectangle{820.0f, 80.0f, 420.0f, 490.0f};
}

Rectangle SaveDraftButtonBounds() {
    return Rectangle{840.0f, 132.0f, 180.0f, 34.0f};
}

Rectangle RefreshDraftButtonBounds() {
    return Rectangle{1035.0f, 132.0f, 180.0f, 34.0f};
}

Rectangle PreviousPageButtonBounds() {
    return Rectangle{840.0f, 474.0f, 105.0f, 32.0f};
}

Rectangle NextPageButtonBounds() {
    return Rectangle{1110.0f, 474.0f, 105.0f, 32.0f};
}

Rectangle DraftRowBounds(std::size_t visibleIndex) {
    return Rectangle{840.0f, kRowStart + static_cast<float>(visibleIndex) * kRowStep, 375.0f, 23.0f};
}

} // namespace

bool TrackEditor::UpdateTrackLibraryInput() {
    if (!libraryOpen_) return false;

    if (namingDraft_) {
        int character = GetCharPressed();
        while (character > 0) {
            if ((character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == ' ' || character == '_' || character == '-') {
                if (draftName_.size() < 32) draftName_ += static_cast<char>(character);
            }
            character = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !draftName_.empty()) {
            draftName_.erase(draftName_.size() - 1);
        }
        if (IsKeyPressed(KEY_ENTER)) SaveNamedDraft();
        return true;
    }

    const Vector2 mouse = GetMousePosition();
    const float wheel = CheckCollisionPointRec(mouse, TrackLibraryBounds()) ? GetMouseWheelMove() : 0.0f;
    const bool previousPage = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_PAGE_UP) || wheel > 0.0f ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    const bool nextPage = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_PAGE_DOWN) || wheel < 0.0f ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    if (previousPage) draftLibraryFlow_.PreviousPage();
    if (nextPage) draftLibraryFlow_.NextPage();

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return true;
    if (CheckCollisionPointRec(mouse, SaveDraftButtonBounds())) {
        BeginSaveDraft();
        return true;
    }
    if (CheckCollisionPointRec(mouse, RefreshDraftButtonBounds())) {
        RefreshDraftList();
        return true;
    }
    if (CheckCollisionPointRec(mouse, PreviousPageButtonBounds())) {
        draftLibraryFlow_.PreviousPage();
        return true;
    }
    if (CheckCollisionPointRec(mouse, NextPageButtonBounds())) {
        draftLibraryFlow_.NextPage();
        return true;
    }

    const std::size_t first = draftLibraryFlow_.FirstVisibleIndex();
    const std::size_t visibleCount = draftLibraryFlow_.VisibleCount();
    for (std::size_t visibleIndex = 0; visibleIndex < visibleCount; ++visibleIndex) {
        if (!CheckCollisionPointRec(mouse, DraftRowBounds(visibleIndex))) continue;
        const std::size_t itemIndex = first + visibleIndex;
        if (itemIndex < savedDrafts_.size()) LoadDraft(savedDrafts_[itemIndex]);
        return true;
    }
    return true;
}

void TrackEditor::DrawTrackLibrary() const {
    if (!libraryOpen_) return;

    const Rectangle bounds = TrackLibraryBounds();
    Neon::DrawOverlayPanel(bounds, 0.92f);
    DrawText("CUSTOM TRACK LIBRARY", static_cast<int>(bounds.x) + 20,
             static_cast<int>(bounds.y) + 18, 22, Neon::Cyan);

    const Rectangle save = SaveDraftButtonBounds();
    const Rectangle refresh = RefreshDraftButtonBounds();
    const bool actionsEnabled = !namingDraft_;
    const Neon::ButtonState saveState = Neon::GetButtonState(save, actionsEnabled);
    const Neon::ButtonState refreshState = Neon::GetButtonState(refresh, actionsEnabled);
    Neon::DrawButton(save, Neon::Pink, saveState, actionsEnabled);
    const char* saveLabel = currentDraftName_.empty() ? "SAVE AS" : "SAVE CURRENT";
    const int saveLabelWidth = MeasureText(saveLabel, 15);
    DrawText(saveLabel, static_cast<int>(save.x + save.width * 0.5f) - saveLabelWidth / 2,
             static_cast<int>(save.y) + 9, 15,
             actionsEnabled ? BLACK : Fade(RAYWHITE, 0.36f));
    Neon::DrawButton(refresh, Neon::Cyan, refreshState);
    DrawText("REFRESH", static_cast<int>(refresh.x) + 54, static_cast<int>(refresh.y) + 9, 15,
             actionsEnabled
                 ? (Neon::IsButtonHighlighted(refreshState) ? RAYWHITE : Neon::Cyan)
                 : Fade(RAYWHITE, 0.36f));

    if (namingDraft_) {
        DrawText("Name your draft, then press Enter:", 840, 188, 16, RAYWHITE);
        DrawRectangleRec(Rectangle{840.0f, 214.0f, 375.0f, 34.0f}, Fade(BLACK, 0.65f));
        DrawRectangleLinesEx(Rectangle{840.0f, 214.0f, 375.0f, 34.0f}, 2.0f, Neon::Yellow);
        DrawText(draftName_.c_str(), 850, 223, 18, Neon::Yellow);
        DrawText("Saved in your Neon Racer data folder.", 840, 265, 14,
                 Fade(RAYWHITE, 0.68f));
        return;
    }

    DrawText("CUSTOM DRAFTS  |  click one to load", 840, 175, 16, RAYWHITE);
    if (!currentDraftName_.empty()) {
        DrawText(("CURRENT: " + currentDraftName_).c_str(), 840, 202, 14, Neon::Yellow);
    } else if (savedDrafts_.empty()) {
        DrawText("No saved custom tracks yet.", 840, 205, 16, Fade(RAYWHITE, 0.65f));
    }

    const std::size_t first = draftLibraryFlow_.FirstVisibleIndex();
    const std::size_t visibleCount = draftLibraryFlow_.VisibleCount();
    for (std::size_t visibleIndex = 0; visibleIndex < visibleCount; ++visibleIndex) {
        const std::size_t itemIndex = first + visibleIndex;
        if (itemIndex >= savedDrafts_.size()) break;
        const Rectangle row = DraftRowBounds(visibleIndex);
        const Neon::ButtonState rowState = Neon::GetButtonState(row);
        Neon::DrawButton(row, Neon::Cyan, rowState);
        DrawText(savedDrafts_[itemIndex].c_str(), static_cast<int>(row.x) + 10,
                 static_cast<int>(row.y) + 4, 15,
                 Neon::IsButtonHighlighted(rowState) ? RAYWHITE : Neon::Cyan);
    }

    const Rectangle previous = PreviousPageButtonBounds();
    const Rectangle next = NextPageButtonBounds();
    const bool hasPrevious = draftLibraryFlow_.HasPreviousPage();
    const bool hasNext = draftLibraryFlow_.HasNextPage();
    const Neon::ButtonState previousState = Neon::GetButtonState(previous, hasPrevious);
    const Neon::ButtonState nextState = Neon::GetButtonState(next, hasNext);
    Neon::DrawButton(previous, Neon::Pink, previousState, hasPrevious);
    Neon::DrawButton(next, Neon::Cyan, nextState, hasNext);
    DrawText("PREVIOUS", 855, 483, 14,
             hasPrevious ? (Neon::IsButtonHighlighted(previousState) ? RAYWHITE : Neon::Pink)
                         : Fade(RAYWHITE, 0.32f));
    DrawText("NEXT", 1143, 483, 14,
             hasNext ? (Neon::IsButtonHighlighted(nextState) ? RAYWHITE : Neon::Cyan)
                     : Fade(RAYWHITE, 0.32f));

    const char* pageText = draftLibraryFlow_.PageCount() == 0
        ? "PAGE 0 / 0"
        : TextFormat("PAGE %i / %i", static_cast<int>(draftLibraryFlow_.CurrentPage()),
                     static_cast<int>(draftLibraryFlow_.PageCount()));
    DrawText(pageText, 1028 - MeasureText(pageText, 14) / 2, 483, 14, Neon::Yellow);

    DrawText(currentDraftName_.empty()
                 ? "Ctrl+S: name/save  |  Ctrl+O: close  |  arrows/PgUp/PgDn/wheel: page"
                 : "Ctrl+S: update  |  Ctrl+O: close  |  arrows/PgUp/PgDn/wheel: page",
             840, 540, 12, Neon::Yellow);
}
