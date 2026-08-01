#include "../internal/track_position_tracker.hpp"

#include "../internal/track_position_geometry.hpp"
#include "../../track/track.hpp"
#include "../../track/track_progress_graph.hpp"

#include <limits>

TrackPositionTracker::TrackPositionTracker()
    : track_(0), transitions_(), finishGate_(),
      startRecoveryPose_(RecoveryPose{RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f}),
      recoveryPose_(RecoveryPose{RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f}),
      startPieceId_(0), currentPieceId_(0), currentProgress_(0.0f), configured_(false),
      hasDepartedStart_(false), hasReturnedToStart_(false), lapCompleted_(false),
      departedNextLapAtFinish_(false) {
}

void TrackPositionTracker::Configure(const Track& track) {
    track_ = &track;
    transitions_.clear();
    const TrackProgressGraph graph = BuildTrackProgressGraph(track);
    for (std::vector<TrackProgressTransition>::const_iterator transition = graph.transitions.begin();
         transition != graph.transitions.end(); ++transition) {
        transitions_[transition->fromPieceId].push_back(Transition{
            transition->toPieceId,
            TrackPositionGeometry::GateFor(transition->portal),
            TrackPositionGeometry::RecoveryPoseFor(track, *transition),
        });
    }

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
}

void TrackPositionTracker::Update(RaceVector3 previousPosition, RaceVector3 currentPosition) {
    if (!configured_) return;

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
const TrackPositionTracker::RecoveryPose& TrackPositionTracker::CurrentRecoveryPose() const {
    return recoveryPose_;
}

void TrackPositionTracker::UpdateProjectedProgress(RaceVector3 position) {
    if (track_ == 0) return;
    const TrackPiece* piece = track_->GetPiece(currentPieceId_);
    if (piece == 0) return;
    currentProgress_ = TrackPositionGeometry::ProjectedProgress(
        *piece, position, currentProgress_,
        track_->SelectedRaceDirection() == RaceDirection::Forward);
}