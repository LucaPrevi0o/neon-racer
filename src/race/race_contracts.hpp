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
    RaceVector3 position;
    RaceVector3 velocity;
    RaceVector3 forward;
    RaceVector3 up;
    float headingRadians;
    float speed;
};

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
