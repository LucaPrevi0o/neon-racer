#pragma once

#include <raylib.h>

#include <cstdint>
#include <vector>

#include "track/track.hpp"

// Pure editor-selection helpers. Keeping them ray-based lets TrackEditor own
// input polling while callers with another viewport can reuse the same logic.
namespace EditorPicking {

bool GridPositionFromRay(const Ray& ray, GridPosition& position);
std::uint32_t PickPieceFromRay(const Ray& ray, const std::vector<TrackPiece>& pieces);

} // namespace EditorPicking
