#pragma once

#include <raylib.h>

#include "../track/track.hpp"

// Renders the physical road surface from a piece's continuous centre path.
// Layout and validation remain in track.*, independent of Raylib rendering.
void DrawTrackPieceSurface(const TrackPiece& piece, Color surfaceColor, Color edgeColor);
