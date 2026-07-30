#include "../../src/race/vehicle_dynamics.hpp"
#include "../../src/track/track.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

const float kFixedStep = 1.0f / 120.0f;

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

bool NearlyEqual(float first, float second) {
    return std::fabs(first - second) < 0.0001f;
}

RaceInput Input(float steering = 0.0f, float accelerate = 0.0f, float brake = 0.0f, float reverse = 0.0f) {
    return RaceInput{steering, accelerate, brake, reverse, false, false};
}

TrackContact FlatContact(SurfaceMaterial material = SurfaceMaterial::Regular, bool guardrailHit = false) {
    return TrackContact{true,
                        TrackSurfaceSample{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                           0.0f, 1.0f, 0.0f, 2.5f, material, 1},
                        0.0f, guardrailHit, false, 0.0f};
}

class ScriptedSurfaceQuery : public VehicleSurfaceQuery {
public:
    explicit ScriptedSurfaceQuery(const std::vector<TrackContact>& contacts)
        : contacts_(contacts), nextContact_(0) {}

    TrackContact QuerySurface(RaceVector3 position, float maxDistance) const override {
        queriedPositions_.push_back(position);
        queriedDistances_.push_back(maxDistance);
        if (contacts_.empty()) return TrackContact{false, FlatContact().surface, 0.0f, false, false, 0.0f};
        const std::size_t index = nextContact_ < contacts_.size() ? nextContact_ : contacts_.size() - 1;
        ++nextContact_;
        return contacts_[index];
    }

    std::size_t QueryCount() const { return queriedPositions_.size(); }
    const RaceVector3& LastPosition() const { return queriedPositions_.back(); }
    float LastDistance() const { return queriedDistances_.back(); }

private:
    std::vector<TrackContact> contacts_;
    mutable std::size_t nextContact_;
    mutable std::vector<RaceVector3> queriedPositions_;
    mutable std::vector<float> queriedDistances_;
};

class TrackSurfaceQuery : public VehicleSurfaceQuery {
public:
    explicit TrackSurfaceQuery(const Track& track) : track_(track) {}

    TrackContact QuerySurface(RaceVector3 position, float maxDistance) const override {
        return track_.QuerySurface(position.x, position.y, position.z, maxDistance);
    }

private:
    const Track& track_;
};

void TestResetPose() {
    VehicleDynamics vehicle;
    vehicle.ResetPose(RaceVector3{3.0f, 2.0f, -4.0f}, 0.0f);
    const RaceCar& car = vehicle.Car();
    Expect(NearlyEqual(car.position.x, 3.0f) && NearlyEqual(car.position.y, 2.16f) &&
               NearlyEqual(car.position.z, -4.0f) && NearlyEqual(car.velocity.x, 0.0f) &&
               NearlyEqual(car.velocity.y, 0.0f) && NearlyEqual(car.velocity.z, 0.0f) &&
               NearlyEqual(car.forward.x, 1.0f) && NearlyEqual(car.forward.y, 0.0f) &&
               NearlyEqual(car.forward.z, 0.0f) && NearlyEqual(car.up.x, 0.0f) &&
               NearlyEqual(car.up.y, 1.0f) && NearlyEqual(car.up.z, 0.0f) &&
               NearlyEqual(car.headingRadians, 0.0f) && NearlyEqual(car.speed, 0.0f),
           "reset pose applies ride height and restores a canonical car state");
}

void TestVisualBodyCenter() {
    RaceCar car = RaceCar{RaceVector3{3.0f, 2.0f, -4.0f}, RaceVector3{0.0f, 0.0f, 0.0f},
                          RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f, 0.0f};
    RaceVector3 bodyCenter = RaceCarVisualCenter(car);
    Expect(NearlyEqual(bodyCenter.x, 3.0f) &&
               NearlyEqual(bodyCenter.y, 2.0f + kRaceCarVisualBodyLift) && NearlyEqual(bodyCenter.z, -4.0f),
           "the rendered chassis center clears a flat road from the suspension reference");

    car.up = RaceVector3{0.0f, 0.0f, 1.0f};
    bodyCenter = RaceCarVisualCenter(car);
    Expect(NearlyEqual(bodyCenter.x, 3.0f) && NearlyEqual(bodyCenter.y, 2.0f) &&
               NearlyEqual(bodyCenter.z, -4.0f + kRaceCarVisualBodyLift),
           "the visual chassis lift follows the road normal on banked track");
}

