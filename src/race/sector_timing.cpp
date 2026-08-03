#include "sector_timing.hpp"

#include <cstddef>

RaceTimingSnapshot::RaceTimingSnapshot()
    : sectorsAvailable(false), completedLaps(0), currentSectorIndex(0),
      currentSectorTime(0.0f), currentLapTime(0.0f), lastLapTime(0.0f),
      bestLapTime(0.0f), totalTime(0.0f), currentLapSectorTimes(),
      bestSectorTimes(), currentLapSectorCompleted() {
    currentLapSectorTimes.fill(0.0f);
    bestSectorTimes.fill(0.0f);
    currentLapSectorCompleted.fill(false);
}

SectorTimingUpdate::SectorTimingUpdate()
    : sectorCompleted(false), completedSectorIndex(-1), completedSectorTime(0.0f),
      lapCompleted(false), completedLapTime(0.0f) {
}

SectorTiming::SectorTiming() : lapCommitted_(false), snapshot_() {
}

void SectorTiming::Configure(bool sectorsAvailable) {
    snapshot_.sectorsAvailable = sectorsAvailable;
    ResetAttempt();
}

void SectorTiming::ResetAttempt() {
    const bool sectorsAvailable = snapshot_.sectorsAvailable;
    snapshot_ = RaceTimingSnapshot();
    snapshot_.sectorsAvailable = sectorsAvailable;
    lapCommitted_ = false;
}

SectorTimingUpdate SectorTiming::Advance(
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

void SectorTiming::BeginNextLap() {
    snapshot_.currentSectorIndex = 0;
    snapshot_.currentSectorTime = 0.0f;
    snapshot_.currentLapTime = 0.0f;
    snapshot_.currentLapSectorTimes.fill(0.0f);
    snapshot_.currentLapSectorCompleted.fill(false);
    lapCommitted_ = false;
}

const RaceTimingSnapshot& SectorTiming::Snapshot() const {
    return snapshot_;
}

bool SectorTiming::CommitSector(int sectorIndex, float& completedTime) {
    if (!snapshot_.sectorsAvailable || sectorIndex < 0 || sectorIndex > 2 ||
        sectorIndex != snapshot_.currentSectorIndex ||
        snapshot_.currentLapSectorCompleted[static_cast<std::size_t>(sectorIndex)]) {
        return false;
    }

    completedTime = snapshot_.currentSectorTime;
    snapshot_.currentLapSectorTimes[static_cast<std::size_t>(sectorIndex)] = completedTime;
    snapshot_.currentLapSectorCompleted[static_cast<std::size_t>(sectorIndex)] = true;
    float& best = snapshot_.bestSectorTimes[static_cast<std::size_t>(sectorIndex)];
    if (best == 0.0f || completedTime < best) best = completedTime;

    snapshot_.currentSectorTime = 0.0f;
    if (sectorIndex < 2) snapshot_.currentSectorIndex = sectorIndex + 1;
    return true;
}
