#pragma once

#include "race_contracts.hpp"
#include "race_input.hpp"
#include "../track/track_contracts.hpp"

// Narrow contact-query boundary for vehicle simulation. Track adapts its
// surface sampler to this interface at the time-trial boundary, while tests
// can supply precise contacts without constructing layout geometry.
class VehicleSurfaceQuery {
public:
    virtual ~VehicleSurfaceQuery() {}
    virtual TrackContact QuerySurface(RaceVector3 position, float maxDistance) const = 0;
};

// Per-step information that is meaningful to the time-trial coordinator but
// should not make vehicle simulation depend on UI status text or lap rules.
struct VehicleStepResult {
    bool guardrailImpact;
};

// Raylib-free vehicle simulation. It owns the car pose and the contact history
// needed to calculate grip on the next fixed step; TimeTrial owns scheduling,
// laps, ghost recording, and player-facing state around it.
class VehicleDynamics {
public:
    VehicleDynamics();

    // `roadPosition` is the track surface position. The vehicle applies its
    // own suspension ride height when creating the car pose.
    void ResetPose(RaceVector3 roadPosition, float headingRadians);
    VehicleStepResult Step(const VehicleSurfaceQuery& surfaceQuery, float deltaTime, const RaceInput& input);

    const RaceCar& Car() const;
    bool IsOnTrack() const;
    SurfaceMaterial CurrentSurfaceMaterial() const;

private:
    RaceCar car_;
    bool onTrack_;
    SurfaceMaterial surfaceMaterial_;
};
