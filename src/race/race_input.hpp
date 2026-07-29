#pragma once

// Device-independent controls consumed by race simulation. The current Raylib
// adapter fills this structure, while replay and AI drivers can provide it
// without depending on a physical input device.
struct RaceInput {
    float steering;
    float accelerate;
    float brake;
    float reverse;
};

RaceInput ReadRaceInput();
