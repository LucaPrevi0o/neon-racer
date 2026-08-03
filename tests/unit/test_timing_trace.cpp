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

TrackPositionTracker::UpdateResult Progress(
    int sectorIndex, float sectorProgress, float lapProgress,
    std::uint32_t routeVariantId) {
    TrackPositionTracker::UpdateResult result;
    result.progress.configured = true;
    result.progress.sectorsReady = true;
    result.progress.sectorIndex = sectorIndex;
    result.progress.routeVariantId = routeVariantId;
    result.progress.sectorProgress = sectorProgress;
    result.progress.lapProgress = lapProgress;
    result.progress.hasSectorProgress = true;
    result.progress.hasLapProgress = true;
    return result;
}

TrackPositionTracker::UpdateResult Boundary(
    int completedSectorIndex, int nextSectorIndex,
    float lapProgress, std::uint32_t routeVariantId) {
    TrackPositionTracker::UpdateResult result =
        Progress(nextSectorIndex, 0.0f, lapProgress, routeVariantId);
    result.sectorBoundaryCrossed = true;
    result.completedSectorIndex = completedSectorIndex;
    return result;
}

TrackPositionTracker::UpdateResult Finish(std::uint32_t routeVariantId) {
    TrackPositionTracker::UpdateResult result =
        Progress(2, 1.0f, 1.0f, routeVariantId);
    result.lapCompleted = true;
    return result;
}

void TestMonotonicTraceAndLookupQuality() {
    TimingTrace trace;
    trace.Begin();
    Expect(trace.Append(0.25f, 2.0f, 0u),
           "a trace accepts its first advancing progress point");
    Expect(!trace.Append(0.25f, 3.0f, 0u) &&
               !trace.Append(0.20f, 4.0f, 0u),
           "waiting and backwards progress do not rewrite the first arrival time");
    Expect(trace.Append(0.50f, 4.0f, 7u),
           "a trace records the exact route once a branch is resolved");
    trace.Complete(8.0f, 7u);

    const TimingTraceLookup exact = trace.Lookup(0.75f, 7u, true);
    const TimingTraceLookup normalized = trace.Lookup(0.75f, 9u, true);
    const TimingTraceLookup unavailable = trace.Lookup(0.75f, 9u, false);
    Expect(trace.IsComplete() && trace.Points().size() == 4u &&
               exact.available && NearlyEqual(exact.elapsedTime, 6.0f) &&
               exact.quality == TimingComparisonQuality::ExactRoute,
           "a complete trace interpolates an exact-route elapsed time");
    Expect(normalized.available && NearlyEqual(normalized.elapsedTime, 6.0f) &&
               normalized.quality == TimingComparisonQuality::NormalizedProgress,
           "a different branch can request the explicit normalized fallback");
    Expect(!unavailable.available &&
               unavailable.quality == TimingComparisonQuality::Unavailable,
           "a route mismatch stays unavailable when fallback is disabled");
}

void CompleteFirstLap(SectorTiming& timing, std::uint32_t routeVariantId) {
    timing.Advance(1.0f, Progress(0, 0.25f, 0.10f, 0u));
    timing.Advance(1.0f, Progress(0, 0.75f, 0.25f, routeVariantId));
    timing.Advance(1.0f, Boundary(0, 1, 0.34f, routeVariantId));
    timing.Advance(1.0f, Progress(1, 0.50f, 0.50f, routeVariantId));
    timing.Advance(1.0f, Boundary(1, 2, 0.67f, routeVariantId));
    timing.Advance(2.0f, Progress(2, 0.50f, 0.82f, routeVariantId));
    timing.Advance(2.0f, Finish(routeVariantId));
}

void TestBestSectorAndCoherentLapProfiles() {
    SectorTiming timing;
    timing.Configure(true);
    CompleteFirstLap(timing, 7u);

    const RaceTimingReferences& firstReferences = timing.References();
    Expect(firstReferences.bestLap.valid &&
               NearlyEqual(firstReferences.bestLap.lapTime, 9.0f) &&
               firstReferences.bestLap.routeVariantId == 7u &&
               firstReferences.bestLap.trace.IsComplete() &&
               NearlyEqual(firstReferences.bestLap.sectorTimes[0], 3.0f) &&
               NearlyEqual(firstReferences.bestLap.sectorTimes[1], 2.0f) &&
               NearlyEqual(firstReferences.bestLap.sectorTimes[2], 4.0f),
           "the first completed lap becomes one coherent lap reference");
    Expect(firstReferences.bestSectors[0].valid &&
               firstReferences.bestSectors[1].valid &&
               firstReferences.bestSectors[2].valid &&
               NearlyEqual(firstReferences.bestSectors[0].sectorTime, 3.0f) &&
               NearlyEqual(firstReferences.bestSectors[1].sectorTime, 2.0f) &&
               NearlyEqual(firstReferences.bestSectors[2].sectorTime, 4.0f) &&
               firstReferences.bestSectors[0].trace.IsComplete(),
           "each completed sector retains an independent fastest trace");

    timing.BeginNextLap();
    timing.Advance(4.0f, Boundary(0, 1, 0.34f, 8u));
    timing.Advance(1.0f, Boundary(1, 2, 0.67f, 8u));
    timing.Advance(5.0f, Finish(8u));

    const RaceTimingReferences& mixedReferences = timing.References();
    Expect(NearlyEqual(mixedReferences.bestSectors[0].sectorTime, 3.0f) &&
               NearlyEqual(mixedReferences.bestSectors[1].sectorTime, 1.0f) &&
               NearlyEqual(mixedReferences.bestSectors[2].sectorTime, 4.0f),
           "individual sectors promote without requiring the whole lap to improve");
    Expect(NearlyEqual(mixedReferences.bestLap.lapTime, 9.0f) &&
               NearlyEqual(mixedReferences.bestLap.sectorTimes[0], 3.0f) &&
               NearlyEqual(mixedReferences.bestLap.sectorTimes[1], 2.0f) &&
               NearlyEqual(mixedReferences.bestLap.sectorTimes[2], 4.0f),
           "the best lap is never synthesized from unrelated fastest sectors");

    timing.BeginNextLap();
    timing.Advance(2.0f, Boundary(0, 1, 0.34f, 9u));
    timing.Advance(2.0f, Boundary(1, 2, 0.67f, 9u));
    timing.Advance(3.0f, Finish(9u));

    const RaceTimingReferences& finalReferences = timing.References();
    Expect(NearlyEqual(finalReferences.bestLap.lapTime, 7.0f) &&
               finalReferences.bestLap.routeVariantId == 9u &&
               NearlyEqual(finalReferences.bestLap.sectorTimes[0], 2.0f) &&
               NearlyEqual(finalReferences.bestLap.sectorTimes[1], 2.0f) &&
               NearlyEqual(finalReferences.bestLap.sectorTimes[2], 3.0f) &&
               NearlyEqual(finalReferences.bestSectors[1].sectorTime, 1.0f),
           "a faster complete lap replaces the coherent reference without overwriting a faster isolated sector");

    timing.ResetAttempt();
    Expect(!timing.References().bestLap.valid &&
               !timing.References().bestSectors[0].valid &&
               timing.CurrentLapTrace().Points().size() == 1u &&
               timing.CurrentSectorTrace().Points().size() == 1u,
           "attempt reset clears reference profiles and restarts active traces at zero");
}

} // namespace

int main() {
    TestMonotonicTraceAndLookupQuality();
    TestBestSectorAndCoherentLapProfiles();
    if (failures == 0) {
        std::cout << "Neon Racer timing trace and reference-profile tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
