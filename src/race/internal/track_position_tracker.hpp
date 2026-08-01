#pragma once

#include "../race_contracts.hpp"

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
        std::uint32_t toPieceId;
        Gate gate;
        RecoveryPose recoveryPose;
    };

    TrackPositionTracker();

    void Configure(const Track& track);
    void Reset();
    void Update(RaceVector3 previousPosition, RaceVector3 currentPosition);
    void BeginNextLap();

    bool LapCompleted() const;
    bool HasDepartedStart() const;
    bool HasReturnedToStart() const;
    std::uint32_t CurrentPieceId() const;
    float CurrentProgress() const;
    const RecoveryPose& CurrentRecoveryPose() const;

private:
    typedef std::map<std::uint32_t, std::vector<Transition> > TransitionGraph;

    void UpdateProjectedProgress(RaceVector3 position);

    const Track* track_;
    TransitionGraph transitions_;
    Gate finishGate_;
    RecoveryPose startRecoveryPose_;
    RecoveryPose recoveryPose_;
    std::uint32_t startPieceId_;
    std::uint32_t currentPieceId_;
    float currentProgress_;
    bool configured_;
    bool hasDepartedStart_;
    bool hasReturnedToStart_;
    bool lapCompleted_;
    bool departedNextLapAtFinish_;
};