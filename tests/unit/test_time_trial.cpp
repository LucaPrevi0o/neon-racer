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
    return RaceInput{steering, accelerate, brake, reverse, pausePressed, resetPressed};
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

// TimeTrial stores a pointer to its track, so every test keeps its layout alive
// for the entire trial rather than constructing it in a temporary helper.
void TestStartAndResetPose() {
    Track forwardTrack = Track::CreateSampleCircuit();
    TimeTrial forwardTrial;
    forwardTrial.Start(forwardTrack);
    ExpectStartPose(forwardTrial.Car(), 1.0f, 0.0f,
                    "a forward start uses the Raylib-free car contract's canonical pose");
    forwardTrial.Update(kFixedStep, Input(0.0f, 1.0f));
    forwardTrial.Reset();
    ExpectStartPose(forwardTrial.Car(), 1.0f, 0.0f,
                    "reset restores the exact forward start pose without a rendering dependency");

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
    Expect(accelerating.TotalTime() > 0.0f,
           "a fixed-step frame with injected input advances the time trial");
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

    trial.Update(kFixedStep * 2.0f, Input(0.0f, 0.0f, 0.0f, 0.0f, true));
    Expect(trial.IsPaused() && NearlyEqual(trial.TotalTime(), beforePause),
           "a pause edge is processed once before every fixed step in its frame");
    trial.Update(kFixedStep, Input());
    Expect(NearlyEqual(trial.TotalTime(), beforePause), "a paused trial ignores held driving input");

    trial.Update(kFixedStep, Input(0.0f, 0.0f, 0.0f, 0.0f, true, true));
    Expect(!trial.IsPaused() && trial.TotalTime() > 0.0f && trial.TotalTime() < beforePause,
           "simultaneous pause and reset preserves reset-after-pause ordering");
}

void TestInvalidTrackIgnoresInjectedInput() {
    Track incomplete;
    incomplete.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    TimeTrial trial;
    trial.Start(incomplete);
    trial.Update(kFixedStep, Input(0.0f, 1.0f, 0.0f, 0.0f, true, true));
    Expect(!trial.IsReady() && NearlyEqual(trial.TotalTime(), 0.0f),
           "an unready trial ignores injected actions and driving axes");
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
    TestStartAndResetPose();
    TestInjectedDriveInput();
    TestHeldInputFeedsEveryFixedStep();
    TestPauseAndResetActions();
    TestInvalidTrackIgnoresInjectedInput();
    TestVerifiedGhostTransferForActiveLayout();
    if (failures == 0) std::cout << "Neon Racer time-trial tests passed.\n";
    return failures == 0 ? 0 : 1;
}
