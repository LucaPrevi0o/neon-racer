#pragma once

#include <cstdint>
#include <map>
#include <set>

class Track;

// Tracks a car's stable surface-piece transitions through the selected race
// direction. A lap becomes eligible only after the car leaves the start piece,
// follows directed connector edges, and returns to the start piece.
class RouteProgress {
public:
    RouteProgress();

    void Configure(const Track& track);
    void Reset();
    void ObserveSurfacePiece(std::uint32_t pieceId);
    void BeginNextLap();

    bool CanCompleteLap() const;
    bool IsValid() const;
    bool HasDepartedStart() const;

private:
    typedef std::map<std::uint32_t, std::set<std::uint32_t> > DirectedGraph;

    bool IsAllowedTransition(std::uint32_t fromPieceId, std::uint32_t toPieceId) const;
    void AcceptCandidate();

    DirectedGraph graph_;
    std::uint32_t startPieceId_;
    std::uint32_t currentPieceId_;
    std::uint32_t candidatePieceId_;
    int candidateObservationCount_;
    bool configured_;
    bool hasDepartedStart_;
    bool hasReturnedToStart_;
    bool valid_;
};
