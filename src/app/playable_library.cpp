#include "playable_library.hpp"

#include "../persistence/playable_track_io.hpp"
#include "../ui/neon.hpp"

#include <raylib.h>

namespace {

Rectangle PanelBounds() { return Rectangle{302.0f, 76.0f, 676.0f, 568.0f}; }
Rectangle CloseButtonBounds() { return Rectangle{920.0f, 94.0f, 38.0f, 28.0f}; }
Rectangle RefreshButtonBounds() { return Rectangle{752.0f, 132.0f, 188.0f, 32.0f}; }
Rectangle SaveButtonBounds() { return Rectangle{542.0f, 540.0f, 190.0f, 34.0f}; }
Rectangle CancelButtonBounds() { return Rectangle{746.0f, 540.0f, 190.0f, 34.0f}; }
Rectangle PreviousPageButtonBounds() { return Rectangle{342.0f, 522.0f, 150.0f, 30.0f}; }
Rectangle NextPageButtonBounds() { return Rectangle{788.0f, 522.0f, 150.0f, 30.0f}; }

Rectangle FieldBounds(int field) {
    return Rectangle{390.0f, 208.0f + static_cast<float>(field) * 82.0f, 500.0f, 34.0f};
}

Rectangle PlayableRowBounds(std::size_t index) {
    return Rectangle{342.0f, 194.0f + static_cast<float>(index) * 31.0f, 596.0f, 26.0f};
}

const char* FieldLabel(int field) {
    return field == 0 ? "TRACK NAME" : field == 1 ? "CREATOR" : "DESCRIPTION";
}

int FieldLimit(int field) { return field == 2 ? 160 : 48; }

bool GamepadBackPressed() {
    return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
}

bool GamepadConfirmPressed() {
    return IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

} // namespace

PlayableLibrary::PlayableLibrary()
    : mode_(Mode::Closed), activeField_(0), libraryFlow_(), exportRequested_(false) {
}

bool PlayableLibrary::IsOpen() const { return mode_ != Mode::Closed; }

void PlayableLibrary::Open() {
    mode_ = Mode::Library;
    pendingLaunchPath_.clear();
    exportRequested_ = false;
    Refresh();
}

void PlayableLibrary::Close() {
    mode_ = Mode::Closed;
    pendingLaunchPath_.clear();
    exportRequested_ = false;
}

void PlayableLibrary::BeginExport(const TrackMetadata& defaults) {
    mode_ = Mode::Export;
    exportMetadata_ = defaults;
    if (exportMetadata_.name.empty()) exportMetadata_.name = "Untitled time trial";
    if (exportMetadata_.creator.empty()) exportMetadata_.creator = "Player";
    if (exportMetadata_.description.empty()) exportMetadata_.description = "A verified Neon Racer time trial.";
    exportMetadata_.playableExportVersion = 0;
    activeField_ = 0;
    pendingLaunchPath_.clear();
    exportRequested_ = false;
    message_ = "Complete the metadata, then save the verified playable package.";
}

void PlayableLibrary::Update() {
    if (mode_ == Mode::Closed) return;

    if (mode_ == Mode::Library) {
        if (IsKeyPressed(KEY_ESCAPE) || GamepadBackPressed()) {
            Close();
            return;
        }

        if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_F5) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) {
            Refresh();
            return;
        }

        const bool previousPressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP);
        const bool nextPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        const bool previousPagePressed = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_PAGE_UP) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        const bool nextPagePressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_PAGE_DOWN) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        const bool navigationPressed = previousPressed || nextPressed || previousPagePressed || nextPagePressed;

        if (previousPressed) libraryFlow_.SelectPrevious();
        if (nextPressed) libraryFlow_.SelectNext();
        if (previousPagePressed) libraryFlow_.PreviousPage();
        if (nextPagePressed) libraryFlow_.NextPage();

        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || GamepadConfirmPressed()) &&
            libraryFlow_.HasSelection()) {
            pendingLaunchPath_ = playableFiles_[libraryFlow_.SelectedIndex()].path;
            return;
        }

        const Vector2 mouse = GetMousePosition();
        bool hasHoveredRow = false;
        std::size_t hoveredRow = 0u;
        for (std::size_t index = 0; index < libraryFlow_.VisibleCount(); ++index) {
            if (CheckCollisionPointRec(mouse, PlayableRowBounds(index))) {
                hasHoveredRow = true;
                hoveredRow = index;
                break;
            }
        }
        if (hasHoveredRow && !navigationPressed) libraryFlow_.SelectVisible(hoveredRow);

        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
        if (CheckCollisionPointRec(mouse, CloseButtonBounds())) {
            Close();
            return;
        }
        if (CheckCollisionPointRec(mouse, RefreshButtonBounds())) {
            Refresh();
            return;
        }
        if (CheckCollisionPointRec(mouse, PreviousPageButtonBounds())) {
            libraryFlow_.PreviousPage();
            return;
        }
        if (CheckCollisionPointRec(mouse, NextPageButtonBounds())) {
            libraryFlow_.NextPage();
            return;
        }
        if (hasHoveredRow) {
            libraryFlow_.SelectVisible(hoveredRow);
            pendingLaunchPath_ = playableFiles_[libraryFlow_.SelectedIndex()].path;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || GamepadBackPressed()) {
        Close();
        return;
    }

    int character = GetCharPressed();
    while (character > 0) {
        AppendCharacter(character);
        character = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) RemoveCharacter();
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_DOWN) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
        activeField_ = (activeField_ + 1) % 3;
    }
    if (IsKeyPressed(KEY_UP) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
        activeField_ = (activeField_ + 2) % 3;
    }
    if (IsKeyPressed(KEY_ENTER) || GamepadConfirmPressed()) {
        exportRequested_ = true;
        return;
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    const Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, CloseButtonBounds()) || CheckCollisionPointRec(mouse, CancelButtonBounds())) {
        Close();
        return;
    }
    if (CheckCollisionPointRec(mouse, SaveButtonBounds())) {
        exportRequested_ = true;
        return;
    }
    for (int field = 0; field < 3; ++field) {
        if (CheckCollisionPointRec(mouse, FieldBounds(field))) {
            activeField_ = field;
            return;
        }
    }
}

