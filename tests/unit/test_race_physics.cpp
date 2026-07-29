#include "../../src/race/race_physics.hpp"

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void ExpectNear(float actual, float expected, const char* message) {
    Expect(std::fabs(actual - expected) < 0.0001f, message);
}

void TestBrakeDamping() {
    const float fullAnalog = RacePhysics::BrakeDamping(1.0f, 1.0f);
    const float digital = RacePhysics::BrakeDamping(RacePhysics::kDigitalBrakeAxis, 1.0f);
    ExpectNear(RacePhysics::BrakeDamping(0.0f, 1.0f), 0.0f,
               "zero brake input produces no damping");
    ExpectNear(RacePhysics::BrakeDamping(0.25f, 1.0f), 2.0f,
               "analogue brake input remains proportional");
    ExpectNear(fullAnalog, 8.0f, "a fully pressed analogue trigger retains full braking");
    Expect(digital > 0.0f && digital < fullAnalog,
           "digital braking is moderated below full analogue braking");
    ExpectNear(RacePhysics::BrakeDamping(1.0f, 0.58f), 4.64f,
               "surface grip continues to scale braking");
    ExpectNear(RacePhysics::BrakeDamping(-1.0f, 1.0f), 0.0f,
               "negative brake input is clamped");
    ExpectNear(RacePhysics::BrakeDamping(2.0f, 1.0f), fullAnalog,
               "brake input above one is clamped");
    ExpectNear(RacePhysics::BrakeDamping(1.0f, -1.0f), 0.0f,
               "negative grip is clamped");
}

} // namespace

int main() {
    TestBrakeDamping();
    if (failures == 0) std::cout << "Neon Racer race physics tests passed.\n";
    return failures == 0 ? 0 : 1;
}
