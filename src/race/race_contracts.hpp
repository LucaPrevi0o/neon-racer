#pragma once

#include <cstdint>
#include <vector>

// Core race values deliberately use no presentation-library types. Rendering
// adapts these fields to Raylib only at its drawing boundary.
struct RaceVector3 {
    float x;
    float y;
    float z;
};

struct RaceCar {
    // This is the suspension/contact reference used by vehicle dynamics, not
    // the center of the rendered chassis.  It can settle very close to the
    // road surface while the visual body remains above it.
    RaceVector3 position;
    RaceVector3 velocity;
    RaceVector3 forward;
    RaceVector3 up;
    float headingRadians;
    float speed;
};

// The chassis mesh is 0.28 units tall, so keep its center above the contact
// reference even when suspension compression brings that reference to the
// road. Applying the lift along `up` keeps it correct on banks and twists.
const float kRaceCarVisualBodyLift = 0.22f;

inline RaceVector3 RaceCarVisualCenter(const RaceCar& car) {
    return RaceVector3{car.position.x + car.up.x * kRaceCarVisualBodyLift,
                       car.position.y + car.up.y * kRaceCarVisualBodyLift,
                       car.position.z + car.up.z * kRaceCarVisualBodyLift};
}

struct GhostSample {
    RaceCar car;
    float time;
};

// A value-only snapshot of a verified replay.  GhostReplay owns the samples
// and duration; TimeTrial supplies the layout fingerprint when transferring a
// replay across the race/persistence boundary.  Candidate-recording state is
// intentionally excluded.
struct VerifiedGhostData {
    std::vector<GhostSample> samples;
    float durationSeconds;
    std::uint64_t layoutFingerprint;

    VerifiedGhostData() : durationSeconds(0.0f), layoutFingerprint(0) {}
};