void TestDefaultTwistMaintainsContact() {
    Track track;
    const int approachLength = 40;
    Expect(track.AddStraight(GridPosition{-approachLength, 0, 0}, Heading::East, 20) != 0 &&
               track.AddStraight(GridPosition{-20, 0, 0}, Heading::East, 20) != 0,
           "twist contact setup provides a straight run-up");
    const std::uint32_t twistId = track.AddTwist(GridPosition{0, 0, 0}, Heading::East,
                                                   TrackLimits::kDefaultTwistLength);
    Expect(track.AddStraight(GridPosition{TrackLimits::kDefaultTwistLength, 0, 0}, Heading::East, 20) != 0,
           "twist contact setup provides a straight exit");
    TrackSurfaceQuery query(track);
    VehicleDynamics vehicle;
    vehicle.ResetPose(RaceVector3{-static_cast<float>(approachLength), 0.0f, 0.0f}, 0.0f);

    bool lostContactInsideTwist = false;
    bool enteredTwist = false;
    bool reachedExit = false;
    bool sawInvertedFrame = false;
    float highestPosition = -1000000.0f;
    float maximumRideHeightError = 0.0f;
    for (int step = 0; step < 3000; ++step) {
        vehicle.Step(query, kFixedStep, Input(0.0f, 1.0f));
        const RaceCar& car = vehicle.Car();
        const TrackContact contact = track.QuerySurface(car.position.x, car.position.y, car.position.z, 1.4f);
        if (contact.found && contact.surface.pieceId == twistId) {
            enteredTwist = true;
            sawInvertedFrame = sawInvertedFrame || car.up.y < -0.80f;
            highestPosition = std::max(highestPosition, car.position.y);
            const float height = (car.position.x - contact.surface.x) * contact.surface.normalX +
                (car.position.y - contact.surface.y) * contact.surface.normalY +
                (car.position.z - contact.surface.z) * contact.surface.normalZ;
            maximumRideHeightError = std::max(maximumRideHeightError, std::fabs(height - 0.16f));
            if (!vehicle.IsOnTrack() || !contact.twistGuide || contact.guardrailHit) lostContactInsideTwist = true;
        }
        if (enteredTwist && car.position.x >= static_cast<float>(TrackLimits::kDefaultTwistLength + 4)) {
            reachedExit = true;
            break;
        }
    }

    const float expectedTop = static_cast<float>(TrackLimits::kDefaultTwistRadius) * 2.0f;
    Expect(twistId != 0 && enteredTwist && reachedExit,
           "an accelerating car enters and exits the default wide corkscrew");
    Expect(!lostContactInsideTwist && sawInvertedFrame,
           "the default wide corkscrew keeps a car guided through its inverted frame");
    Expect(highestPosition >= expectedTop - 1.0f && maximumRideHeightError < 0.75f,
           "the default wide corkscrew reaches its high loop apex without excess ride-height drift");
}

void TestDriveAndSurfaceState() {
    const std::vector<TrackContact> contacts(48, FlatContact(SurfaceMaterial::Slippery));
    ScriptedSurfaceQuery coastQuery(contacts);
    ScriptedSurfaceQuery driveQuery(contacts);
    VehicleDynamics coast;
    VehicleDynamics driven;
    coast.ResetPose(RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f);
    driven.ResetPose(RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f);

    for (int step = 0; step < 24; ++step) {
        coast.Step(coastQuery, kFixedStep, Input());
        driven.Step(driveQuery, kFixedStep, Input(0.0f, 1.0f));
    }

    Expect(driven.Car().speed > coast.Car().speed + 0.1f,
           "direct vehicle dynamics applies injected acceleration independently of TimeTrial");
    Expect(driven.IsOnTrack() && driven.CurrentSurfaceMaterial() == SurfaceMaterial::Slippery,
           "a found contact updates the vehicle's contact state");
    Expect(driveQuery.QueryCount() == 48 && NearlyEqual(driveQuery.LastDistance(), 1.4f),
           "each vehicle step performs the pre- and post-movement surface queries");
}

void TestOffTrackTransition() {
    ScriptedSurfaceQuery noSurface(std::vector<TrackContact>{});
    VehicleDynamics vehicle;
    vehicle.ResetPose(RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f);
    vehicle.Step(noSurface, kFixedStep, Input());

    Expect(!vehicle.IsOnTrack() && vehicle.Car().velocity.y < 0.0f,
           "missing contacts transition the vehicle into gravity-driven off-track motion");
    Expect(noSurface.QueryCount() == 2 && noSurface.LastPosition().y < 0.16f,
           "off-track movement still performs the post-movement guardrail query");
}

