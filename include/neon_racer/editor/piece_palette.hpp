#pragma once

#include <raylib.h>

#include "neon_racer/editor/piece_catalog.hpp"

// Raylib-facing presentation for the editor component catalogue. The palette
// owns hover animation and pointer hit testing, but leaves preview mutation to
// TrackEditor so placement and edit semantics stay centralized.
class PiecePalette {
public:
    PiecePalette();

    void Update(Vector2 pointer, int screenWidth, int screenHeight, float frameTime, bool enabled);
    bool ConsumesPointer(Vector2 pointer) const;
    bool ConsumeSelection(Vector2 pointer, TrackPieceType& selectedType) const;
    void Draw(TrackPieceType selectedType) const;

private:
    struct Layout {
        Rectangle visibleBounds;
        Rectangle fullBounds;
        Rectangle handleBounds;
        Rectangle itemBounds[6];
        std::size_t itemCount;
    };

    Layout CurrentLayout() const;

    float openAmount_;
    int screenWidth_;
    int screenHeight_;
    bool enabled_;
};
