#pragma once

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
