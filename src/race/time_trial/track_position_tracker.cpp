#include "../internal/track_position_tracker.hpp"

#include "../internal/track_position_geometry.hpp"
#include "../../track/track.hpp"
#include "../../track/track_progress_graph.hpp"

#include <cmath>
#include <limits>

namespace {

float Clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

bool NearlyEqual(float first, float second) {
    return std::fabs(first - second) < 0.001f;
}

} // namespace

TrackPositionTracker::TrackPositionTracker()
    : track_(0), transitions_(), transitionLengths_(), sectorLayout_(), compatibleRouteVariants_(),
      finishGate_(), startRecoveryPose_(RecoveryPose{RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f}),
      recoveryPose_(RecoveryPose{RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f}),
      startPieceId_(0), currentPieceId_(0), currentProgress_(0.0f), routeTransitionIndex_(0u),
      completedRouteDistance_(0.0f), currentSectorIndex_(0), configured_(false),
      hasDepartedStart_(false), hasReturnedToStart_(false), lapCompleted_(false),
      departedNextLapAtFinish_(false), hasPendingNextLapTransition_(false),
      pendingNextLapTransitionId_(0), progressSnapshot_() {
}

void TrackPositionTracker::Configure(const Track& track) {
    track_ = &track;
    transitions_.clear();
    transitionLengths_.clear();
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        transitions_[transition->fromPieceId].push_back(Transition{
            transition->id,
            transition->toPieceId,
            TrackPositionGeometry::GateFor(transition->portal),
            TrackPositionGeometry::RecoveryPoseFor(track, *transition),
        });
        const TrackPiece* fromPiece = track.GetPiece(transition->fromPieceId);
        if (fromPiece != 0) transitionLengths_[transition->id] = TrackRouteLengthForPiece(*fromPiece);
    }

    sectorLayout_ = BuildTrackSectorLayout(track);
    startPieceId_ = graph.hasStartFinish ? graph.startFinishPieceId : 0;
    configured_ = graph.hasStartFinish && track.GetPiece(startPieceId_) != 0;
    if (configured_) {
        finishGate_ = TrackPositionGeometry::GateFor(graph.finishPortal);
        startRecoveryPose_ = TrackPositionGeometry::StartRecoveryPoseFor(graph.finishPortal);
    }
    Reset();
}

void TrackPositionTracker::Reset() {
    currentPieceId_ = startPieceId_;
    currentProgress_ = track_ != 0 && track_->SelectedRaceDirection() == RaceDirection::Reverse ? 1.0f : 0.0f;
    recoveryPose_ = startRecoveryPose_;
    hasDepartedStart_ = false;
    hasReturnedToStart_ = false;
    lapCompleted_ = false;
    departedNextLapAtFinish_ = false;
    hasPendingNextLapTransition_ = false;
    pendingNextLapTransitionId_ = 0;
    InitializeRouteProgress();
    RefreshProgressSnapshot();
}

TrackPositionTracker::UpdateResult TrackPositionTracker::Update(
    RaceVector3 previousPosition, RaceVector3 currentPosition) {
    UpdateResult result;
    if (!configured_) {
        result.progress = progressSnapshot_;
        return result;
    }

    const bool routeReadyBeforeTransition = currentPieceId_ == startPieceId_ && hasReturnedToStart_;
    float finishScore = 0.0f;
    const bool crossedFinish = TrackPositionGeometry::CrossesGate(
        finishGate_, previousPosition, currentPosition, finishScore);

    TransitionGraph::const_iterator found = transitions_.find(currentPieceId_);
    const Transition* selected = 0;
    float selectedScore = std::numeric_limits<float>::max();
    if (found != transitions_.end()) {
        for (std::vector<Transition>::const_iterator transition = found->second.begin();
             transition != found->second.end(); ++transition) {
            float score = 0.0f;
            if (TrackPositionGeometry::CrossesGate(
                    transition->gate, previousPosition, currentPosition, score) &&
                score < selectedScore) {
                selected = &(*transition);
                selectedScore = score;
            }
        }
    }

    if (selected != 0) {
        const std::uint32_t fromPieceId = currentPieceId_;
        currentPieceId_ = selected->toPieceId;
        recoveryPose_ = selected->recoveryPose;

        result.transition.occurred = true;
        result.transition.transitionId = selected->id;
        result.transition.fromPieceId = fromPieceId;
        result.transition.toPieceId = currentPieceId_;

        const bool belongsToNextLap = fromPieceId == startPieceId_ &&
            currentPieceId_ != startPieceId_ && crossedFinish && routeReadyBeforeTransition;
        if (belongsToNextLap) {
            hasPendingNextLapTransition_ = true;
            pendingNextLapTransitionId_ = selected->id;
        } else {
            int completedSectorIndex = -1;
            result.sectorBoundaryCrossed = ApplyRouteTransition(selected->id, completedSectorIndex);
            result.completedSectorIndex = completedSectorIndex;
        }

        if (fromPieceId == startPieceId_ && currentPieceId_ != startPieceId_) {
            if (belongsToNextLap) departedNextLapAtFinish_ = true;
            else hasDepartedStart_ = true;
        }
        if (currentPieceId_ == startPieceId_ && hasDepartedStart_) hasReturnedToStart_ = true;
    }

    if (crossedFinish && (routeReadyBeforeTransition ||
                          (currentPieceId_ == startPieceId_ && hasReturnedToStart_))) {
        lapCompleted_ = true;
    }

    UpdateProjectedProgress(currentPosition);
    RefreshProgressSnapshot();
    result.progress = progressSnapshot_;
    result.lapCompleted = lapCompleted_;
    return result;
}

