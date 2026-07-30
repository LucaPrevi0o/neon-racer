#include "track.hpp"

namespace {

bool IsWithinSymmetricRange(int value, int maximumMagnitude) {
    return value >= -maximumMagnitude && value <= maximumMagnitude;
}

bool IsValidEntryPosition(const GridPosition& position) {
    return IsWithinSymmetricRange(position.x, TrackLimits::kMaximumGridCoordinate) &&
           IsWithinSymmetricRange(position.y, TrackLimits::kMaximumGridCoordinate) &&
           IsWithinSymmetricRange(position.z, TrackLimits::kMaximumGridCoordinate);
}

bool IsKnownPieceType(TrackPieceType type) {
    return type >= TrackPieceType::Straight && type <= TrackPieceType::Merge;
}

bool IsKnownHeading(Heading heading) {
    return heading >= Heading::North && heading <= Heading::West;
}

bool IsKnownTurn(CurveTurn turn) {
    return turn == CurveTurn::Left || turn == CurveTurn::Right;
}

bool IsKnownMaterial(SurfaceMaterial material) {
    return material >= SurfaceMaterial::Regular && material <= SurfaceMaterial::HighResistance;
}

} // namespace

// Component admissibility is independent of topology: a valid piece can still
// be disconnected or overlap another piece in a particular layout.
bool Track::IsValidPiece(const TrackPiece& piece) {
    if (!IsKnownPieceType(piece.type) || !IsKnownHeading(piece.entryHeading) || !IsKnownTurn(piece.curveTurn) ||
        !IsKnownMaterial(piece.material) || !IsValidEntryPosition(piece.entryPosition) ||
        !IsWithinSymmetricRange(piece.elevationDelta, TrackLimits::kMaximumElevationDelta)) {
        return false;
    }
    if (piece.width < 5 || piece.width > 11 || piece.exitWidth < 5 || piece.exitWidth > 11) return false;
    if (piece.type == TrackPieceType::Straight) {
        const int maximumOffset = piece.length < 4 ? 2 : 3;
        return piece.length >= 3 && piece.length <= 20 &&
               IsWithinSymmetricRange(piece.lateralOffset, maximumOffset);
    }
    if (piece.type == TrackPieceType::Twist) {
        if (TrackLimits::IsLegacyFlatTwistRadius(piece.curveRadius)) {
            const int maximumOffset = piece.length < 4 ? 2 : 3;
            return piece.length >= 3 && piece.length <= 20 &&
                   IsWithinSymmetricRange(piece.lateralOffset, maximumOffset);
        }
        const int minimumLength = TrackLimits::MinimumTwistLengthForRoadWidth(piece.width, piece.exitWidth);
        return piece.length >= minimumLength && piece.length <= TrackLimits::kMaximumTwistLength &&
               TrackLimits::IsValidTwistRadius(piece.curveRadius, piece.width, piece.exitWidth) &&
               IsWithinSymmetricRange(piece.lateralOffset, 3);
    }
    if (piece.type == TrackPieceType::Loop)
        return piece.curveRadius >= 3 && piece.curveRadius <= 10 &&
               IsWithinSymmetricRange(piece.lateralOffset, 10);
    if (piece.type == TrackPieceType::Branch || piece.type == TrackPieceType::Merge)
        return piece.length >= 3 && piece.length <= 20 &&
               (piece.lateralOffset <= -1 || piece.lateralOffset >= 1) &&
               IsWithinSymmetricRange(piece.lateralOffset, 10) && piece.elevationDelta == 0;
    return piece.curveRadius >= 3 && piece.curveRadius <= 20 && piece.lateralOffset == 0 &&
        (piece.curveDegrees == 90 || piece.curveDegrees == 180 || piece.curveDegrees == 270) &&
        IsWithinSymmetricRange(piece.bankAngleDegrees, 45);
}

Track Track::CreateSampleCircuit() {
    Track track;
    const std::uint32_t start = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 6);
    track.AddCurve(GridPosition{6, 0, 0}, Heading::East, CurveTurn::Right, 4);
    track.AddStraight(GridPosition{10, 0, 4}, Heading::South, 6);
    track.AddCurve(GridPosition{10, 0, 10}, Heading::South, CurveTurn::Right, 4);
    track.AddStraight(GridPosition{6, 0, 14}, Heading::West, 6);
    track.AddCurve(GridPosition{0, 0, 14}, Heading::West, CurveTurn::Right, 4);
    track.AddStraight(GridPosition{-4, 0, 10}, Heading::North, 6);
    track.AddCurve(GridPosition{-4, 0, 4}, Heading::North, CurveTurn::Right, 4);
    track.SetStartFinish(start, RaceDirection::Forward);
    return track;
}

const char* HeadingName(Heading heading) {
    switch (heading) {
    case Heading::North: return "North";
    case Heading::East: return "East";
    case Heading::South: return "South";
    case Heading::West: return "West";
    }
    return "Unknown";
}