void PlayableLibrary::Draw() const {
    if (mode_ == Mode::Closed) return;

    const Rectangle panel = PanelBounds();
    Neon::DrawOverlayPanel(panel, 0.95f);
    DrawRectangleLinesEx(panel, 2.0f, Neon::Cyan);
    const Rectangle close = CloseButtonBounds();
    const Neon::ButtonState closeState = Neon::GetButtonState(close);
    Neon::DrawButton(close, Neon::Pink, closeState);
    DrawText("X", static_cast<int>(close.x) + 13, static_cast<int>(close.y) + 5, 18,
             Neon::IsButtonHighlighted(closeState) ? RAYWHITE : Neon::Pink);

    if (mode_ == Mode::Library) {
        DrawText("PLAYABLE TIME TRIALS", 332, 98, 24, Neon::Cyan);
        const Rectangle refresh = RefreshButtonBounds();
        const Neon::ButtonState refreshState = Neon::GetButtonState(refresh);
        Neon::DrawButton(refresh, Neon::Cyan, refreshState);
        DrawText("REFRESH LIBRARY", static_cast<int>(refresh.x) + 40, static_cast<int>(refresh.y) + 8, 15,
                 Neon::IsButtonHighlighted(refreshState) ? RAYWHITE : Neon::Cyan);
        DrawText("Choose a verified time trial. Ghost included.", 342, 172, 15, RAYWHITE);
        if (playableFiles_.empty()) {
            DrawText("No saved playable time trials yet.", 342, 210, 17, Fade(RAYWHITE, 0.65f));
        }

        const std::size_t firstVisibleIndex = libraryFlow_.FirstVisibleIndex();
        for (std::size_t index = 0; index < libraryFlow_.VisibleCount(); ++index) {
            const std::size_t absoluteIndex = firstVisibleIndex + index;
            const Rectangle row = PlayableRowBounds(index);
            const bool selected = libraryFlow_.HasSelection() && libraryFlow_.SelectedIndex() == absoluteIndex;
            const Neon::ButtonState rowState = Neon::GetButtonState(row, true, selected);
            const bool rowHighlighted = Neon::IsButtonHighlighted(rowState);
            Neon::DrawButton(row, Neon::Green, rowState);
            DrawText(playableFiles_[absoluteIndex].displayName.c_str(), static_cast<int>(row.x) + 12,
                     static_cast<int>(row.y) + 5, 16, rowHighlighted ? RAYWHITE : Neon::Green);
            DrawText("RACE", static_cast<int>(row.x + row.width) - 56, static_cast<int>(row.y) + 5, 15,
                     rowHighlighted ? Neon::Yellow : Fade(Neon::Yellow, 0.82f));
        }

        const std::size_t pageNumber = playableFiles_.empty()
            ? 0u : libraryFlow_.FirstVisibleIndex() / PlayableLibraryFlow::kPageSize + 1u;
        const std::size_t pageCount =
            (playableFiles_.size() + PlayableLibraryFlow::kPageSize - 1u) / PlayableLibraryFlow::kPageSize;
        const Rectangle previous = PreviousPageButtonBounds();
        const Rectangle next = NextPageButtonBounds();
        const bool hasPreviousPage = libraryFlow_.HasPreviousPage();
        const bool hasNextPage = libraryFlow_.HasNextPage();
        const Neon::ButtonState previousState = Neon::GetButtonState(previous, hasPreviousPage);
        const Neon::ButtonState nextState = Neon::GetButtonState(next, hasNextPage);
        Neon::DrawButton(previous, Neon::Cyan, previousState);
        DrawText("< PREVIOUS", static_cast<int>(previous.x) + 34, static_cast<int>(previous.y) + 8, 14,
                 hasPreviousPage ? (Neon::IsButtonHighlighted(previousState) ? RAYWHITE : Neon::Cyan) : Fade(RAYWHITE, 0.32f));
        Neon::DrawButton(next, Neon::Cyan, nextState);
        DrawText("NEXT >", static_cast<int>(next.x) + 51, static_cast<int>(next.y) + 8, 14,
                 hasNextPage ? (Neon::IsButtonHighlighted(nextState) ? RAYWHITE : Neon::Cyan) : Fade(RAYWHITE, 0.32f));
        DrawText(TextFormat("PAGE %i / %i", static_cast<int>(pageNumber), static_cast<int>(pageCount)), 602, 531, 14,
                 Neon::Yellow);
        DrawText(message_.c_str(), 342, 578, 14, Neon::Yellow);
        DrawText("Up/Down: select  Left/Right: page  Enter/A: race  R/X: refresh  Esc/B: close",
                 342, 606, 12, Fade(RAYWHITE, 0.66f));
        return;
    }

    DrawText("SAVE PLAYABLE TIME TRIAL", 332, 98, 24, Neon::Cyan);
    DrawText("Verified layout + best three-lap ghost.", 342, 142, 15, RAYWHITE);
    for (int field = 0; field < 3; ++field) {
        const Rectangle bounds = FieldBounds(field);
        const bool active = field == activeField_;
        const std::string& value = field == 0 ? exportMetadata_.name :
                                   field == 1 ? exportMetadata_.creator : exportMetadata_.description;
        DrawText(FieldLabel(field), 390, static_cast<int>(bounds.y) - 23, 14, active ? Neon::Yellow : RAYWHITE);
        DrawRectangleRec(bounds, Fade(BLACK, 0.66f));
        DrawRectangleLinesEx(bounds, active ? 2.0f : 1.0f, active ? Neon::Yellow : Fade(Neon::Cyan, 0.62f));
        DrawText(value.c_str(), 400, static_cast<int>(bounds.y) + 8, 16,
                 active ? Neon::Yellow : Fade(RAYWHITE, 0.85f));
    }
    const Rectangle save = SaveButtonBounds();
    const Rectangle cancel = CancelButtonBounds();
    const Neon::ButtonState saveState = Neon::GetButtonState(save);
    const Neon::ButtonState cancelState = Neon::GetButtonState(cancel);
    Neon::DrawButton(save, Neon::Green, saveState, true);
    DrawText("SAVE PLAYABLE", static_cast<int>(save.x) + 38, static_cast<int>(save.y) + 9, 15, BLACK);
    Neon::DrawButton(cancel, Neon::Pink, cancelState);
    DrawText("CANCEL", static_cast<int>(cancel.x) + 65, static_cast<int>(cancel.y) + 9, 15,
             Neon::IsButtonHighlighted(cancelState) ? RAYWHITE : Neon::Pink);
    DrawText("Tab/Up/Down: field  |  Enter/A: save  |  Esc/B: cancel", 390, 496, 14, Neon::Yellow);
    DrawText(message_.c_str(), 390, 600, 14, Neon::Yellow);
}

