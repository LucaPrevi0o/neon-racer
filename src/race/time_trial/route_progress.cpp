#include "../internal/route_progress.hpp"

#include "../../track/track.hpp"

namespace {

// Surface ownership can briefly alternate at a shared connector. Requiring two
// consecutive contacts keeps connector ties from becoming false wrong-way
// transitions while still confirming a new piece within a few centimetres of
// ordinary fixed-step travel.
const int kRequiredStableObservations = 2;

} // namespace

RouteProgress::RouteProgress()
    : graph_(), startPieceId_(0), currentPieceId_(0), candidatePieceId_(0),
      candidateObservationCount_(0), configured_(false), hasDepartedStart_(false),
      hasReturnedToStart_(false), valid_(false) {
}

void RouteProgress::Configure(const Track& track) {
    graph_.clear();
    const bool forward = track.SelectedRaceDirection() == RaceDirection::Forward;
    const std::vector<TrackConnection> connections = track.Connections();
    for (std::vector<TrackConnection>::const_iterator connection = connections.begin();
         connection != connections.end(); ++connection) {
        const std::uint32_t fromPieceId = forward ? connection->exit.pieceId : connection->entry.pieceId;
        const std::uint32_t toPieceId = forward ? connection->entry.pieceId : connection->exit.pieceId;
        graph_[fromPieceId].insert(toPieceId);
    }

    startPieceId_ = track.HasStartFinish() ? track.StartFinishPieceId() : 0;
    configured_ = startPieceId_ != 0;
    Reset();
}

void RouteProgress::Reset() {
    currentPieceId_ = startPieceId_;
    candidatePieceId_ = 0;
    candidateObservationCount_ = 0;
    hasDepartedStart_ = false;
    hasReturnedToStart_ = false;
    valid_ = configured_;
}

void RouteProgress::ObserveSurfacePiece(std::uint32_t pieceId) {
    if (!valid_ || pieceId == 0) return;
    if (pieceId == currentPieceId_) {
        candidatePieceId_ = 0;
        candidateObservationCount_ = 0;
        return;
    }

    if (pieceId != candidatePieceId_) {
        candidatePieceId_ = pieceId;
        candidateObservationCount_ = 1;
        // Returning to the start is also synchronized with a narrow geometric
        // finish zone. Confirm that directed edge immediately so a shared
        // connector tie cannot make the car pass the line before the route
        // state catches up on the next fixed step.
        if (pieceId == startPieceId_ && hasDepartedStart_ &&
            IsAllowedTransition(currentPieceId_, pieceId)) {
            AcceptCandidate();
        }
        return;
    }

    ++candidateObservationCount_;
    if (candidateObservationCount_ >= kRequiredStableObservations) AcceptCandidate();
}

void RouteProgress::BeginNextLap() {
    Reset();
}

bool RouteProgress::CanCompleteLap() const {
    return valid_ && hasDepartedStart_ && hasReturnedToStart_ && currentPieceId_ == startPieceId_;
}

bool RouteProgress::IsValid() const {
    return valid_;
}

bool RouteProgress::HasDepartedStart() const {
    return hasDepartedStart_;
}

bool RouteProgress::IsAllowedTransition(std::uint32_t fromPieceId, std::uint32_t toPieceId) const {
    const DirectedGraph::const_iterator found = graph_.find(fromPieceId);
    return found != graph_.end() && found->second.find(toPieceId) != found->second.end();
}

void RouteProgress::AcceptCandidate() {
    const std::uint32_t fromPieceId = currentPieceId_;
    const std::uint32_t toPieceId = candidatePieceId_;
    candidatePieceId_ = 0;
    candidateObservationCount_ = 0;
    currentPieceId_ = toPieceId;

    if (!IsAllowedTransition(fromPieceId, toPieceId)) {
        valid_ = false;
        return;
    }

    if (fromPieceId == startPieceId_ && toPieceId != startPieceId_) hasDepartedStart_ = true;
    if (toPieceId == startPieceId_ && hasDepartedStart_) hasReturnedToStart_ = true;
}
