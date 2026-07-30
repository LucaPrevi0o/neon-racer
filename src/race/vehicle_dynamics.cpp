#include "vehicle_dynamics.hpp"

#include "race_physics.hpp"
#include "../track/track_road_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace {

const float kMaxForwardSpeed = 32.0f;
const float kMaxReverseSpeed = 11.0f;
const float kCarRideHeight = 0.16f;
const float kGravity = 18.0f;
const float kSuspensionSpring = 115.0f;
const float kSuspensionDamping = 20.0f;
const float kAirDrag = 0.018f;
const float kGuardrailSkin = 0.015f;
const float kTwistGuideCaptureDistance = 0.90f;
const float kTwistGuideSpring = 145.0f;
const float kTwistGuideDamping = 24.0f;
const float kMaximumTwistGuideForce = 155.0f;
const float kTwistLateralGuideSpring = 36.0f;
const float kTwistLateralGuideDamping = 14.0f;
const float kMaximumTwistLateralGuideForce = 105.0f;

RaceVector3 Forward(float heading) {
    return RaceVector3{std::cos(heading), 0.0f, std::sin(heading)};
}

RaceVector3 Add(RaceVector3 a, RaceVector3 b) { return RaceVector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
RaceVector3 Scale(RaceVector3 vector, float amount) {
    return RaceVector3{vector.x * amount, vector.y * amount, vector.z * amount};
}
float Dot(RaceVector3 a, RaceVector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
RaceVector3 Cross(RaceVector3 a, RaceVector3 b) {
    return RaceVector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Magnitude(RaceVector3 vector) { return std::sqrt(Dot(vector, vector)); }
RaceVector3 Normalize(RaceVector3 vector, RaceVector3 fallback) {
    const float length = Magnitude(vector);
    return length > 0.0001f ? Scale(vector, 1.0f / length) : fallback;
}
RaceVector3 ProjectOnPlane(RaceVector3 vector, RaceVector3 normal) {
    return Add(vector, Scale(normal, -Dot(vector, normal)));
}
RaceVector3 RotateAround(RaceVector3 vector, RaceVector3 axis, float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return Add(Add(Scale(vector, cosine), Scale(Cross(axis, vector), sine)),
               Scale(axis, Dot(axis, vector) * (1.0f - cosine)));
}

RaceVector3 RoadSide(const TrackSurfaceSample& surface, RaceVector3 fallback) {
    // Contacts normally come from Track::QuerySurface(), whose frame is
    // already valid. Retain the fallback policy so a synthesized or malformed
    // contact cannot destabilize guardrail response.
    const RaceVector3 tangent =
        Normalize(RaceVector3{surface.tangentX, surface.tangentY, surface.tangentZ}, fallback);
    const RaceVector3 normal = Normalize(RaceVector3{surface.normalX, surface.normalY, surface.normalZ},
                                         RaceVector3{0.0f, 1.0f, 0.0f});
    TrackSurfaceSample normalizedSurface = surface;
    normalizedSurface.tangentX = tangent.x;
    normalizedSurface.tangentY = tangent.y;
    normalizedSurface.tangentZ = tangent.z;
    normalizedSurface.normalX = normal.x;
    normalizedSurface.normalY = normal.y;
    normalizedSurface.normalZ = normal.z;
    TrackRoadAxis axis;
    if (!TrackRoadGeometry::NormalizedSurfaceRightAxis(normalizedSurface, axis)) return fallback;
    return RaceVector3{axis.x, axis.y, axis.z};
}

// `TrackContact::surface` sits on the closest road edge when guardrailHit is
// true. Resolve only the signed distance across that edge: the distance to the
// sampled point also contains motion along the road, which must not be treated
// as penetration or it can pull a car backwards into a rail.
bool ResolveGuardrailContact(RaceCar& car, const TrackContact& contact, RaceVector3* acceleration) {
    if (!contact.found || !contact.guardrailHit) return false;

    const RaceVector3 fallbackSide = Normalize(Cross(car.forward, car.up), RaceVector3{0.0f, 0.0f, 1.0f});
    const RaceVector3 side = RoadSide(contact.surface, fallbackSide);
    const RaceVector3 edgePoint = RaceVector3{contact.surface.x, contact.surface.y, contact.surface.z};
    const float signedPenetration = Dot(Add(car.position, Scale(edgePoint, -1.0f)), side);
    if (std::fabs(signedPenetration) <= 0.0001f) return true;

    const RaceVector3 outward = Scale(side, signedPenetration < 0.0f ? -1.0f : 1.0f);
    const float penetration = std::fabs(signedPenetration);

    // Keep a small inward skin so the next surface query is clearly on the
    // drivable side of the rail. This deliberately has no tangent component.
    car.position = Add(car.position, Scale(outward, -(penetration + kGuardrailSkin)));

    // A rail only removes the component that continues out through it. The
    // tangential component allows sliding; an inward component lets the car
    // reverse away without becoming stuck.
    const float outwardSpeed = Dot(car.velocity, outward);
    if (outwardSpeed > 0.0f) car.velocity = Add(car.velocity, Scale(outward, -outwardSpeed));
    if (acceleration != 0) {
        const float outwardAcceleration = Dot(*acceleration, outward);
        if (outwardAcceleration > 0.0f)
            *acceleration = Add(*acceleration, Scale(outward, -outwardAcceleration));
    }
    return true;
}

float GripFor(SurfaceMaterial material) {
    switch (material) {
    case SurfaceMaterial::Slippery: return 0.58f;
    case SurfaceMaterial::HighResistance: return 1.20f;
    case SurfaceMaterial::Regular: return 1.0f;
    }
    return 1.0f;
}

float DriveFor(SurfaceMaterial material) {
    return material == SurfaceMaterial::HighResistance ? 0.82f : 1.0f;
}

} // namespace

VehicleDynamics::VehicleDynamics()
    : car_{RaceVector3{0.0f, kCarRideHeight, 0.0f}, RaceVector3{0.0f, 0.0f, 0.0f},
           RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f, 0.0f},
      onTrack_(true), surfaceMaterial_(SurfaceMaterial::Regular) {
}

void VehicleDynamics::ResetPose(RaceVector3 roadPosition, float headingRadians) {
    car_.position = roadPosition;
    car_.position.y += kCarRideHeight;
    car_.headingRadians = headingRadians;
    car_.velocity = RaceVector3{0.0f, 0.0f, 0.0f};
    car_.forward = Forward(car_.headingRadians);
    car_.up = RaceVector3{0.0f, 1.0f, 0.0f};
    car_.speed = 0.0f;
    // Do not reset contact history here. TimeTrial historically preserves it
    // across a reset until this vehicle's next surface query.
}

VehicleStepResult VehicleDynamics::Step(const VehicleSurfaceQuery& surfaceQuery, float deltaTime,
                                        const RaceInput& input) {
    const float steering = input.steering;
    const float accelerate = input.accelerate;
    const float brake = input.brake;
    const float reverse = input.reverse;

    VehicleStepResult result = VehicleStepResult{false, false, 0};
    const TrackContact contact = surfaceQuery.QuerySurface(car_.position, 1.4f);
    if (contact.found) {
        result.hasSurfaceContact = true;
        result.surfacePieceId = contact.surface.pieceId;
    }
    const float grip = onTrack_ ? GripFor(surfaceMaterial_) : 0.20f;
    RaceVector3 acceleration = RaceVector3{0.0f, -kGravity, 0.0f};

    if (contact.found) {
        const RaceVector3 normal = Normalize(RaceVector3{contact.surface.normalX, contact.surface.normalY,
                                                          contact.surface.normalZ},
                                            RaceVector3{0.0f, 1.0f, 0.0f});
        const RaceVector3 surfacePoint = RaceVector3{contact.surface.x, contact.surface.y, contact.surface.z};
        const float height = Dot(Add(car_.position, Scale(surfacePoint, -1.0f)), normal);
        const float normalVelocity = Dot(car_.velocity, normal);
        float support = std::max(0.0f, (kCarRideHeight - height) * kSuspensionSpring -
                                       normalVelocity * kSuspensionDamping);
        if (contact.twistGuide && contact.distance <= kTwistGuideCaptureDistance) {
            // A Twist is an enclosed corkscrew frame, unlike an ordinary
            // road or open vertical loop. Keep a nearby car centered at its
            // ride height through the inverted section, but cap the pull so
            // a car that has genuinely left the road is still released.
            const float guideForce = (kCarRideHeight - height) * kTwistGuideSpring -
                normalVelocity * kTwistGuideDamping;
            support = std::max(-kMaximumTwistGuideForce,
                               std::min(kMaximumTwistGuideForce, guideForce));
        }
        acceleration = Add(acceleration, Scale(normal, support));

        const RaceVector3 roadForward = Normalize(RaceVector3{contact.surface.tangentX, contact.surface.tangentY,
                                                               contact.surface.tangentZ},
                                                  car_.forward);
        if (contact.twistGuide && contact.distance <= kTwistGuideCaptureDistance) {
            // The corkscrew guide supplies a stable longitudinal frame as
            // well as normal/lateral restraint. This prevents a guided car
            // from parallel-transporting its old heading into the side wall.
            car_.forward = roadForward;
        } else {
            car_.forward = Normalize(ProjectOnPlane(car_.forward, normal), roadForward);
        }
        const float forwardSpeed = Dot(car_.velocity, car_.forward);
        const float steeringRate = -steering * 2.35f * grip * std::min(1.0f, std::fabs(forwardSpeed) / 8.0f) *
                                   (forwardSpeed < 0.0f ? -1.0f : 1.0f);
        car_.forward = Normalize(RotateAround(car_.forward, normal, steeringRate * deltaTime), roadForward);
        car_.up = normal;

        const RaceVector3 right = Normalize(Cross(car_.forward, normal), RaceVector3{0.0f, 0.0f, 1.0f});
        const float lateralSpeed = Dot(car_.velocity, right);
        const float lateralForce = std::max(-kGravity * grip, std::min(kGravity * grip, -lateralSpeed / deltaTime));
        acceleration = Add(acceleration, Scale(right, lateralForce));
        if (contact.twistGuide && contact.distance <= kTwistGuideCaptureDistance) {
            // Keep the car near the corkscrew centerline as well as its
            // normal ride height. This is deliberately Twist-only: ordinary
            // roads retain their existing drift and open-loop behavior.
            const RaceVector3 roadRight = RoadSide(contact.surface, right);
            const float roadLateralSpeed = Dot(car_.velocity, roadRight);
            const float guideForce = -contact.lateralOffset * kTwistLateralGuideSpring -
                roadLateralSpeed * kTwistLateralGuideDamping;
            const float clampedGuideForce = std::max(-kMaximumTwistLateralGuideForce,
                                                     std::min(kMaximumTwistLateralGuideForce, guideForce));
            acceleration = Add(acceleration, Scale(roadRight, clampedGuideForce));
        }

        const float driveInput = accelerate - reverse;
        const float driveForce = driveInput >= 0.0f ? 22.0f : 14.0f;
        if (std::fabs(forwardSpeed) < (driveInput >= 0.0f ? kMaxForwardSpeed : kMaxReverseSpeed)) {
            acceleration = Add(acceleration,
                               Scale(car_.forward, driveInput * driveForce * DriveFor(contact.surface.material)));
        }
        if (brake > 0.0f)
            acceleration = Add(acceleration, Scale(car_.velocity, -RacePhysics::BrakeDamping(brake, grip)));
        acceleration = Add(acceleration, Scale(car_.velocity, -0.75f));
        if (ResolveGuardrailContact(car_, contact, &acceleration)) result.guardrailImpact = true;
        surfaceMaterial_ = contact.surface.material;
        onTrack_ = true;
    } else {
        onTrack_ = false;
        car_.up = Normalize(Add(Scale(car_.up, 0.98f), RaceVector3{0.0f, 0.02f, 0.0f}),
                            RaceVector3{0.0f, 1.0f, 0.0f});
    }

    const float velocityLength = Magnitude(car_.velocity);
    acceleration = Add(acceleration, Scale(car_.velocity, -kAirDrag * velocityLength));
    car_.velocity = Add(car_.velocity, Scale(acceleration, deltaTime));
    car_.position = Add(car_.position, Scale(car_.velocity, deltaTime));

    // The pre-integration response prevents persistent overlap. Query once
    // more after movement as well: a car can cross an edge during this fixed
    // step even when it began inside the road.
    const TrackContact postMoveContact = surfaceQuery.QuerySurface(car_.position, 1.4f);
    if (postMoveContact.found) {
        result.hasSurfaceContact = true;
        result.surfacePieceId = postMoveContact.surface.pieceId;
    }
    if (ResolveGuardrailContact(car_, postMoveContact, 0)) result.guardrailImpact = true;

    car_.headingRadians = std::atan2(car_.forward.z, car_.forward.x);
    car_.speed = Dot(car_.velocity, car_.forward);
    return result;
}

const RaceCar& VehicleDynamics::Car() const { return car_; }
bool VehicleDynamics::IsOnTrack() const { return onTrack_; }
SurfaceMaterial VehicleDynamics::CurrentSurfaceMaterial() const { return surfaceMaterial_; }