bool PlayableLibrary::ConsumeLaunchRequest(std::string& path) {
    if (pendingLaunchPath_.empty()) return false;
    path = pendingLaunchPath_;
    pendingLaunchPath_.clear();
    return true;
}

bool PlayableLibrary::ConsumeExportRequest(TrackMetadata& metadata) {
    if (!exportRequested_) return false;
    exportRequested_ = false;
    metadata = exportMetadata_;
    return true;
}

void PlayableLibrary::ReportMessage(const std::string& message) { message_ = message; }

void PlayableLibrary::FinishExport(const std::string& message) {
    mode_ = Mode::Library;
    if (Refresh()) message_ = message;
}

bool PlayableLibrary::Refresh() {
    std::string error;
    if (!PlayableTrackIO::ListCustomPlayableTracks(playableFiles_, error)) {
        libraryFlow_.Reset(0u);
        message_ = error;
        return false;
    }
    libraryFlow_.Reset(playableFiles_.size());
    if (playableFiles_.empty()) {
        message_ = "No saved playable time trials yet.";
    } else {
        message_ = "Select a package and confirm to start its verified time trial.";
    }
    return true;
}

void PlayableLibrary::AppendCharacter(int character) {
    if (character < 32 || character > 126) return;
    std::string& field = ActiveField();
    if (static_cast<int>(field.size()) >= FieldLimit(activeField_)) return;
    field += static_cast<char>(character);
}

void PlayableLibrary::RemoveCharacter() {
    std::string& field = ActiveField();
    if (!field.empty()) field.erase(field.size() - 1);
}

std::string& PlayableLibrary::ActiveField() {
    return activeField_ == 0 ? exportMetadata_.name : activeField_ == 1 ? exportMetadata_.creator : exportMetadata_.description;
}

const std::string& PlayableLibrary::ActiveField() const {
    return activeField_ == 0 ? exportMetadata_.name : activeField_ == 1 ? exportMetadata_.creator : exportMetadata_.description;
}
