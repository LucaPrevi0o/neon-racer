#include "neon_racer/editor/editor.hpp"

#include "ui/neon.hpp"

namespace {

Rectangle TrackLibraryBounds() {
    return Rectangle{820.0f, 80.0f, 420.0f, 490.0f};
}

Rectangle SaveDraftButtonBounds() {
    return Rectangle{840.0f, 132.0f, 180.0f, 34.0f};
}

Rectangle RefreshDraftButtonBounds() {
    return Rectangle{1035.0f, 132.0f, 180.0f, 34.0f};
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

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return true;
    const Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, SaveDraftButtonBounds())) {
        BeginSaveDraft();
        return true;
    }
    if (CheckCollisionPointRec(mouse, RefreshDraftButtonBounds())) {
        RefreshDraftList();
        return true;
    }
    for (std::size_t index = 0; index < savedDrafts_.size() && index < 12; ++index) {
        const Rectangle row{840.0f, 192.0f + static_cast<float>(index) * 27.0f, 375.0f, 23.0f};
        if (CheckCollisionPointRec(mouse, row)) {
            LoadDraft(savedDrafts_[index]);
            return true;
        }
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
    const float rowStart = currentDraftName_.empty() ? 192.0f : 222.0f;
    for (std::size_t index = 0; index < savedDrafts_.size() && index < 11; ++index) {
        const Rectangle row{840.0f, rowStart + static_cast<float>(index) * 27.0f, 375.0f, 23.0f};
        const Neon::ButtonState rowState = Neon::GetButtonState(row);
        Neon::DrawButton(row, Neon::Cyan, rowState);
        DrawText(savedDrafts_[index].c_str(), static_cast<int>(row.x) + 10,
                 static_cast<int>(row.y) + 4, 15,
                 Neon::IsButtonHighlighted(rowState) ? RAYWHITE : Neon::Cyan);
    }
    DrawText(currentDraftName_.empty()
                 ? "Ctrl+S: name and save  |  Ctrl+O/F5: library"
                 : "Ctrl+S: update current draft  |  Ctrl+O/F5: library",
             840, 540, 14, Neon::Yellow);
}
