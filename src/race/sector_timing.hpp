#pragma once

#include "internal/track_position_tracker.hpp"

#include <array>

// Immutable timing state consumed by presentation code. Sector indices are
// zero-based in the domain layer; TimeTrial exposes one-based convenience
// accessors for the HUD-facing current sector and lap numbers.
struct RaceTimingSnapshot {
    bool sectorsAvailable;
    int completedLaps;
    int currentSectorIndex;
    float currentSectorTime;
    float currentLapTime;
    float lastLapTime;
    float bestLapTime;
    float totalTime;
    std::array<float, 3> currentLapSectorTimes;
    std::array<float, 3> bestSectorTimes;
    std::array<bool, 3> currentLapSectorCompleted;

    RaceTimingSnapshot();
};

// Transient result from one fixed timing step. The immutable snapshot retains
// the authoritative values; this event lets TimeTrial and the future HUD react
// exactly once when a split or lap is committed.
struct SectorTimingUpdate {
    bool sectorCompleted;
    int completedSectorIndex;
    float completedSectorTime;
    bool lapCompleted;
    float completedLapTime;

    SectorTimingUpdate();
};

// Owns clocks and completed timing records, but not route topology. Exact split
// events come from TrackPositionTracker so this component never guesses from
// world-space proximity or rendering state.
class SectorTiming {
public:
    SectorTiming();

    void Configure(bool sectorsAvailable);
    void ResetAttempt();
    SectorTimingUpdate Advance(
        float deltaTime,
        const TrackPositionTracker::UpdateResult& progressUpdate);
    void BeginNextLap();

    const RaceTimingSnapshot& Snapshot() const;

private:
    bool CommitSector(int sectorIndex, float& completedTime);

    bool lapCommitted_;
    RaceTimingSnapshot snapshot_;
};