void TrackPositionTracker::BeginNextLap() {
    lapCompleted_ = false;
    hasReturnedToStart_ = false;
    hasDepartedStart_ = departedNextLapAtFinish_;
    departedNextLapAtFinish_ = false;

    const bool applyPendingTransition = hasPendingNextLapTransition_;
    const std::uint32_t pendingTransitionId = pendingNextLapTransitionId_;
    hasPendingNextLapTransition_ = false;
    pendingNextLapTransitionId_ = 0;

    InitializeRouteProgress();
    if (applyPendingTransition) {
        int completedSectorIndex = -1;
        ApplyRouteTransition(pendingTransitionId, completedSectorIndex);
    }
    RefreshProgressSnapshot();
}

bool TrackPositionTracker::LapCompleted() const { return lapCompleted_; }
bool TrackPositionTracker::HasDepartedStart() const { return hasDepartedStart_; }
bool TrackPositionTracker::HasReturnedToStart() const { return hasReturnedToStart_; }
std::uint32_t TrackPositionTracker::CurrentPieceId() const { return currentPieceId_; }
float TrackPositionTracker::CurrentProgress() const { return currentProgress_; }
const TrackPositionTracker::ProgressSnapshot& TrackPositionTracker::CurrentSnapshot() const {
    return progressSnapshot_;
}
const TrackPositionTracker::RecoveryPose& TrackPositionTracker::CurrentRecoveryPose() const {
    return recoveryPose_;
}

void TrackPositionTracker::InitializeRouteProgress() {
    compatibleRouteVariants_.clear();
    if (sectorLayout_.IsReady()) {
        for (std::size_t index = 0; index < sectorLayout_.routeVariants.size(); ++index) {
            compatibleRouteVariants_.push_back(index);
        }
    }
    routeTransitionIndex_ = 0u;
    completedRouteDistance_ = 0.0f;
    currentSectorIndex_ = 0;
}

bool TrackPositionTracker::ApplyRouteTransition(
    std::uint32_t transitionId, int& completedSectorIndex) {
    const std::map<std::uint32_t, float>::const_iterator length = transitionLengths_.find(transitionId);
    if (length != transitionLengths_.end()) completedRouteDistance_ += length->second;

    std::vector<std::size_t> remainingVariants;
    for (std::vector<std::size_t>::const_iterator index = compatibleRouteVariants_.begin();
         index != compatibleRouteVariants_.end(); ++index) {
        if (*index >= sectorLayout_.routeVariants.size()) continue;
        const TrackSectorRouteVariant& variant = sectorLayout_.routeVariants[*index];
        if (routeTransitionIndex_ < variant.transitionIds.size() &&
            variant.transitionIds[routeTransitionIndex_] == transitionId) {
            remainingVariants.push_back(*index);
        }
    }
    compatibleRouteVariants_.swap(remainingVariants);
    ++routeTransitionIndex_;

    if (!compatibleRouteVariants_.empty() && currentSectorIndex_ < 2 &&
        IsBoundaryTransition(currentSectorIndex_, transitionId)) {
        completedSectorIndex = currentSectorIndex_;
        ++currentSectorIndex_;
        return true;
    }
    completedSectorIndex = -1;
    return false;
}

bool TrackPositionTracker::IsBoundaryTransition(
    int boundaryIndex, std::uint32_t transitionId) const {
    if (!sectorLayout_.IsReady() || boundaryIndex < 0 ||
        static_cast<std::size_t>(boundaryIndex) >= sectorLayout_.boundaries.size()) {
        return false;
    }
    const TrackSectorBoundary& boundary = sectorLayout_.boundaries[static_cast<std::size_t>(boundaryIndex)];
    for (std::vector<TrackSectorGate>::const_iterator gate = boundary.alternativeGates.begin();
         gate != boundary.alternativeGates.end(); ++gate) {
        if (gate->transitionId == transitionId) return true;
    }
    return false;
}

