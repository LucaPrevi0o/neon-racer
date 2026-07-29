#include "race_input.hpp"
#include "race_physics.hpp"

#include <algorithm>
#include <cmath>
#include <raylib.h>

RaceInput ReadRaceInput() {
    RaceInput input = {0.0f, 0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) input.steering -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.steering += 1.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) input.accelerate = 1.0f;
    // A key has no analogue travel. Keep it controllable while preserving the
    // full brake range for a real trigger below.
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) input.brake = RacePhysics::kDigitalBrakeAxis;
    if (IsKeyDown(KEY_X)) input.reverse = 1.0f;

    if (!IsGamepadAvailable(0)) return input;
    const float stick = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    const float rightTrigger = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER);
    const float leftTrigger = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER);
    if (std::fabs(stick) > 0.12f) input.steering = stick;
    if (rightTrigger > 0.05f) input.accelerate = rightTrigger;
    if (leftTrigger > 0.05f) input.brake = std::max(input.brake, leftTrigger);
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) input.accelerate = 1.0f;
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        input.brake = std::max(input.brake, RacePhysics::kDigitalBrakeAxis);
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) input.reverse = 1.0f;
    return input;
}
