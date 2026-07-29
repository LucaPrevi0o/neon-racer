#pragma once

// Raylib-free race-dynamics helpers. Keeping the input curve here makes the
// controller policy testable without coupling the unit tests to a window or a
// physical gamepad.
namespace RacePhysics {

// Keyboard and face buttons cannot express a partial brake press. They use a
// controllable fixed equivalent while analogue triggers retain their full
// 0..1 range.
constexpr float kDigitalBrakeAxis = 0.45f;

// Returns the velocity-damping coefficient for a brake axis and the current
// surface grip. Brake input is clamped to [0, 1]; grip is clamped only at zero
// so high-resistance surfaces can still strengthen braking.
float BrakeDamping(float brakeAxis, float grip);

} // namespace RacePhysics
