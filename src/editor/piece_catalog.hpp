#pragma once

#include <cstddef>

#include "../track/track.hpp"

// The editor's stable, user-facing catalogue of placeable components.  It is
// deliberately Raylib-free so keyboard shortcuts, visual palettes, and tests
// all use the same source of truth without coupling to presentation code.
namespace EditorPieceCatalog {

struct Item {
    TrackPieceType type;
    const char* name;
    const char* description;
    int shortcut;
};

// Returns the display order used by the visual palette and the 1–6 shortcuts.
const Item* Items(std::size_t& count);

const Item* Find(TrackPieceType type);
const Item* FindShortcut(int shortcut);

// Supplies an isolated, valid example for a palette thumbnail.  It is never
// copied into the editable preview, whose dimensions and material should stay
// under the player's control.
TrackPiece Thumbnail(TrackPieceType type);

} // namespace EditorPieceCatalog
