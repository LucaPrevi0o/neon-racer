#pragma once

#include <vector>

#include "race_contracts.hpp"
#include "race_input.hpp"
#include "../track/track.hpp"

class TimeTrial {
public:
    TimeTrial();

    void Start(const Track& track);
    void Update(float frameTime, const RaceInput& input);
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
    void FixedUpdate(float deltaTime, const RaceInput& input);
    void CompleteLap();
    void ResetCarToStart();
    RaceVector3 StartPosition() const;
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
