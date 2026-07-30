#include "playable_library.hpp"

#include "../persistence/playable_track_io.hpp"
#include "../ui/neon.hpp"

#include <raylib.h>

#include <algorithm>

namespace {

Rectangle PanelBounds() { return Rectangle{302.0f, 76.0f, 676.0f, 568.0f}; }
Rectangle CloseButtonBounds() { return Rectangle{920.0f, 94.0f, 38.0f, 28.0f}; }
Rectangle RefreshButtonBounds() { return Rectangle{752.0f, 132.0f, 188.0f, 32.0f}; }
Rectangle SaveButtonBounds() { return Rectangle{542.0f, 540.0f, 190.0f, 34.0f}; }
Rectangle CancelButtonBounds() { return Rectangle{746.0f, 540.0f, 190.0f, 34.0f}; }
Rectangle PreviousPageButtonBounds() { return Rectangle{342.0f, 522.0f, 150.0f, 30.0f}; }
Rectangle NextPageButtonBounds() { return Rectangle{788.0f, 522.0f, 150.0f, 30.0f}; }

const std::size_t kPlayablePageSize = 10u;

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

} // namespace

PlayableLibrary::PlayableLibrary()
    : mode_(Mode::Closed), activeField_(0), firstVisibleIndex_(0), exportRequested_(false) {
}

bool PlayableLibrary::IsOpen() const { return mode_ != Mode::Closed; }

