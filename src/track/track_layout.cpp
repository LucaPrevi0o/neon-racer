#include "track.hpp"

#include <algorithm>

// This unit owns the mutable Track layout: component IDs, replacement and
// removal, start/finish selection, and validation-cache invalidation. Piece
// geometry and overlap detection deliberately stay in their own units.

Track::Track()
    : nextPieceId_(1), startFinishPieceId_(0), raceDirection_(RaceDirection::Forward),
      layoutRevision_(0), validationDirty_(true), cachedValidation_{false, std::vector<TrackIssue>()} {
}

std::uint32_t Track::AddStraight(GridPosition entry, Heading heading, int length, int width,
                                 int requestedExitWidth, int elevationDelta, int lateralOffset,
                                 SurfaceMaterial material) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Straight, entry, heading, width, resolvedExitWidth,
                               length, CurveTurn::Right, 0, 90, 0, elevationDelta, lateralOffset, material});
}

std::uint32_t Track::AddCurve(GridPosition entry, Heading heading, CurveTurn turn, int radius, int width,
                              int requestedExitWidth, int elevationDelta, SurfaceMaterial material, int curveDegrees,
                              int bankAngleDegrees) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Curve, entry, heading, width, resolvedExitWidth,
                               0, turn, radius, curveDegrees, bankAngleDegrees, elevationDelta, 0, material});
}

std::uint32_t Track::AddLoop(GridPosition entry, Heading heading, int radius, int width,
                             int requestedExitWidth, int elevationDelta, SurfaceMaterial material, int lateralOffset) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Loop, entry, heading, width, resolvedExitWidth,
                               0, CurveTurn::Right, radius, 360, 0, elevationDelta, lateralOffset, material});
}

std::uint32_t Track::AddTwist(GridPosition entry, Heading heading, int length, int width,
                              int requestedExitWidth, int elevationDelta, int lateralOffset,
                              SurfaceMaterial material) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Twist, entry, heading, width, resolvedExitWidth,
                               length, CurveTurn::Right, 0, 90, 0, elevationDelta, lateralOffset, material});
}

std::uint32_t Track::AddBranch(GridPosition entry, Heading heading, int length, int armOffset,
                               int width, SurfaceMaterial material) {
    return AddPiece(TrackPiece{0, TrackPieceType::Branch, entry, heading, width, width,
                               length, CurveTurn::Right, 0, 90, 0, 0, armOffset, material});
}

std::uint32_t Track::AddMerge(GridPosition exit, Heading heading, int length, int armOffset,
                              int width, SurfaceMaterial material) {
    return AddPiece(TrackPiece{0, TrackPieceType::Merge, exit, heading, width, width,
                               length, CurveTurn::Right, 0, 90, 0, 0, armOffset, material});
}

bool Track::RemovePiece(std::uint32_t id) {
    const std::vector<TrackPiece>::iterator found = std::find_if(pieces_.begin(), pieces_.end(),
        [id](const TrackPiece& piece) { return piece.id == id; });
    if (found == pieces_.end()) return false;
    pieces_.erase(found);
    if (startFinishPieceId_ == id) startFinishPieceId_ = 0;
    InvalidateValidation();
    return true;
}

std::uint32_t Track::Add(const TrackPiece& piece) {
    return AddPiece(piece);
}

void Track::Clear() {
    pieces_.clear();
    startFinishPieceId_ = 0;
    raceDirection_ = RaceDirection::Forward;
    nextPieceId_ = 1;
    InvalidateValidation();
}

const std::vector<TrackPiece>& Track::Pieces() const {
    return pieces_;
}

const TrackPiece* Track::GetPiece(std::uint32_t id) const {
    return FindPiece(id);
}

bool Track::ReplacePiece(const TrackPiece& replacement) {
    if (replacement.id == 0 || !IsValidPiece(replacement)) return false;
    if (FindPiece(replacement.id) == 0 || HasOverlappingGeometry(replacement)) return false;

    for (std::vector<TrackPiece>::iterator piece = pieces_.begin(); piece != pieces_.end(); ++piece) {
        if (piece->id == replacement.id) {
            *piece = replacement;
            InvalidateValidation();
            return true;
        }
    }
    return false;
}

std::uint32_t Track::LayoutRevision() const {
    return layoutRevision_;
}

bool Track::SetStartFinish(std::uint32_t pieceId, RaceDirection direction) {
    if (direction != RaceDirection::Forward && direction != RaceDirection::Reverse) return false;
    const TrackPiece* piece = FindPiece(pieceId);
    if (piece == 0 || piece->type != TrackPieceType::Straight) return false;
    startFinishPieceId_ = pieceId;
    raceDirection_ = direction;
    InvalidateValidation();
    return true;
}

bool Track::HasStartFinish() const {
    return startFinishPieceId_ != 0;
}

std::uint32_t Track::StartFinishPieceId() const {
    return startFinishPieceId_;
}

RaceDirection Track::SelectedRaceDirection() const {
    return raceDirection_;
}

bool Track::IsLayoutValid() const {
    const TrackValidation validation = Validate();
    for (std::vector<TrackIssue>::const_iterator issue = validation.issues.begin();
         issue != validation.issues.end(); ++issue) {
        if (issue->kind != TrackIssueKind::MissingStartFinish &&
            issue->kind != TrackIssueKind::InvalidStartFinish) {
            return false;
        }
    }
    return true;
}

std::uint32_t Track::AddPiece(const TrackPiece& piece) {
    TrackPiece candidate = piece;
    if (!IsValidPiece(candidate)) return 0;
    candidate.id = nextPieceId_;
    if (HasOverlappingGeometry(candidate)) return 0;
    pieces_.push_back(candidate);
    ++nextPieceId_;
    InvalidateValidation();
    return candidate.id;
}

void Track::InvalidateValidation() {
    validationDirty_ = true;
    ++layoutRevision_;
}

const TrackPiece* Track::FindPiece(std::uint32_t id) const {
    for (std::vector<TrackPiece>::const_iterator it = pieces_.begin(); it != pieces_.end(); ++it) {
        if (it->id == id) return &(*it);
    }
    return 0;
}
