#include "../internal/track_position_geometry.hpp"

#include "../../track/track.hpp"
#include "../../track/track_progress_graph.hpp"
#include "../../track/track_road_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

const float kGateLateralMargin = 0.75f;
const float kGateVerticalTolerance = 2.5f;
const float kMinimumForwardMovement = 0.0001f;
const float kProjectionTieTolerance = 0.0025f;
const float kRecoveryInset = 0.75f;

RaceVector3 HeadingVector(Heading heading) {
    switch (heading) {
    case Heading::North: return RaceVector3{0.0f, 0.0f, -1.0f};
    case Heading::East: return RaceVector3{1.0f, 0.0f, 0.0f};
    case Heading::South: return RaceVector3{0.0f, 0.0f, 1.0f};
    case Heading::West: return RaceVector3{-1.0f, 0.0f, 0.0f};
    }
    return RaceVector3{0.0f, 0.0f, 0.0f};
}

RaceVector3 Scale(RaceVector3 vector, float amount) {
    return RaceVector3{vector.x * amount, vector.y * amount, vector.z * amount};
}

RaceVector3 Add(RaceVector3 first, RaceVector3 second) {
    return RaceVector3{first.x + second.x, first.y + second.y, first.z + second.z};
}

RaceVector3 Subtract(RaceVector3 first, RaceVector3 second) {
    return RaceVector3{first.x - second.x, first.y - second.y, first.z - second.z};
}

float Dot(RaceVector3 first, RaceVector3 second) {
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

float Length(RaceVector3 vector) {
    return std::sqrt(Dot(vector, vector));
}

float DistanceSquared(RaceVector3 first, RaceVector3 second) {
    return Dot(Subtract(first, second), Subtract(first, second));
}

RaceVector3 Point(const TrackPathPoint& point) {
    return RaceVector3{point.x, point.y, point.z};
}

RaceVector3 ConnectorPoint(const TrackConnector& connector) {
    return RaceVector3{static_cast<float>(connector.position.x),
                       static_cast<float>(connector.position.y),
                       static_cast<float>(connector.position.z)};
}

float HeadingRadians(RaceVector3 direction, Heading fallbackHeading) {
    if (direction.x * direction.x + direction.z * direction.z <= 0.000001f)
        direction = HeadingVector(fallbackHeading);
    return std::atan2(direction.z, direction.x);
}

bool SamplePathFromArrival(const std::vector<TrackPathPoint>& path, bool forwardRace,
                           float distance, RaceVector3& position, RaceVector3& tangent) {
    if (path.size() < 2) return false;
    float remaining = distance;

    if (forwardRace) {
        for (std::size_t index = 0; index + 1 < path.size(); ++index) {
            const RaceVector3 from = Point(path[index]);
            const RaceVector3 segment = Subtract(Point(path[index + 1]), from);
            const float segmentLength = Length(segment);
            if (segmentLength <= 0.0001f) continue;
            if (remaining <= segmentLength) {
                position = Add(from, Scale(segment, remaining / segmentLength));
                tangent = Scale(segment, 1.0f / segmentLength);
                return true;
            }
            remaining -= segmentLength;
        }
    } else {
        for (std::size_t index = path.size() - 1; index > 0; --index) {
            const RaceVector3 from = Point(path[index]);
            const RaceVector3 segment = Subtract(Point(path[index - 1]), from);
            const float segmentLength = Length(segment);
            if (segmentLength <= 0.0001f) continue;
            if (remaining <= segmentLength) {
                position = Add(from, Scale(segment, remaining / segmentLength));
                tangent = Scale(segment, 1.0f / segmentLength);
                return true;
            }
            remaining -= segmentLength;
        }
    }
    return false;
}

float PathLength(const std::vector<TrackPathPoint>& path) {
    float length = 0.0f;
    for (std::size_t index = 0; index + 1 < path.size(); ++index)
        length += Length(Subtract(Point(path[index + 1]), Point(path[index])));
    return length;
}

} // namespace

