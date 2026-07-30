#include "../../src/race/ghost_replay.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

const float kSampleInterval = 1.0f / 30.0f;

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

bool NearlyEqual(float first, float second) { return std::fabs(first - second) < 0.0001f; }

RaceCar CarAt(float positionX, float velocityX, RaceVector3 forward, RaceVector3 up, float heading, float speed) {
    return RaceCar{RaceVector3{positionX, 2.0f, -3.0f}, RaceVector3{velocityX, 0.0f, 0.0f}, forward, up, heading, speed};
}

void RecordTwoSamples(GhostReplay& replay, const RaceCar& first, const RaceCar& second) {
    replay.Capture(first, 0.0f);
    replay.Capture(second, kSampleInterval);
}

void TestSamplingCadenceAndInterpolation() {
    const RaceCar first = CarAt(0.0f, 0.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                0.0f, 10.0f);
    const RaceCar skipped = CarAt(100.0f, 100.0f, RaceVector3{-1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, -1.0f, 0.0f},
                                  10.0f, 100.0f);
    const RaceCar second = CarAt(12.0f, 6.0f, RaceVector3{0.0f, 0.0f, 1.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                 2.0f, 30.0f);
    const RaceCar fallback = CarAt(-5.0f, -5.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                   -5.0f, -5.0f);

    GhostReplay replay;
    replay.Capture(first, 0.0f);
    replay.Capture(skipped, kSampleInterval * 0.5f);
    replay.Capture(second, kSampleInterval);
    Expect(replay.PromoteCandidateIfFaster(0.10f), "a non-empty candidate can become the first verified replay");

    const RaceCar middle = replay.SampleAt(kSampleInterval * 0.5f, fallback);
    Expect(NearlyEqual(middle.position.x, 6.0f) && NearlyEqual(middle.velocity.x, 3.0f) &&
               NearlyEqual(middle.headingRadians, 1.0f) && NearlyEqual(middle.speed, 20.0f),
           "playback interpolates scalar and vector motion fields between cadence samples");
    const float inverseRootTwo = 0.70710678f;
    Expect(NearlyEqual(middle.forward.x, inverseRootTwo) && NearlyEqual(middle.forward.z, inverseRootTwo) &&
               NearlyEqual(middle.up.y, 1.0f),
           "playback normalizes interpolated orientation vectors");

    Expect(NearlyEqual(replay.SampleAt(kSampleInterval * 2.0f, fallback).position.x, second.position.x),
           "playback holds the final sample until the run duration boundary");
    Expect(NearlyEqual(replay.SampleAt(0.10f, fallback).position.x, first.position.x),
           "playback loops to the initial sample at the verified duration");
}

void TestPromotionResetAndClearing() {
    const RaceCar fallback = CarAt(-1.0f, -1.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                   -1.0f, -1.0f);
    const RaceCar first = CarAt(1.0f, 1.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                                1.0f);
    const RaceCar second = CarAt(2.0f, 2.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                                 2.0f);

    GhostReplay replay;
    RecordTwoSamples(replay, first, second);
    Expect(replay.PromoteCandidateIfFaster(0.20f) && replay.HasVerified(),
           "the first completed candidate establishes a verified replay");

    replay.ResetCandidate();
    RecordTwoSamples(replay, CarAt(100.0f, 100.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                                        100.0f),
                     CarAt(200.0f, 200.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                           200.0f));
    Expect(!replay.PromoteCandidateIfFaster(0.20f), "an equal-duration candidate cannot replace a verified replay");
    Expect(NearlyEqual(replay.SampleAt(0.05f, fallback).position.x, second.position.x),
           "resetting or rejecting a candidate preserves the verified replay");

    replay.ResetCandidate();
    const RaceCar fasterFirst = CarAt(3.0f, 3.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                       0.0f, 3.0f);
    const RaceCar fasterSecond = CarAt(4.0f, 4.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                        0.0f, 4.0f);
    RecordTwoSamples(replay, fasterFirst, fasterSecond);
    Expect(replay.PromoteCandidateIfFaster(0.15f), "a strictly faster candidate replaces the verified replay");
    Expect(NearlyEqual(replay.SampleAt(0.05f, fallback).position.x, fasterSecond.position.x),
           "a faster candidate becomes the replay used for future samples");

    replay.ClearVerified();
    Expect(!replay.HasVerified() && NearlyEqual(replay.SampleAt(0.05f, fallback).position.x, fallback.position.x),
           "clearing verified data restores fallback playback");
}

void TestVerifiedReplayTransfer() {
    const RaceCar fallback = CarAt(-8.0f, -8.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                   -8.0f, -8.0f);
    const RaceCar first = CarAt(5.0f, 1.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                                1.0f);
    const RaceCar second = CarAt(7.0f, 2.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                                 2.0f);

    GhostReplay source;
    RecordTwoSamples(source, first, second);
    Expect(source.PromoteCandidateIfFaster(0.20f), "a replay can be prepared for value transfer");

    VerifiedGhostData exported;
    exported.layoutFingerprint = 123u;
    Expect(source.ExportVerified(exported) && exported.samples.size() == 2 &&
               NearlyEqual(exported.durationSeconds, 0.20f) && exported.layoutFingerprint == 0u,
           "replay export copies only verified replay data and leaves layout binding to its owner");
    Expect(GhostReplay::IsValidVerifiedData(exported),
           "the public replay validator accepts data produced by a verified replay");

    GhostReplay imported;
    Expect(imported.ImportVerified(exported) && imported.HasVerified() &&
               NearlyEqual(imported.SampleAt(0.05f, fallback).position.x, second.position.x),
           "a valid transfer imports the replay used for playback");

    GhostReplay preserved;
    RecordTwoSamples(preserved,
                     CarAt(30.0f, 30.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                           30.0f),
                     CarAt(40.0f, 40.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f,
                           40.0f));
    Expect(preserved.PromoteCandidateIfFaster(0.30f), "a destination replay exists before an invalid import");

    VerifiedGhostData invalid = exported;
    invalid.samples[1].time = 0.0f;
    Expect(!GhostReplay::IsValidVerifiedData(invalid) && !preserved.ImportVerified(invalid) &&
               NearlyEqual(preserved.SampleAt(0.05f, fallback).position.x, 40.0f),
           "an invalid import is rejected without replacing the verified replay");

    GhostReplay empty;
    VerifiedGhostData untouched;
    untouched.durationSeconds = 9.0f;
    untouched.layoutFingerprint = 77u;
    Expect(!empty.ExportVerified(untouched) && NearlyEqual(untouched.durationSeconds, 9.0f) &&
               untouched.layoutFingerprint == 77u,
           "exporting an absent replay leaves the caller transfer object unchanged");
}

void TestEmptyCandidateFallsBack() {
    const RaceCar fallback = CarAt(-3.0f, -3.0f, RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f},
                                   -3.0f, -3.0f);
    GhostReplay replay;
    Expect(!replay.PromoteCandidateIfFaster(0.10f), "an empty recording cannot become a verified replay");
    Expect(NearlyEqual(replay.SampleAt(0.0f, fallback).position.x, fallback.position.x),
           "an empty replay always returns the caller fallback");
}

