#include "race_physics.hpp"

#include <algorithm>

namespace {

const float kFullBrakeDamping = 8.0f;

} // namespace

namespace RacePhysics {

float BrakeDamping(float brakeAxis, float grip) {
    const float clampedBrake = std::max(0.0f, std::min(1.0f, brakeAxis));
    return clampedBrake * kFullBrakeDamping * std::max(0.0f, grip);
}

} // namespace RacePhysics
