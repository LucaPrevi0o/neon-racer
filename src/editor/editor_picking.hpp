#pragma once

#include <raylib.h>

#include <cstdint>
#include <vector>

#include "../track/track.hpp"

// Pure editor-selection helpers. Keeping them ray-based lets TrackEditor own
// input polling while callers with another viewport can reuse the same logic.
namespace EditorPicking {

// Projects a ray onto the editor's y = 0 grid and rounds to its nearest cell.
// Returns false when the ray is parallel to, or points away from, the grid.
bool GridPositionFromRay(const Ray& ray, GridPosition& position);

// Returns the id of the nearest piece whose rendered road surface intersects
// `ray`, or zero when the ray does not hit a piece.
std::uint32_t PickPieceFromRay(const Ray& ray, const std::vector<TrackPiece>& pieces);

} // namespace EditorPicking
