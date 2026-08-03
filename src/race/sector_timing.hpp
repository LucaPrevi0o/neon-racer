#pragma once

#include "internal/track_position_tracker.hpp"

#include <array>
#include <cstddef>

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

    // Monotonic split event data. Presentation can remember splitSequence and
    // react exactly once even when Sector 3 and BeginNextLap occur in the same
    // fixed update.
    unsigned int splitSequence;
    int lastCompletedSectorIndex;
    float lastCompletedSectorTime;
    bool lastCompletedSectorImprovedBest;

    RaceTimingSnapshot()
        : sectorsAvailable(false), completedLaps(0), currentSectorIndex(0),
          currentSectorTime(0.0f), currentLapTime(0.0f), lastLapTime(0.0f),
          bestLapTime(0.0f), totalTime(0.0f), currentLapSectorTimes(),
          bestSectorTimes(), currentLapSectorCompleted(), splitSequence(0u),
          lastCompletedSectorIndex(-1), lastCompletedSectorTime(0.0f),
          lastCompletedSectorImprovedBest(false) {
        currentLapSectorTimes.fill(0.0f);
        bestSectorTimes.fill(0.0f);
        currentLapSectorCompleted.fill(false);
    }
};

// Transient result from one fixed timing step. The immutable snapshot retains
// the authoritative values; this event lets TimeTrial and presentation react
// exactly once when a split or lap is committed.
struct SectorTimingUpdate {
    bool sectorCompleted;
    int completedSectorIndex;
    float completedSectorTime;
    bool lapCompleted;
    float completedLapTime;

    SectorTimingUpdate()
        : sectorCompleted(false), completedSectorIndex(-1), completedSectorTime(0.0f),
          lapCompleted(false), completedLapTime(0.0f) {}
};

// Owns clocks and completed timing records, but not route topology. Exact split
// events come from TrackPositionTracker so this component never guesses from
// world-space proximity or rendering state.
class SectorTiming {
public:
    SectorTiming() : lapCommitted_(false), snapshot_() {}

    void Configure(bool sectorsAvailable) {
        snapshot_.sectorsAvailable = sectorsAvailable;
        ResetAttempt();
    }

    void ResetAttempt() {
        const bool sectorsAvailable = snapshot_.sectorsAvailable;
        snapshot_ = RaceTimingSnapshot();
        snapshot_.sectorsAvailable = sectorsAvailable;
        lapCommitted_ = false;
    }

    SectorTimingUpdate Advance(
        float deltaTime,
        const TrackPositionTracker::UpdateResult& progressUpdate) {
        SectorTimingUpdate update;
        if (lapCommitted_) return update;

        if (deltaTime > 0.0f) {
            snapshot_.currentLapTime += deltaTime;
            snapshot_.totalTime += deltaTime;
            if (snapshot_.sectorsAvailable) snapshot_.currentSectorTime += deltaTime;
        }

        if (snapshot_.sectorsAvailable && progressUpdate.sectorBoundaryCrossed) {
            float completedTime = 0.0f;
            if (CommitSector(progressUpdate.completedSectorIndex, completedTime)) {
                update.sectorCompleted = true;
                update.completedSectorIndex = progressUpdate.completedSectorIndex;
                update.completedSectorTime = completedTime;
            }
        }

        if (progressUpdate.lapCompleted) {
            if (snapshot_.sectorsAvailable && !snapshot_.currentLapSectorCompleted[2]) {
                float completedTime = 0.0f;
                if (CommitSector(2, completedTime)) {
                    update.sectorCompleted = true;
                    update.completedSectorIndex = 2;
                    update.completedSectorTime = completedTime;
                }
            }

            snapshot_.lastLapTime = snapshot_.currentLapTime;
            if (snapshot_.bestLapTime == 0.0f ||
                snapshot_.currentLapTime < snapshot_.bestLapTime) {
                snapshot_.bestLapTime = snapshot_.currentLapTime;
            }
            ++snapshot_.completedLaps;
            lapCommitted_ = true;
            update.lapCompleted = true;
            update.completedLapTime = snapshot_.currentLapTime;
        }

        return update;
    }

    void BeginNextLap() {
        snapshot_.currentSectorIndex = 0;
        snapshot_.currentSectorTime = 0.0f;
        snapshot_.currentLapTime = 0.0f;
        snapshot_.currentLapSectorTimes.fill(0.0f);
        snapshot_.currentLapSectorCompleted.fill(false);
        lapCommitted_ = false;
    }

    const RaceTimingSnapshot& Snapshot() const { return snapshot_; }

private:
    bool CommitSector(int sectorIndex, float& completedTime) {
        if (!snapshot_.sectorsAvailable || sectorIndex < 0 || sectorIndex > 2 ||
            sectorIndex != snapshot_.currentSectorIndex ||
            snapshot_.currentLapSectorCompleted[static_cast<std::size_t>(sectorIndex)]) {
            return false;
        }

        completedTime = snapshot_.currentSectorTime;
        snapshot_.currentLapSectorTimes[static_cast<std::size_t>(sectorIndex)] = completedTime;
        snapshot_.currentLapSectorCompleted[static_cast<std::size_t>(sectorIndex)] = true;
        float& best = snapshot_.bestSectorTimes[static_cast<std::size_t>(sectorIndex)];
        const bool improvedBest = best == 0.0f || completedTime < best;
        if (improvedBest) best = completedTime;

        ++snapshot_.splitSequence;
        snapshot_.lastCompletedSectorIndex = sectorIndex;
        snapshot_.lastCompletedSectorTime = completedTime;
        snapshot_.lastCompletedSectorImprovedBest = improvedBest;

        snapshot_.currentSectorTime = 0.0f;
        if (sectorIndex < 2) snapshot_.currentSectorIndex = sectorIndex + 1;
        return true;
    }

    bool lapCommitted_;
    RaceTimingSnapshot snapshot_;
};
