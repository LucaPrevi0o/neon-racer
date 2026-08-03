#include "../../src/race/sector_timing.hpp"

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

bool NearlyEqual(float first, float second) {
    return std::fabs(first - second) < 0.0001f;
}

TrackPositionTracker::UpdateResult NoProgressEvent() {
    return TrackPositionTracker::UpdateResult();
}

TrackPositionTracker::UpdateResult SectorBoundary(int completedSectorIndex) {
    TrackPositionTracker::UpdateResult result;
    result.sectorBoundaryCrossed = true;
    result.completedSectorIndex = completedSectorIndex;
    return result;
}

TrackPositionTracker::UpdateResult LapFinish() {
    TrackPositionTracker::UpdateResult result;
    result.lapCompleted = true;
    return result;
}

void TestInitialAndResetState() {
    SectorTiming timing;
    timing.Configure(true);
    const RaceTimingSnapshot& initial = timing.Snapshot();
    Expect(initial.sectorsAvailable && initial.completedLaps == 0 &&
               initial.currentSectorIndex == 0 && NearlyEqual(initial.currentSectorTime, 0.0f) &&
               NearlyEqual(initial.currentLapTime, 0.0f) && NearlyEqual(initial.totalTime, 0.0f),
           "configured timing starts at sector one with zeroed clocks");

    timing.Advance(1.5f, NoProgressEvent());
    timing.ResetAttempt();
    const RaceTimingSnapshot& reset = timing.Snapshot();
    Expect(reset.sectorsAvailable && reset.completedLaps == 0 &&
               reset.currentSectorIndex == 0 && NearlyEqual(reset.currentSectorTime, 0.0f) &&
               NearlyEqual(reset.currentLapTime, 0.0f) && NearlyEqual(reset.totalTime, 0.0f) &&
               NearlyEqual(reset.bestLapTime, 0.0f) && NearlyEqual(reset.lastLapTime, 0.0f),
           "attempt reset clears active and session timing while retaining sector availability");
}

void TestCompletedSectorsAndLapAreCommitted() {
    SectorTiming timing;
    timing.Configure(true);

    timing.Advance(2.0f, NoProgressEvent());
    const SectorTimingUpdate first = timing.Advance(1.0f, SectorBoundary(0));
    Expect(first.sectorCompleted && first.completedSectorIndex == 0 &&
               NearlyEqual(first.completedSectorTime, 3.0f) &&
               timing.Snapshot().currentSectorIndex == 1 &&
               NearlyEqual(timing.Snapshot().currentSectorTime, 0.0f),
           "the first exact boundary commits sector one and starts sector two");

    const SectorTimingUpdate second = timing.Advance(4.0f, SectorBoundary(1));
    Expect(second.sectorCompleted && second.completedSectorIndex == 1 &&
               NearlyEqual(second.completedSectorTime, 4.0f) &&
               timing.Snapshot().currentSectorIndex == 2,
           "the second exact boundary commits sector two and starts sector three");

    const SectorTimingUpdate finish = timing.Advance(5.0f, LapFinish());
    const RaceTimingSnapshot& completed = timing.Snapshot();
    Expect(finish.sectorCompleted && finish.completedSectorIndex == 2 &&
               NearlyEqual(finish.completedSectorTime, 5.0f) && finish.lapCompleted &&
               NearlyEqual(finish.completedLapTime, 12.0f) && completed.completedLaps == 1 &&
               NearlyEqual(completed.lastLapTime, 12.0f) && NearlyEqual(completed.bestLapTime, 12.0f) &&
               NearlyEqual(completed.totalTime, 12.0f) &&
               NearlyEqual(completed.currentLapSectorTimes[0], 3.0f) &&
               NearlyEqual(completed.currentLapSectorTimes[1], 4.0f) &&
               NearlyEqual(completed.currentLapSectorTimes[2], 5.0f),
           "lap completion commits sector three before recording the complete lap");

    timing.Advance(10.0f, LapFinish());
    Expect(NearlyEqual(timing.Snapshot().totalTime, 12.0f) &&
               timing.Snapshot().completedLaps == 1,
           "a committed lap ignores duplicate finish updates until the next lap begins");
}

void TestNextLapPreservesReferencesAndImprovesThem() {
    SectorTiming timing;
    timing.Configure(true);
    timing.Advance(3.0f, SectorBoundary(0));
    timing.Advance(4.0f, SectorBoundary(1));
    timing.Advance(5.0f, LapFinish());
    timing.BeginNextLap();

    const RaceTimingSnapshot& next = timing.Snapshot();
    Expect(next.completedLaps == 1 && next.currentSectorIndex == 0 &&
               NearlyEqual(next.currentLapTime, 0.0f) && NearlyEqual(next.currentSectorTime, 0.0f) &&
               NearlyEqual(next.totalTime, 12.0f) && NearlyEqual(next.lastLapTime, 12.0f) &&
               NearlyEqual(next.bestLapTime, 12.0f) &&
               !next.currentLapSectorCompleted[0] && !next.currentLapSectorCompleted[1] &&
               !next.currentLapSectorCompleted[2],
           "beginning another lap resets only active split state and keeps session references");

    timing.Advance(2.0f, SectorBoundary(0));
    timing.Advance(3.0f, SectorBoundary(1));
    timing.Advance(4.0f, LapFinish());
    const RaceTimingSnapshot& improved = timing.Snapshot();
    Expect(improved.completedLaps == 2 && NearlyEqual(improved.lastLapTime, 9.0f) &&
               NearlyEqual(improved.bestLapTime, 9.0f) &&
               NearlyEqual(improved.bestSectorTimes[0], 2.0f) &&
               NearlyEqual(improved.bestSectorTimes[1], 3.0f) &&
               NearlyEqual(improved.bestSectorTimes[2], 4.0f) &&
               NearlyEqual(improved.totalTime, 21.0f),
           "later laps independently improve the coherent best lap and each best sector");
}

void TestLapTimingRemainsAvailableWithoutSectors() {
    SectorTiming timing;
    timing.Configure(false);
    timing.Advance(7.5f, SectorBoundary(0));
    const SectorTimingUpdate finish = timing.Advance(2.5f, LapFinish());
    const RaceTimingSnapshot& snapshot = timing.Snapshot();
    Expect(!snapshot.sectorsAvailable && !finish.sectorCompleted && finish.lapCompleted &&
               NearlyEqual(finish.completedLapTime, 10.0f) && snapshot.completedLaps == 1 &&
               NearlyEqual(snapshot.lastLapTime, 10.0f) && NearlyEqual(snapshot.bestLapTime, 10.0f) &&
               !snapshot.currentLapSectorCompleted[0] && !snapshot.currentLapSectorCompleted[1] &&
               !snapshot.currentLapSectorCompleted[2],
           "unsupported sector topology still retains authoritative lap and total timing");
}

} // namespace

int main() {
    TestInitialAndResetState();
    TestCompletedSectorsAndLapAreCommitted();
    TestNextLapPreservesReferencesAndImprovesThem();
    TestLapTimingRemainsAvailableWithoutSectors();
    if (failures == 0) std::cout << "Neon Racer sector-timing tests passed.\n";
    return failures == 0 ? 0 : 1;
}
