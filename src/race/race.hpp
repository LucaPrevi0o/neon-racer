#pragma once

#include <raylib.h>

#include "../track/track.hpp"

struct RaceCar {
    Vector3 position;
    Vector3 velocity;
    Vector3 forward;
    Vector3 up;
    float headingRadians;
    float speed;
};

struct GhostSample {
    RaceCar car;
    float time;
};

class TimeTrial {
public:
    TimeTrial();

    void Start(const Track& track);
    void Update(float frameTime);
    void Reset();
    void TogglePause();

    bool IsReady() const;
    bool IsPaused() const;
    bool IsFinished() const;
    int CurrentLap() const;
    float CurrentLapTime() const;
    float BestLapTime() const;
    float TotalTime() const;
    const RaceCar& Car() const;
    bool IsOnTrack() const;
    SurfaceMaterial CurrentSurfaceMaterial() const;
    bool HasVerifiedGhost() const;
    RaceCar GhostCar() const;
    const VerificationState& Verification() const;
    const char* StatusMessage() const;

private:
    void FixedUpdate(float deltaTime);
    void CompleteLap();
    void ResetCarToStart();
    Vector3 StartPosition() const;
    float StartHeading() const;

    const Track* track_;
    RaceCar car_;
    float accumulator_;
    float currentLapTime_;
    float bestLapTime_;
    float totalTime_;
    float previousStartProjection_;
    int completedLaps_;
    bool hasLeftStart_;
    bool paused_;
    bool finished_;
    bool ready_;
    bool onTrack_;
    SurfaceMaterial surfaceMaterial_;
    std::vector<GhostSample> recordingSamples_;
    std::vector<GhostSample> verifiedGhostSamples_;
    float nextGhostSampleTime_;
    float verifiedGhostDuration_;
    VerificationState verification_;
    const char* statusMessage_;
};

const char* FormatRaceTime(float seconds);
