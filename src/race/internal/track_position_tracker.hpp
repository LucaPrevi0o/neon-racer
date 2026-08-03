#pragma once

#include "../race_contracts.hpp"
#include "../../track/track_sector_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

class Track;

// Maintains the car's last topologically confirmed position on the selected
// race route. Ordinary nearest-surface fluctuations never change route state:
// only movement through an expected connector gate advances to another piece.
class TrackPositionTracker {
public:
    struct Gate {
        RaceVector3 center;
        RaceVector3 forward;
        float halfWidth;
    };

    struct RecoveryPose {
        RaceVector3 position;
        float headingRadians;
    };

    struct Transition {
        std::uint32_t id;
        std::uint32_t toPieceId;
        Gate gate;
        RecoveryPose recoveryPose;
    };

    struct TransitionEvent {
        bool occurred;
        std::uint32_t transitionId;
        std::uint32_t fromPieceId;
        std::uint32_t toPieceId;

        TransitionEvent()
            : occurred(false), transitionId(0), fromPieceId(0), toPieceId(0) {}
    };

    // Immutable race-facing progress. Piece progress always follows the
    // selected race direction, so both forward and reverse attempts begin at
    // zero and approach one as the confirmed piece is traversed.
    struct ProgressSnapshot {
        bool configured;
        bool sectorsReady;
        std::uint32_t pieceId;
        float pieceProgress;
        int sectorIndex;
        std::uint32_t routeVariantId;
        float routeDistance;
        float lapProgress;
        float sectorProgress;
        bool hasLapProgress;
        bool hasSectorProgress;

        ProgressSnapshot()
            : configured(false), sectorsReady(false), pieceId(0), pieceProgress(0.0f),
              sectorIndex(0), routeVariantId(0), routeDistance(0.0f), lapProgress(0.0f),
              sectorProgress(0.0f), hasLapProgress(false), hasSectorProgress(false) {}
    };

    struct UpdateResult {
        TransitionEvent transition;
        ProgressSnapshot progress;
        bool sectorBoundaryCrossed;
        int completedSectorIndex;
        bool lapCompleted;

        UpdateResult()
            : transition(), progress(), sectorBoundaryCrossed(false),
              completedSectorIndex(-1), lapCompleted(false) {}
    };

    TrackPositionTracker();

    void Configure(const Track& track);
    void Reset();
    UpdateResult Update(RaceVector3 previousPosition, RaceVector3 currentPosition);
    void BeginNextLap();

    bool LapCompleted() const;
    bool HasDepartedStart() const;
    bool HasReturnedToStart() const;
    std::uint32_t CurrentPieceId() const;
    float CurrentProgress() const;
    const ProgressSnapshot& CurrentSnapshot() const;
    const RecoveryPose& CurrentRecoveryPose() const;

private:
    typedef std::map<std::uint32_t, std::vector<Transition> > TransitionGraph;

    void InitializeRouteProgress();
    bool ApplyRouteTransition(std::uint32_t transitionId, int& completedSectorIndex);
    bool IsBoundaryTransition(int boundaryIndex, std::uint32_t transitionId) const;
    float BoundaryDistance(const TrackSectorRouteVariant& variant, int boundaryIndex) const;
    bool ConsensusRouteLength(float& length) const;
    bool ConsensusSectorRange(float& startDistance, float& endDistance) const;
    float DirectedPieceProgress() const;
    void RefreshProgressSnapshot();
    void UpdateProjectedProgress(RaceVector3 position);

    const Track* track_;
    TransitionGraph transitions_;
    std::map<std::uint32_t, float> transitionLengths_;
    TrackSectorLayout sectorLayout_;
    std::vector<std::size_t> compatibleRouteVariants_;
    Gate finishGate_;
    RecoveryPose startRecoveryPose_;
    RecoveryPose recoveryPose_;
    std::uint32_t startPieceId_;
    std::uint32_t currentPieceId_;
    float currentProgress_;
    std::size_t routeTransitionIndex_;
    float completedRouteDistance_;
    int currentSectorIndex_;
    bool configured_;
    bool hasDepartedStart_;
    bool hasReturnedToStart_;
    bool lapCompleted_;
    bool departedNextLapAtFinish_;
    bool hasPendingNextLapTransition_;
    std::uint32_t pendingNextLapTransitionId_;
    ProgressSnapshot progressSnapshot_;
};
