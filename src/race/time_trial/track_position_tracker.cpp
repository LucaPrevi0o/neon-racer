#include "../internal/track_position_tracker.hpp"

#include "../../track/track.hpp"
#include "../../track/track_progress_graph.hpp"
#include "../../track/track_road_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

const float kGateLateralMargin = 0.75f;
const float kGateVerticalTolerance = 2.5f;
const float kMinimumForwardMovement = 0.0001f;
const float kProjectionTieTolerance = 0.0025f;

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
    const RaceVector3 difference = Subtract(first, second);
    return Dot(difference, difference);
}

RaceVector3 Point(const TrackPathPoint& point) {
    return RaceVector3{point.x, point.y, point.z};
}

TrackPositionTracker::Gate GateFor(const TrackProgressPortal& portal) {
    const TrackConnector& connector = portal.connector;
    return TrackPositionTracker::Gate{
        RaceVector3{static_cast<float>(connector.position.x), static_cast<float>(connector.position.y),
                    static_cast<float>(connector.position.z)},
        HeadingVector(portal.crossingHeading),
        static_cast<float>(connector.width) * 0.5f,
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

    // At a branch, multiple exit gates can share a plane. Prefer the gate whose
    // centre is closest to the actual crossing point rather than relying on map
    // or connector iteration order.
    score = lateral + vertical * 0.25f + crossingTime * 0.001f;
    return true;
}

float ProjectedProgress(const TrackPiece& piece, RaceVector3 position, float previousProgress,
                        bool forwardRace) {
    const std::vector<TrackPiece> arms = TrackRoadGeometry::PhysicalRoadArms(piece);
    float bestDistanceSquared = std::numeric_limits<float>::max();
    float bestProgress = forwardRace ? 0.0f : 1.0f;

    for (std::vector<TrackPiece>::const_iterator arm = arms.begin(); arm != arms.end(); ++arm) {
        const std::vector<TrackPathPoint> path = arm->PathPoints();
        if (path.size() < 2) continue;

        float totalLength = 0.0f;
        for (std::size_t index = 0; index + 1 < path.size(); ++index)
            totalLength += Length(Subtract(Point(path[index + 1]), Point(path[index])));
        if (totalLength <= 0.0001f) continue;

        float distanceAlongPath = 0.0f;
        for (std::size_t index = 0; index + 1 < path.size(); ++index) {
            const RaceVector3 from = Point(path[index]);
            const RaceVector3 to = Point(path[index + 1]);
            const RaceVector3 segment = Subtract(to, from);
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

} // namespace

TrackPositionTracker::TrackPositionTracker()
    : track_(0), transitions_(), finishGate_(), startPieceId_(0), currentPieceId_(0),
      currentProgress_(0.0f), configured_(false), hasDepartedStart_(false),
      hasReturnedToStart_(false), lapCompleted_(false), departedNextLapAtFinish_(false) {
}

void TrackPositionTracker::Configure(const Track& track) {
    track_ = &track;
    transitions_.clear();
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        transitions_[transition->fromPieceId].push_back(
            Transition{transition->toPieceId, GateFor(transition->portal)});
    }

    startPieceId_ = graph.hasStartFinish ? graph.startFinishPieceId : 0;
    configured_ = graph.hasStartFinish && track.GetPiece(startPieceId_) != 0;
    if (configured_) finishGate_ = GateFor(graph.finishPortal);
    Reset();
}

void TrackPositionTracker::Reset() {
    currentPieceId_ = startPieceId_;
    currentProgress_ = track_ != 0 && track_->SelectedRaceDirection() == RaceDirection::Reverse ? 1.0f : 0.0f;
    hasDepartedStart_ = false;
    hasReturnedToStart_ = false;
    lapCompleted_ = false;
    departedNextLapAtFinish_ = false;
}

void TrackPositionTracker::Update(RaceVector3 previousPosition, RaceVector3 currentPosition) {
    if (!configured_) return;

    const bool routeReadyBeforeTransition = currentPieceId_ == startPieceId_ && hasReturnedToStart_;
    float finishScore = 0.0f;
    const bool crossedFinish = CrossesGate(finishGate_, previousPosition, currentPosition, finishScore);

    TransitionGraph::const_iterator found = transitions_.find(currentPieceId_);
    const Transition* selected = 0;
    float selectedScore = std::numeric_limits<float>::max();
    if (found != transitions_.end()) {
        for (std::vector<Transition>::const_iterator transition = found->second.begin();
             transition != found->second.end(); ++transition) {
            float score = 0.0f;
            if (CrossesGate(transition->gate, previousPosition, currentPosition, score) && score < selectedScore) {
                selected = &(*transition);
                selectedScore = score;
            }
        }
    }

    if (selected != 0) {
        const std::uint32_t fromPieceId = currentPieceId_;
        currentPieceId_ = selected->toPieceId;
        if (fromPieceId == startPieceId_ && currentPieceId_ != startPieceId_) {
            if (crossedFinish && routeReadyBeforeTransition) departedNextLapAtFinish_ = true;
            else hasDepartedStart_ = true;
        }
        if (currentPieceId_ == startPieceId_ && hasDepartedStart_) hasReturnedToStart_ = true;
    }

    if (crossedFinish && (routeReadyBeforeTransition ||
                          (currentPieceId_ == startPieceId_ && hasReturnedToStart_))) {
        lapCompleted_ = true;
    }

    UpdateProjectedProgress(currentPosition);
}

void TrackPositionTracker::BeginNextLap() {
    lapCompleted_ = false;
    hasReturnedToStart_ = false;
    hasDepartedStart_ = departedNextLapAtFinish_;
    departedNextLapAtFinish_ = false;
}

bool TrackPositionTracker::LapCompleted() const { return lapCompleted_; }
bool TrackPositionTracker::HasDepartedStart() const { return hasDepartedStart_; }
bool TrackPositionTracker::HasReturnedToStart() const { return hasReturnedToStart_; }
std::uint32_t TrackPositionTracker::CurrentPieceId() const { return currentPieceId_; }
float TrackPositionTracker::CurrentProgress() const { return currentProgress_; }

void TrackPositionTracker::UpdateProjectedProgress(RaceVector3 position) {
    if (track_ == 0) return;
    const TrackPiece* piece = track_->GetPiece(currentPieceId_);
    if (piece == 0) return;
    currentProgress_ = ProjectedProgress(*piece, position, currentProgress_,
        track_->SelectedRaceDirection() == RaceDirection::Forward);
}