void PlayableLibrary::Open() {
    mode_ = Mode::Library;
    pendingLaunchPath_.clear();
    firstVisibleIndex_ = 0;
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
        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
        const Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, CloseButtonBounds())) {
            Close();
            return;
        }
        if (CheckCollisionPointRec(mouse, RefreshButtonBounds())) {
            Refresh();
            return;
        }
        if (CheckCollisionPointRec(mouse, PreviousPageButtonBounds()) && firstVisibleIndex_ >= kPlayablePageSize) {
            firstVisibleIndex_ -= kPlayablePageSize;
            return;
        }
        if (CheckCollisionPointRec(mouse, NextPageButtonBounds()) &&
            firstVisibleIndex_ + kPlayablePageSize < playableFiles_.size()) {
            firstVisibleIndex_ += kPlayablePageSize;
            return;
        }
        const std::size_t visibleCount = firstVisibleIndex_ < playableFiles_.size()
            ? std::min(kPlayablePageSize, playableFiles_.size() - firstVisibleIndex_) : 0u;
        for (std::size_t index = 0; index < visibleCount; ++index) {
            if (CheckCollisionPointRec(mouse, PlayableRowBounds(index))) {
                pendingLaunchPath_ = playableFiles_[firstVisibleIndex_ + index].path;
                return;
            }
        }
        return;
    }

    int character = GetCharPressed();
    while (character > 0) {
        AppendCharacter(character);
        character = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) RemoveCharacter();
    if (IsKeyPressed(KEY_TAB)) activeField_ = (activeField_ + 1) % 3;
    if (IsKeyPressed(KEY_ENTER)) {
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
    DrawRectangleRec(close, Fade(Neon::Panel, 0.95f));
    DrawRectangleLinesEx(close, 1.5f, Neon::Pink);
    DrawText("X", static_cast<int>(close.x) + 13, static_cast<int>(close.y) + 5, 18, Neon::Pink);

    if (mode_ == Mode::Library) {
        DrawText("PLAYABLE TIME TRIALS", 332, 98, 24, Neon::Cyan);
        const Rectangle refresh = RefreshButtonBounds();
        DrawRectangleRec(refresh, Fade(Neon::Panel, 0.95f));
        DrawRectangleLinesEx(refresh, 1.5f, Neon::Cyan);
        DrawText("REFRESH LIBRARY", static_cast<int>(refresh.x) + 40, static_cast<int>(refresh.y) + 8, 15, Neon::Cyan);
        DrawText("Frozen packages keep a verified layout and ghost. Click one to race it.", 342, 172, 15, RAYWHITE);
        if (playableFiles_.empty()) {
            DrawText("No saved playable time trials yet.", 342, 210, 17, Fade(RAYWHITE, 0.65f));
        }
        const std::size_t visibleCount = firstVisibleIndex_ < playableFiles_.size()
            ? std::min(kPlayablePageSize, playableFiles_.size() - firstVisibleIndex_) : 0u;
        for (std::size_t index = 0; index < visibleCount; ++index) {
            const Rectangle row = PlayableRowBounds(index);
            DrawRectangleRec(row, Fade(Neon::Panel, 0.72f));
            DrawRectangleLinesEx(row, 1.0f, Fade(Neon::Green, 0.55f));
            DrawText(playableFiles_[firstVisibleIndex_ + index].displayName.c_str(), static_cast<int>(row.x) + 12,
                     static_cast<int>(row.y) + 5, 16, Neon::Green);
            DrawText("RACE", static_cast<int>(row.x + row.width) - 56, static_cast<int>(row.y) + 5, 15, Neon::Yellow);
        }
        const std::size_t pageNumber = playableFiles_.empty() ? 0u : firstVisibleIndex_ / kPlayablePageSize + 1u;
        const std::size_t pageCount = (playableFiles_.size() + kPlayablePageSize - 1u) / kPlayablePageSize;
        const Rectangle previous = PreviousPageButtonBounds();
        const Rectangle next = NextPageButtonBounds();
        DrawRectangleRec(previous, Fade(Neon::Panel, 0.95f));
        DrawRectangleLinesEx(previous, 1.0f, firstVisibleIndex_ >= kPlayablePageSize ? Neon::Cyan : Fade(RAYWHITE, 0.25f));
        DrawText("< PREVIOUS", static_cast<int>(previous.x) + 34, static_cast<int>(previous.y) + 8, 14,
                 firstVisibleIndex_ >= kPlayablePageSize ? Neon::Cyan : Fade(RAYWHITE, 0.32f));
        DrawRectangleRec(next, Fade(Neon::Panel, 0.95f));
        const bool hasNextPage = firstVisibleIndex_ + kPlayablePageSize < playableFiles_.size();
        DrawRectangleLinesEx(next, 1.0f, hasNextPage ? Neon::Cyan : Fade(RAYWHITE, 0.25f));
        DrawText("NEXT >", static_cast<int>(next.x) + 51, static_cast<int>(next.y) + 8, 14,
                 hasNextPage ? Neon::Cyan : Fade(RAYWHITE, 0.32f));
        DrawText(TextFormat("PAGE %i / %i", static_cast<int>(pageNumber), static_cast<int>(pageCount)), 602, 531, 14,
                 Neon::Yellow);
        DrawText(message_.c_str(), 342, 590, 14, Neon::Yellow);
        return;
    }

    DrawText("SAVE PLAYABLE TIME TRIAL", 332, 98, 24, Neon::Cyan);
    DrawText("This saves the frozen layout and its verified three-lap ghost.", 342, 142, 15, RAYWHITE);
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
    DrawRectangleRec(save, Fade(Neon::Green, 0.84f));
    DrawRectangleLinesEx(save, 2.0f, Neon::Green);
    DrawText("SAVE PLAYABLE", static_cast<int>(save.x) + 38, static_cast<int>(save.y) + 9, 15, BLACK);
    DrawRectangleRec(cancel, Fade(Neon::Panel, 0.95f));
    DrawRectangleLinesEx(cancel, 2.0f, Neon::Pink);
    DrawText("CANCEL", static_cast<int>(cancel.x) + 65, static_cast<int>(cancel.y) + 9, 15, Neon::Pink);
    DrawText("Click a field or press Tab. Enter saves; Backspace edits.", 390, 496, 14, Neon::Yellow);
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
        message_ = error;
        return false;
    }
    firstVisibleIndex_ = 0;
    if (playableFiles_.empty()) {
        message_ = "No saved playable time trials yet.";
    } else {
        message_ = "Click a package to start its verified time trial.";
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
