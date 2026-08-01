#pragma once

// Device-independent controls consumed by the time trial. Driving axes remain
// held for one application frame; action flags represent one-frame presses.
// Raylib, replay, AI, and tests can all produce the same snapshot without the
// simulation knowing where it came from.
struct RaceInput {
    float steering;
    float accelerate;
    float brake;
    float reverse;
    bool pausePressed;
    bool resetPressed;
    bool recoverPressed;
};