void TestResetPreservesContactHistory() {
    ScriptedSurfaceQuery slipperyQuery(std::vector<TrackContact>{FlatContact(SurfaceMaterial::Slippery),
                                                                  FlatContact(SurfaceMaterial::Slippery)});
    ScriptedSurfaceQuery noSurface(std::vector<TrackContact>{});
    VehicleDynamics vehicle;
    vehicle.ResetPose(RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f);
    vehicle.Step(slipperyQuery, kFixedStep, Input());
    vehicle.Step(noSurface, kFixedStep, Input());
    vehicle.ResetPose(RaceVector3{4.0f, 0.0f, 0.0f}, 0.0f);

    Expect(!vehicle.IsOnTrack() && vehicle.CurrentSurfaceMaterial() == SurfaceMaterial::Slippery,
           "reset preserves the prior contact history until the next surface query");
}

void TestPriorSurfaceGripAppliesToNextStep() {
    const std::vector<TrackContact> slipperyThenRegular{
        FlatContact(SurfaceMaterial::Slippery), FlatContact(SurfaceMaterial::Slippery),
        FlatContact(SurfaceMaterial::Slippery), FlatContact(SurfaceMaterial::Slippery),
        FlatContact(SurfaceMaterial::Regular), FlatContact(SurfaceMaterial::Regular)};
    const std::vector<TrackContact> regularThenRegular{
        FlatContact(SurfaceMaterial::Regular), FlatContact(SurfaceMaterial::Regular),
        FlatContact(SurfaceMaterial::Regular), FlatContact(SurfaceMaterial::Regular),
        FlatContact(SurfaceMaterial::Regular), FlatContact(SurfaceMaterial::Regular)};
    ScriptedSurfaceQuery slipperyQuery(slipperyThenRegular);
    ScriptedSurfaceQuery regularQuery(regularThenRegular);
    VehicleDynamics slipperyPrevious;
    VehicleDynamics regularPrevious;
    slipperyPrevious.ResetPose(RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f);
    regularPrevious.ResetPose(RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f);

    for (int step = 0; step < 2; ++step) {
        slipperyPrevious.Step(slipperyQuery, kFixedStep, Input(0.0f, 1.0f));
        regularPrevious.Step(regularQuery, kFixedStep, Input(0.0f, 1.0f));
    }
    slipperyPrevious.Step(slipperyQuery, kFixedStep, Input(0.0f, 0.0f, 1.0f));
    regularPrevious.Step(regularQuery, kFixedStep, Input(0.0f, 0.0f, 1.0f));

    Expect(slipperyPrevious.Car().speed > regularPrevious.Car().speed,
           "the current step's braking uses the previous contact material's grip");
}

void TestGuardrailEvent() {
    TrackContact guardrail = FlatContact();
    guardrail.surface.z = 2.5f;
    guardrail.guardrailHit = true;
    ScriptedSurfaceQuery query(std::vector<TrackContact>{guardrail, FlatContact()});
    VehicleDynamics vehicle;
    vehicle.ResetPose(RaceVector3{0.0f, 0.0f, 2.7f}, 0.0f);

    const VehicleStepResult result = vehicle.Step(query, kFixedStep, Input());
    Expect(result.guardrailImpact,
           "a guardrail contact is reported to time-trial orchestration as a discrete event");
    Expect(vehicle.Car().position.z < 2.7f,
           "a guardrail response pushes the car inward instead of leaving it beyond the road edge");
}

void TestPostMoveGuardrailEvent() {
    ScriptedSurfaceQuery query(std::vector<TrackContact>{FlatContact(), FlatContact(SurfaceMaterial::Regular, true)});
    VehicleDynamics vehicle;
    vehicle.ResetPose(RaceVector3{0.0f, 0.0f, 0.0f}, 0.0f);

    const VehicleStepResult result = vehicle.Step(query, kFixedStep, Input());
    Expect(result.guardrailImpact && query.QueryCount() == 2,
           "a guardrail reported only after integration still reaches collision response");
}

} // namespace

int main() {
    TestResetPose();
    TestVisualBodyCenter();
    TestDefaultTwistMaintainsContact();
    TestDriveAndSurfaceState();
    TestOffTrackTransition();
    TestResetPreservesContactHistory();
    TestPriorSurfaceGripAppliesToNextStep();
    TestGuardrailEvent();
    TestPostMoveGuardrailEvent();
    if (failures == 0) std::cout << "Neon Racer vehicle dynamics tests passed.\n";
    return failures == 0 ? 0 : 1;
}
