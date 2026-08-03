#include "../../src/race/time_trial.hpp"

#include <cmath>
#include <iostream>

namespace {

int failures = 0;
const float kFixedStep = 1.0f / 120.0f;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

bool NearlyEqual(float first, float second) {
    return std::fabs(first - second) < 0.0001f;
}

RaceInput Input(float steering = 0.0f, float accelerate = 0.0f, float brake = 0.0f,
                float reverse = 0.0f, bool pausePressed = false, bool resetPressed = false) {
    return RaceInput{steering, accelerate, brake, reverse, pausePressed, resetPressed, false};
}

RaceCar ReplayCar(float positionX, float speed) {
    return RaceCar{RaceVector3{positionX, 0.16f, 0.0f}, RaceVector3{speed, 0.0f, 0.0f},
                   RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f, speed};
}

VerifiedGhostData VerifiedGhostFor(std::uint64_t layoutFingerprint) {
    VerifiedGhostData ghost;
    ghost.layoutFingerprint = layoutFingerprint;
    ghost.durationSeconds = 0.10f;
    ghost.samples.push_back(GhostSample{ReplayCar(3.0f, 3.0f), 0.0f});
    ghost.samples.push_back(GhostSample{ReplayCar(6.0f, 6.0f), 1.0f / 30.0f});
    return ghost;
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

void ExpectStartPose(const RaceCar& car, float expectedForwardX, float expectedHeading, const char* message) {
    Expect(NearlyEqual(car.position.x, 0.0f) && NearlyEqual(car.position.y, 0.16f) &&
               NearlyEqual(car.position.z, 0.0f) && NearlyEqual(car.velocity.x, 0.0f) &&
               NearlyEqual(car.velocity.y, 0.0f) && NearlyEqual(car.velocity.z, 0.0f) &&
               NearlyEqual(car.forward.x, expectedForwardX) && NearlyEqual(car.forward.y, 0.0f) &&
               NearlyEqual(car.forward.z, 0.0f) && NearlyEqual(car.up.x, 0.0f) &&
               NearlyEqual(car.up.y, 1.0f) && NearlyEqual(car.up.z, 0.0f) &&
               NearlyEqual(car.headingRadians, expectedHeading) && NearlyEqual(car.speed, 0.0f),
           message);
}

void TestSectorTimingStateMachine() {
    SectorTiming timing;
    timing.Configure(true);
    Expect(timing.Snapshot().sectorsAvailable && timing.Snapshot().completedLaps == 0 &&
               timing.Snapshot().currentSectorIndex == 0 &&
               NearlyEqual(timing.Snapshot().currentSectorTime, 0.0f) &&
               NearlyEqual(timing.Snapshot().currentLapTime, 0.0f) &&
               NearlyEqual(timing.Snapshot().totalTime, 0.0f),
           "configured sector timing starts at the first split with zeroed clocks");

    timing.Advance(2.0f, NoProgressEvent());
    const SectorTimingUpdate first = timing.Advance(1.0f, SectorBoundary(0));
    const SectorTimingUpdate second = timing.Advance(4.0f, SectorBoundary(1));
    const SectorTimingUpdate finish = timing.Advance(5.0f, LapFinish());
    Expect(first.sectorCompleted && first.completedSectorIndex == 0 &&
               NearlyEqual(first.completedSectorTime, 3.0f) &&
               second.sectorCompleted && second.completedSectorIndex == 1 &&
               NearlyEqual(second.completedSectorTime, 4.0f) &&
               finish.sectorCompleted && finish.completedSectorIndex == 2 &&
               NearlyEqual(finish.completedSectorTime, 5.0f) && finish.lapCompleted &&
               NearlyEqual(finish.completedLapTime, 12.0f) &&
               timing.Snapshot().completedLaps == 1 &&
               NearlyEqual(timing.Snapshot().lastLapTime, 12.0f) &&
               NearlyEqual(timing.Snapshot().bestLapTime, 12.0f) &&
               NearlyEqual(timing.Snapshot().currentLapSectorTimes[0], 3.0f) &&
               NearlyEqual(timing.Snapshot().currentLapSectorTimes[1], 4.0f) &&
               NearlyEqual(timing.Snapshot().currentLapSectorTimes[2], 5.0f),
           "exact tracker events commit all three sectors before the completed lap");

    timing.Advance(10.0f, LapFinish());
    Expect(NearlyEqual(timing.Snapshot().totalTime, 12.0f) &&
               timing.Snapshot().completedLaps == 1,
           "a committed lap ignores duplicate finish updates until the next lap begins");

    timing.BeginNextLap();
    Expect(timing.Snapshot().completedLaps == 1 &&
               timing.Snapshot().currentSectorIndex == 0 &&
               NearlyEqual(timing.Snapshot().currentLapTime, 0.0f) &&
               NearlyEqual(timing.Snapshot().currentSectorTime, 0.0f) &&
               NearlyEqual(timing.Snapshot().totalTime, 12.0f) &&
               NearlyEqual(timing.Snapshot().lastLapTime, 12.0f) &&
               NearlyEqual(timing.Snapshot().bestLapTime, 12.0f) &&
               !timing.Snapshot().currentLapSectorCompleted[0] &&
               !timing.Snapshot().currentLapSectorCompleted[1] &&
               !timing.Snapshot().currentLapSectorCompleted[2],
           "beginning another lap resets active splits while retaining session references");

    timing.Advance(2.0f, SectorBoundary(0));
    timing.Advance(3.0f, SectorBoundary(1));
    timing.Advance(4.0f, LapFinish());
    Expect(timing.Snapshot().completedLaps == 2 &&
               NearlyEqual(timing.Snapshot().lastLapTime, 9.0f) &&
               NearlyEqual(timing.Snapshot().bestLapTime, 9.0f) &&
               NearlyEqual(timing.Snapshot().bestSectorTimes[0], 2.0f) &&
               NearlyEqual(timing.Snapshot().bestSectorTimes[1], 3.0f) &&
               NearlyEqual(timing.Snapshot().bestSectorTimes[2], 4.0f) &&
               NearlyEqual(timing.Snapshot().totalTime, 21.0f),
           "later laps independently improve the coherent best lap and best sectors");

    timing.ResetAttempt();
    Expect(timing.Snapshot().sectorsAvailable && timing.Snapshot().completedLaps == 0 &&
               NearlyEqual(timing.Snapshot().bestLapTime, 0.0f) &&
               NearlyEqual(timing.Snapshot().bestSectorTimes[0], 0.0f) &&
               NearlyEqual(timing.Snapshot().totalTime, 0.0f),
           "attempt reset clears active and session timing while retaining sector availability");

    timing.Configure(false);
    timing.Advance(7.5f, SectorBoundary(0));
    const SectorTimingUpdate noSectorFinish = timing.Advance(2.5f, LapFinish());
    Expect(!timing.Snapshot().sectorsAvailable && !noSectorFinish.sectorCompleted &&
               noSectorFinish.lapCompleted && NearlyEqual(noSectorFinish.completedLapTime, 10.0f) &&
               timing.Snapshot().completedLaps == 1 &&
               NearlyEqual(timing.Snapshot().lastLapTime, 10.0f) &&
               !timing.Snapshot().currentLapSectorCompleted[0] &&
               !timing.Snapshot().currentLapSectorCompleted[1] &&
               !timing.Snapshot().currentLapSectorCompleted[2],
           "lap timing remains authoritative when automatic sector topology is unavailable");
}

// TimeTrial stores a pointer to its track, so every test keeps its layout alive
// for the entire trial rather than constructing it in a temporary helper.
void TestStartAndResetPose() {
    Track forwardTrack = Track::CreateSampleCircuit();
    TimeTrial forwardTrial;
    forwardTrial.Start(forwardTrack);
    ExpectStartPose(forwardTrial.Car(), 1.0f, 0.0f,
                    "a forward start uses the Raylib-free car contract's canonical pose");
    Expect(forwardTrial.Timing().sectorsAvailable && forwardTrial.CurrentLap() == 1 &&
               forwardTrial.CurrentSector() == 1 && NearlyEqual(forwardTrial.LastLapTime(), 0.0f),
           "a race-ready circuit exposes a sector-one timing snapshot at the start");
    forwardTrial.Update(kFixedStep, Input(0.0f, 1.0f));
    forwardTrial.Reset();
    ExpectStartPose(forwardTrial.Car(), 1.0f, 0.0f,
                    "reset restores the exact forward start pose without a rendering dependency");
    Expect(NearlyEqual(forwardTrial.CurrentSectorTime(), 0.0f) &&
               NearlyEqual(forwardTrial.CurrentLapTime(), 0.0f) &&
               NearlyEqual(forwardTrial.TotalTime(), 0.0f) &&
               NearlyEqual(forwardTrial.BestLapTime(), 0.0f),
           "time-trial reset clears the complete timing attempt state");

    Track reverseTrack = Track::CreateSampleCircuit();
    reverseTrack.SetStartFinish(reverseTrack.StartFinishPieceId(), RaceDirection::Reverse);
    TimeTrial reverseTrial;
    reverseTrial.Start(reverseTrack);
    ExpectStartPose(reverseTrial.Car(), -1.0f, 3.14159265358979323846f,
                    "a reverse start retains the canonical pi heading without Raylib constants");
}

void TestInjectedDriveInput() {
    const Track track = Track::CreateSampleCircuit();
    TimeTrial coasting;
    TimeTrial accelerating;
    coasting.Start(track);
    accelerating.Start(track);
    for (int step = 0; step < 24; ++step) {
        coasting.Update(kFixedStep, Input());
        accelerating.Update(kFixedStep, Input(0.0f, 1.0f));
    }
    Expect(coasting.IsReady() && accelerating.IsReady(), "sample circuit starts a ready time trial");
    Expect(accelerating.Car().speed > coasting.Car().speed + 0.1f,
           "injected acceleration reaches vehicle dynamics without device polling");
    Expect(accelerating.TotalTime() > 0.0f &&
               NearlyEqual(accelerating.TotalTime(), accelerating.CurrentLapTime()) &&
               NearlyEqual(accelerating.CurrentLapTime(), accelerating.CurrentSectorTime()),
           "a first-sector fixed-step frame advances total, lap, and sector clocks together");
}

void TestHeldInputFeedsEveryFixedStep() {
    const Track track = Track::CreateSampleCircuit();
    TimeTrial stepped;
    TimeTrial batched;
    stepped.Start(track);
    batched.Start(track);

    stepped.Update(kFixedStep, Input(0.0f, 1.0f));
    stepped.Update(kFixedStep, Input(0.0f, 1.0f));
    batched.Update(kFixedStep * 2.0f, Input(0.0f, 1.0f));

    Expect(NearlyEqual(batched.TotalTime(), stepped.TotalTime()) &&
               NearlyEqual(batched.CurrentLapTime(), stepped.CurrentLapTime()) &&
               NearlyEqual(batched.CurrentSectorTime(), stepped.CurrentSectorTime()) &&
               NearlyEqual(batched.Car().speed, stepped.Car().speed) &&
               NearlyEqual(batched.Car().position.x, stepped.Car().position.x),
           "a held input snapshot is reused by every fixed step in its frame");
}

void TestPauseAndResetActions() {
    const Track track = Track::CreateSampleCircuit();
    TimeTrial trial;
    trial.Start(track);
    for (int step = 0; step < 4; ++step) trial.Update(kFixedStep, Input(0.0f, 1.0f));
    const float beforePause = trial.TotalTime();
    const float sectorBeforePause = trial.CurrentSectorTime();
    const float lapBeforePause = trial.CurrentLapTime();

    trial.Update(kFixedStep * 2.0f, Input(0.0f, 0.0f, 0.0f, 0.0f, true));
    Expect(trial.IsPaused() && NearlyEqual(trial.TotalTime(), beforePause) &&
               NearlyEqual(trial.CurrentLapTime(), lapBeforePause) &&
               NearlyEqual(trial.CurrentSectorTime(), sectorBeforePause),
           "a pause edge freezes total, lap, and sector clocks before fixed updates");
    trial.Update(kFixedStep, Input());
    Expect(NearlyEqual(trial.TotalTime(), beforePause) &&
               NearlyEqual(trial.CurrentSectorTime(), sectorBeforePause),
           "a paused trial ignores held driving input and timing advancement");

    trial.Update(kFixedStep, Input(0.0f, 0.0f, 0.0f, 0.0f, true, true));
    Expect(!trial.IsPaused() && trial.TotalTime() > 0.0f && trial.TotalTime() < beforePause &&
               NearlyEqual(trial.TotalTime(), trial.CurrentLapTime()) &&
               NearlyEqual(trial.CurrentLapTime(), trial.CurrentSectorTime()),
           "simultaneous pause and reset preserves reset-after-pause timing order");
}

void TestInvalidTrackIgnoresInjectedInput() {
    Track incomplete;
    incomplete.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    TimeTrial trial;
    trial.Start(incomplete);
    trial.Update(kFixedStep, Input(0.0f, 1.0f, 0.0f, 0.0f, true, true));
    Expect(!trial.IsReady() && !trial.Timing().sectorsAvailable &&
               NearlyEqual(trial.TotalTime(), 0.0f) && NearlyEqual(trial.CurrentLapTime(), 0.0f),
           "an unready trial exposes unavailable sectors and ignores input timing");
}

void TestVerifiedGhostTransferForActiveLayout() {
    Track track = Track::CreateSampleCircuit();
    TimeTrial trial;
    trial.Start(track);
    const std::uint64_t fingerprint = trial.ActiveLayoutFingerprint();

    Track equivalentTrack = Track::CreateSampleCircuit();
    TimeTrial equivalentTrial;
    equivalentTrial.Start(equivalentTrack);
    Expect(fingerprint != 0u && fingerprint == equivalentTrial.ActiveLayoutFingerprint(),
           "equivalent frozen layouts produce the same durable replay fingerprint");

    const VerifiedGhostData imported = VerifiedGhostFor(fingerprint);
    Expect(trial.ImportVerifiedGhost(imported) && trial.HasVerifiedGhost() &&
               trial.Verification().hasSavedGhost && trial.Verification().isVerifiedForPlayableExport &&
               trial.Verification().verifiedLayoutRevision == track.LayoutRevision() &&
               trial.Verification().verifiedLayoutFingerprint == fingerprint &&
               NearlyEqual(trial.GhostCar().position.x, 3.0f),
           "a matching replay import derives verification for the active layout");

    VerifiedGhostData exported;
    Expect(trial.ExportVerifiedGhost(exported) && exported.layoutFingerprint == fingerprint &&
               exported.samples.size() == imported.samples.size() &&
               NearlyEqual(exported.durationSeconds, imported.durationSeconds) &&
               NearlyEqual(exported.samples[1].car.position.x, imported.samples[1].car.position.x),
           "a verified active replay exports its samples, duration, and durable layout fingerprint");

    VerifiedGhostData wrongLayout = imported;
    wrongLayout.layoutFingerprint ^= 1u;
    const std::uint32_t verifiedRevision = trial.Verification().verifiedLayoutRevision;
    Expect(!trial.ImportVerifiedGhost(wrongLayout) && NearlyEqual(trial.GhostCar().position.x, 3.0f) &&
               trial.Verification().verifiedLayoutRevision == verifiedRevision,
           "a mismatched replay import leaves the active verified ghost unchanged");

    VerifiedGhostData malformed = imported;
    malformed.samples[1].time = 0.0f;
    Expect(!trial.ImportVerifiedGhost(malformed) && NearlyEqual(trial.GhostCar().position.x, 3.0f) &&
               trial.Verification().verifiedLayoutRevision == verifiedRevision,
           "a malformed matching-layout replay leaves active verification unchanged");

    Track sameRevisionDifferentLayout = track;
    TrackPiece changedPiece = sameRevisionDifferentLayout.Pieces().front();
    changedPiece.material = SurfaceMaterial::Slippery;
    Expect(sameRevisionDifferentLayout.ReplacePiece(changedPiece) &&
               sameRevisionDifferentLayout.LayoutRevision() == track.LayoutRevision() + 1,
           "same-revision replay invalidation setup changes one persisted layout field");
    Track alsoChangedLayout = track;
    changedPiece = alsoChangedLayout.Pieces().front();
    changedPiece.material = SurfaceMaterial::HighResistance;
    Expect(alsoChangedLayout.ReplacePiece(changedPiece) &&
               alsoChangedLayout.LayoutRevision() == sameRevisionDifferentLayout.LayoutRevision(),
           "two distinct layouts can share the same in-memory revision");

    TimeTrial collisionTrial;
    collisionTrial.Start(sameRevisionDifferentLayout);
    const VerifiedGhostData collisionGhost = VerifiedGhostFor(collisionTrial.ActiveLayoutFingerprint());
    Expect(collisionTrial.ImportVerifiedGhost(collisionGhost),
           "same-revision replay invalidation setup imports a verified ghost");
    collisionTrial.Start(alsoChangedLayout);
    Expect(!collisionTrial.HasVerifiedGhost() && !collisionTrial.ExportVerifiedGhost(exported),
           "a distinct layout with the same revision clears its prior verified replay by fingerprint");

    equivalentTrack.SetStartFinish(equivalentTrack.StartFinishPieceId(), RaceDirection::Reverse);
    equivalentTrial.Start(equivalentTrack);
    Expect(fingerprint != equivalentTrial.ActiveLayoutFingerprint(),
           "race direction participates in the durable replay fingerprint");

    track.SetStartFinish(track.StartFinishPieceId(), RaceDirection::Reverse);
    Expect(!trial.ExportVerifiedGhost(exported),
           "a verified replay cannot be exported after its active layout revision changes");
}

} // namespace

int main() {
    TestSectorTimingStateMachine();
    TestStartAndResetPose();
    TestInjectedDriveInput();
    TestHeldInputFeedsEveryFixedStep();
    TestPauseAndResetActions();
    TestInvalidTrackIgnoresInjectedInput();
    TestVerifiedGhostTransferForActiveLayout();
    if (failures == 0) std::cout << "Neon Racer time-trial and sector-timing tests passed.\n";
    return failures == 0 ? 0 : 1;
}
