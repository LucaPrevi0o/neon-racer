#pragma once

#include "track.hpp"

#include <cstdint>
#include <vector>

// Directed, race-facing view of the authored connector graph. Both gameplay
// route tracking and editor visualization consume this snapshot so direction,
// branch arms, and portal geometry cannot drift between the two systems.
struct TrackProgressPortal {
    TrackConnector connector;
    Heading crossingHeading;
};

struct TrackProgressTransition {
    std::uint32_t fromPieceId;
    std::uint32_t toPieceId;
    TrackProgressPortal portal;
};

struct TrackProgressGraph {
    RaceDirection direction;
    std::vector<TrackProgressTransition> transitions;
    bool hasStartFinish;
    std::uint32_t startFinishPieceId;
    TrackProgressPortal finishPortal;
};

TrackProgressGraph BuildTrackProgressGraph(const Track& track);
