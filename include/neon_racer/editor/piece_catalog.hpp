#pragma once

#include <cstddef>

#include "track/track.hpp"

// The editor's stable, user-facing catalogue of placeable components. It is
// deliberately Raylib-free so shortcuts, palettes, and tests use one source of
// truth without coupling to presentation code.
namespace EditorPieceCatalog {

struct Item {
    TrackPieceType type;
    const char* name;
    const char* description;
    int shortcut;
};

const Item* Items(std::size_t& count);
const Item* Find(TrackPieceType type);
const Item* FindShortcut(int shortcut);
TrackPiece Thumbnail(TrackPieceType type);

} // namespace EditorPieceCatalog