void TestVerifiedReplaySafetyBounds() {
    VerifiedGhostData data;
    data.samples.push_back(GhostSample{CarAt(0.0f, 0.0f, RaceVector3{1.0f, 0.0f, 0.0f},
                                             RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f, 0.0f), 0.0f});
    data.samples.push_back(GhostSample{CarAt(1.0f, 1.0f, RaceVector3{1.0f, 0.0f, 0.0f},
                                             RaceVector3{0.0f, 1.0f, 0.0f}, 0.1f, 1.0f), kSampleInterval});
    data.durationSeconds = 0.10f;
    Expect(GhostReplay::IsValidVerifiedData(data), "a normal bounded replay remains valid");

    VerifiedGhostData extremePosition = data;
    extremePosition.samples[1].car.position.x = std::numeric_limits<float>::max();
    Expect(!GhostReplay::IsValidVerifiedData(extremePosition),
           "finite-but-extreme replay positions are rejected before interpolation can overflow");

    VerifiedGhostData extremeDuration = data;
    extremeDuration.durationSeconds = GhostReplayLimits::kMaximumVerifiedDurationSeconds + 1.0f;
    Expect(!GhostReplay::IsValidVerifiedData(extremeDuration),
           "replay duration is bounded for persistence and playback");
}

} // namespace

int main() {
    TestSamplingCadenceAndInterpolation();
    TestPromotionResetAndClearing();
    TestVerifiedReplayTransfer();
    TestEmptyCandidateFallsBack();
    TestVerifiedReplaySafetyBounds();
    if (failures == 0) std::cout << "Neon Racer ghost replay tests passed.\n";
    return failures == 0 ? 0 : 1;
}
