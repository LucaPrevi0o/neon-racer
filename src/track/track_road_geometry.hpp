#pragma once

#include <vector>

#include "track.hpp"

// A Raylib-free direction used to derive a road's horizontal/three-dimensional
// side axis from a sampled tangent and normal.
struct TrackRoadAxis {
    float x;
    float y;
    float z;
};

// Shared physical-road representation for every consumer of track geometry.
// Branches and merges are layout components, but each is made of two ordinary
// straight road ribbons when it is driven, picked, checked for overlap, or
// drawn. Keeping that expansion here prevents those views from drifting apart.
namespace TrackRoadGeometry {

// Returns the physical road arms represented by `piece`. Ordinary pieces have
// one arm; branches and merges return two straight copies in connector order.
std::vector<TrackPiece> PhysicalRoadArms(const TrackPiece& piece);

// Samples all physical arms in the same stable order as PhysicalRoadArms().
// This is useful to systems such as surface contact that operate on samples
// rather than on individual road ribbons.
std::vector<TrackSurfaceSample> PhysicalRoadSurfaceSamples(const TrackPiece& piece);

// Returns the unnormalised tangent x normal axis. Keep this form when a caller
// needs the exact sampled frame; use the normalized variant for distances.
TrackRoadAxis SurfaceRightAxis(const TrackSurfaceSample& sample);

// Produces a unit right axis. Returns false when the supplied surface frame is
// degenerate, leaving `axis` as the zero vector.
bool NormalizedSurfaceRightAxis(const TrackSurfaceSample& sample, TrackRoadAxis& axis);

} // namespace TrackRoadGeometry
