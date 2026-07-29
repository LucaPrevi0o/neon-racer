#include "../../src/race/race.hpp"

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

// TimeTrial stores a pointer to its track, so every test keeps its layout alive
// for the entire trial rather than constructing it in a temporary helper.
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

} // namespace

int main() {
    TestInjectedDriveInput();
    TestHeldInputFeedsEveryFixedStep();
    TestPauseAndResetActions();
    TestInvalidTrackIgnoresInjectedInput();
    if (failures == 0) std::cout << "Neon Racer time-trial tests passed.\n";
    return failures == 0 ? 0 : 1;
}
