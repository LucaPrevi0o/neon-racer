#include "track.hpp"

#include <cmath>

// Component admissibility is independent of topology: a valid piece can still
// be disconnected or overlap another piece in a particular layout.
bool Track::IsValidPiece(const TrackPiece& piece) {
    if (piece.width < 5 || piece.width > 11 || piece.exitWidth < 5 || piece.exitWidth > 11) return false;
    if (piece.type == TrackPieceType::Straight || piece.type == TrackPieceType::Twist) {
        const int maximumOffset = piece.length < 4 ? 2 : 3;
        return piece.length >= 3 && piece.length <= 20 && std::abs(piece.lateralOffset) <= maximumOffset;
    }
    if (piece.type == TrackPieceType::Loop)
        return piece.curveRadius >= 3 && piece.curveRadius <= 10 && std::abs(piece.lateralOffset) <= 10;
    if (piece.type == TrackPieceType::Branch || piece.type == TrackPieceType::Merge)
        return piece.length >= 3 && piece.length <= 20 && std::abs(piece.lateralOffset) >= 1 &&
            std::abs(piece.lateralOffset) <= 10 && piece.elevationDelta == 0;
    return piece.curveRadius >= 3 && piece.curveRadius <= 20 && piece.lateralOffset == 0 &&
        (piece.curveDegrees == 90 || piece.curveDegrees == 180 || piece.curveDegrees == 270) &&
        std::abs(piece.bankAngleDegrees) <= 45;
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
