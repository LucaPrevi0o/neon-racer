#pragma once

#include "track_position_tracker.hpp"

class Track;
struct TrackPiece;
struct TrackProgressPortal;
struct TrackProgressTransition;

namespace TrackPositionGeometry {

TrackPositionTracker::Gate GateFor(const TrackProgressPortal& portal);
bool CrossesGate(const TrackPositionTracker::Gate& gate, RaceVector3 previousPosition,
                 RaceVector3 currentPosition, float& score);
float ProjectedProgress(const TrackPiece& piece, RaceVector3 position, float previousProgress,
                        bool forwardRace);
TrackPositionTracker::RecoveryPose RecoveryPoseFor(const Track& track,
                                                   const TrackProgressTransition& transition);
TrackPositionTracker::RecoveryPose StartRecoveryPoseFor(const TrackProgressPortal& finishPortal);

} // namespace TrackPositionGeometry