float TrackPositionTracker::BoundaryDistance(
    const TrackSectorRouteVariant& variant, int boundaryIndex) const {
    const std::uint32_t target = boundaryIndex == 0 ? variant.firstBoundaryTransitionId :
        variant.secondBoundaryTransitionId;
    float distance = 0.0f;
    for (std::vector<std::uint32_t>::const_iterator transitionId = variant.transitionIds.begin();
         transitionId != variant.transitionIds.end(); ++transitionId) {
        const std::map<std::uint32_t, float>::const_iterator length = transitionLengths_.find(*transitionId);
        if (length == transitionLengths_.end()) return -1.0f;
        distance += length->second;
        if (*transitionId == target) return distance;
    }
    return -1.0f;
}

bool TrackPositionTracker::ConsensusRouteLength(float& length) const {
    bool found = false;
    for (std::vector<std::size_t>::const_iterator index = compatibleRouteVariants_.begin();
         index != compatibleRouteVariants_.end(); ++index) {
        if (*index >= sectorLayout_.routeVariants.size()) continue;
        const float candidate = sectorLayout_.routeVariants[*index].length;
        if (!found) {
            length = candidate;
            found = true;
        } else if (!NearlyEqual(length, candidate)) {
            return false;
        }
    }
    return found && length > 0.0f;
}

bool TrackPositionTracker::ConsensusSectorRange(
    float& startDistance, float& endDistance) const {
    bool found = false;
    for (std::vector<std::size_t>::const_iterator index = compatibleRouteVariants_.begin();
         index != compatibleRouteVariants_.end(); ++index) {
        if (*index >= sectorLayout_.routeVariants.size()) continue;
        const TrackSectorRouteVariant& variant = sectorLayout_.routeVariants[*index];
        const float candidateStart = currentSectorIndex_ == 0 ? 0.0f :
            BoundaryDistance(variant, currentSectorIndex_ - 1);
        const float candidateEnd = currentSectorIndex_ < 2 ?
            BoundaryDistance(variant, currentSectorIndex_) : variant.length;
        if (candidateStart < 0.0f || candidateEnd <= candidateStart) return false;
        if (!found) {
            startDistance = candidateStart;
            endDistance = candidateEnd;
            found = true;
        } else if (!NearlyEqual(startDistance, candidateStart) ||
                   !NearlyEqual(endDistance, candidateEnd)) {
            return false;
        }
    }
    return found;
}

float TrackPositionTracker::DirectedPieceProgress() const {
    if (track_ == 0) return 0.0f;
    const float progress = track_->SelectedRaceDirection() == RaceDirection::Forward ?
        currentProgress_ : 1.0f - currentProgress_;
    return Clamp01(progress);
}

void TrackPositionTracker::RefreshProgressSnapshot() {
    progressSnapshot_ = ProgressSnapshot();
    progressSnapshot_.configured = configured_;
    progressSnapshot_.sectorsReady = sectorLayout_.IsReady();
    progressSnapshot_.pieceId = currentPieceId_;
    progressSnapshot_.pieceProgress = DirectedPieceProgress();
    progressSnapshot_.sectorIndex = currentSectorIndex_;

    if (!configured_ || track_ == 0 || !sectorLayout_.IsReady()) return;

    const TrackPiece* piece = track_->GetPiece(currentPieceId_);
    const float pieceLength = piece == 0 ? 0.0f : TrackRouteLengthForPiece(*piece);
    progressSnapshot_.routeDistance = completedRouteDistance_ +
        pieceLength * progressSnapshot_.pieceProgress;

    if (compatibleRouteVariants_.size() == 1u &&
        compatibleRouteVariants_.front() < sectorLayout_.routeVariants.size()) {
        progressSnapshot_.routeVariantId =
            sectorLayout_.routeVariants[compatibleRouteVariants_.front()].id;
    }

    float routeLength = 0.0f;
    if (ConsensusRouteLength(routeLength)) {
        progressSnapshot_.hasLapProgress = true;
        progressSnapshot_.lapProgress = lapCompleted_ ? 1.0f :
            Clamp01(progressSnapshot_.routeDistance / routeLength);
    }

    float sectorStart = 0.0f;
    float sectorEnd = 0.0f;
    if (ConsensusSectorRange(sectorStart, sectorEnd)) {
        progressSnapshot_.hasSectorProgress = true;
        progressSnapshot_.sectorProgress = lapCompleted_ && currentSectorIndex_ == 2 ? 1.0f :
            Clamp01((progressSnapshot_.routeDistance - sectorStart) /
                    (sectorEnd - sectorStart));
    }
}

void TrackPositionTracker::UpdateProjectedProgress(RaceVector3 position) {
    if (track_ == 0) return;
    const TrackPiece* piece = track_->GetPiece(currentPieceId_);
    if (piece == 0) return;
    currentProgress_ = TrackPositionGeometry::ProjectedProgress(
        *piece, position, currentProgress_,
        track_->SelectedRaceDirection() == RaceDirection::Forward);
}
