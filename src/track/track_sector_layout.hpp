#pragma once

#include "track_progress_graph.hpp"

#include <cstdint>
#include <vector>

// Automatic timing sectors are derived only for a complete race topology. A
// typed status keeps unsupported graph shapes explicit instead of allowing the
// race layer to guess at boundaries that not every legal route will cross.
enum class TrackSectorLayoutStatus {
    Ready,
    MissingStartFinish,
    TrackNotRaceReady,
    NoClosedRoute,
    TooFewTransitions,
    UnsupportedCycle,
    TooManyRouteVariants,
};

struct TrackSectorGate {
    std::uint32_t transitionId;
    std::uint32_t fromPieceId;
    std::uint32_t toPieceId;
    TrackProgressPortal portal;
};

// One logical boundary may contain several exact connector gates when a track
// offers alternate branch routes through the same timing sector.
struct TrackSectorBoundary {
    std::vector<TrackSectorGate> alternativeGates;
};

struct TrackSectorRouteVariant {
    std::uint32_t id;
    std::vector<std::uint32_t> transitionIds;
    float length;
    std::uint32_t firstBoundaryTransitionId;
    std::uint32_t secondBoundaryTransitionId;
};

struct TrackSectorLayout {
    TrackSectorLayoutStatus status;
    // Exactly two intermediate boundaries when status is Ready. The finish
    // portal from TrackProgressGraph is the implicit end of sector three.
    std::vector<TrackSectorBoundary> boundaries;
    std::vector<TrackSectorRouteVariant> routeVariants;

    bool IsReady() const;
};

// Shared physical-length policy used by both automatic sector placement and
// race progress snapshots. Keeping one calculation prevents timing progress
// from drifting away from the boundaries selected by the track domain.
float TrackRouteLengthForPiece(const TrackPiece& piece);

TrackSectorLayout BuildTrackSectorLayout(const Track& track);
const char* TrackSectorLayoutStatusMessage(TrackSectorLayoutStatus status);