namespace TrackPositionGeometry {

TrackPositionTracker::Gate GateFor(const TrackProgressPortal& portal) {
    return TrackPositionTracker::Gate{
        ConnectorPoint(portal.connector),
        HeadingVector(portal.crossingHeading),
        static_cast<float>(portal.connector.width) * 0.5f,
    };
}

bool CrossesGate(const TrackPositionTracker::Gate& gate, RaceVector3 previousPosition,
                 RaceVector3 currentPosition, float& score) {
    const RaceVector3 movement = Subtract(currentPosition, previousPosition);
    if (Dot(movement, gate.forward) <= kMinimumForwardMovement) return false;

    const float previousPlane = Dot(Subtract(previousPosition, gate.center), gate.forward);
    const float currentPlane = Dot(Subtract(currentPosition, gate.center), gate.forward);
    if (previousPlane > 0.0f || currentPlane <= 0.0f) return false;

    const float denominator = currentPlane - previousPlane;
    if (denominator <= 0.0f) return false;
    const float crossingTime = std::max(0.0f, std::min(1.0f, -previousPlane / denominator));
    const RaceVector3 crossing = Add(previousPosition, Scale(movement, crossingTime));
    const RaceVector3 side = RaceVector3{-gate.forward.z, 0.0f, gate.forward.x};
    const float lateral = std::fabs(Dot(Subtract(crossing, gate.center), side));
    const float vertical = std::fabs(crossing.y - gate.center.y);
    if (lateral > gate.halfWidth + kGateLateralMargin || vertical > kGateVerticalTolerance) return false;

    score = lateral + vertical * 0.25f + crossingTime * 0.001f;
    return true;
}

TrackPositionTracker::RecoveryPose RecoveryPoseFor(const Track& track,
                                                   const TrackProgressTransition& transition) {
    const RaceVector3 fallbackDirection = HeadingVector(transition.portal.crossingHeading);
    const TrackPositionTracker::RecoveryPose fallback = TrackPositionTracker::RecoveryPose{
        Add(ConnectorPoint(transition.arrivalConnector), Scale(fallbackDirection, 0.25f)),
        HeadingRadians(fallbackDirection, transition.portal.crossingHeading),
    };

    const TrackPiece* destination = track.GetPiece(transition.toPieceId);
    if (destination == 0) return fallback;

    const bool forwardRace = track.SelectedRaceDirection() == RaceDirection::Forward;
    const RaceVector3 arrival = ConnectorPoint(transition.arrivalConnector);
    const std::vector<TrackPiece> arms = TrackRoadGeometry::PhysicalRoadArms(*destination);
    std::vector<TrackPathPoint> bestPath;
    float bestDistanceSquared = std::numeric_limits<float>::max();

    for (std::vector<TrackPiece>::const_iterator arm = arms.begin(); arm != arms.end(); ++arm) {
        const std::vector<TrackPathPoint> path = arm->PathPoints();
        if (path.size() < 2) continue;
        const RaceVector3 endpoint = forwardRace ? Point(path.front()) : Point(path.back());
        const float distanceSquared = DistanceSquared(endpoint, arrival);
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestPath = path;
        }
    }

    const float totalLength = PathLength(bestPath);
    if (totalLength <= 0.0001f) return fallback;
    const float inset = std::min(kRecoveryInset, totalLength * 0.25f);
    RaceVector3 position;
    RaceVector3 tangent;
    if (!SamplePathFromArrival(bestPath, forwardRace, inset, position, tangent)) return fallback;

    return TrackPositionTracker::RecoveryPose{
        position,
        HeadingRadians(tangent, transition.portal.crossingHeading),
    };
}

TrackPositionTracker::RecoveryPose StartRecoveryPoseFor(const TrackProgressPortal& finishPortal) {
    const RaceVector3 direction = HeadingVector(finishPortal.crossingHeading);
    return TrackPositionTracker::RecoveryPose{
        ConnectorPoint(finishPortal.connector),
        HeadingRadians(direction, finishPortal.crossingHeading),
    };
}

float ProjectedProgress(const TrackPiece& piece, RaceVector3 position, float previousProgress,
                        bool forwardRace) {
    const std::vector<TrackPiece> arms = TrackRoadGeometry::PhysicalRoadArms(piece);
    float bestDistanceSquared = std::numeric_limits<float>::max();
    float bestProgress = forwardRace ? 0.0f : 1.0f;

    for (std::vector<TrackPiece>::const_iterator arm = arms.begin(); arm != arms.end(); ++arm) {
        const std::vector<TrackPathPoint> path = arm->PathPoints();
        if (path.size() < 2) continue;

        const float totalLength = PathLength(path);
        if (totalLength <= 0.0001f) continue;
        float distanceAlongPath = 0.0f;

        for (std::size_t index = 0; index + 1 < path.size(); ++index) {
            const RaceVector3 from = Point(path[index]);
            const RaceVector3 segment = Subtract(Point(path[index + 1]), from);
            const float segmentLength = Length(segment);
            if (segmentLength <= 0.0001f) continue;

            const float parameter = std::max(0.0f, std::min(1.0f,
                Dot(Subtract(position, from), segment) / (segmentLength * segmentLength)));
            const RaceVector3 projected = Add(from, Scale(segment, parameter));
            const float distanceSquared = DistanceSquared(position, projected);
            const float geometricProgress = (distanceAlongPath + segmentLength * parameter) / totalLength;
            const float raceProgress = forwardRace ? geometricProgress : 1.0f - geometricProgress;

            const bool clearlyCloser = distanceSquared + kProjectionTieTolerance < bestDistanceSquared;
            const bool equallyCloseAndContinuous = std::fabs(distanceSquared - bestDistanceSquared) <=
                    kProjectionTieTolerance &&
                std::fabs(raceProgress - previousProgress) < std::fabs(bestProgress - previousProgress);
            if (clearlyCloser || equallyCloseAndContinuous) {
                bestDistanceSquared = distanceSquared;
                bestProgress = raceProgress;
            }
            distanceAlongPath += segmentLength;
        }
    }

    return std::max(0.0f, std::min(1.0f, bestProgress));
}

} // namespace TrackPositionGeometry