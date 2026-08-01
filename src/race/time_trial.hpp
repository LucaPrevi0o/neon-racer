#pragma once

#include "ghost_replay.hpp"
#include "internal/track_position_tracker.hpp"
#include "race_contracts.hpp"
#include "race_input.hpp"
#include "vehicle_dynamics.hpp"
#include "../track/track.hpp"

// Coordinates a complete three-lap attempt around the reusable simulation
// components without owning device polling, vehicle dynamics, or replay math.
class TimeTrial {
public:
    TimeTrial();

    void Start(const Track& track);
    void Update(float frameTime, const RaceInput& input);
    void Reset();
    void Recover();
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

    // Transfers a replay only when it is verified for the active, unchanged
    // layout.  Imported replay data must carry the active layout fingerprint;
    // verification state is then derived from this TimeTrial's Track.
    bool ExportVerifiedGhost(VerifiedGhostData& output) const;
    bool ImportVerifiedGhost(const VerifiedGhostData& input);
    std::uint64_t ActiveLayoutFingerprint() const;

    const char* StatusMessage() const;

private:
    void FixedUpdate(float deltaTime, const RaceInput& input);
    void CompleteLap();
    void ResetCarToStart();
    void SynchronizeStartProjection();
    RaceVector3 StartPosition() const;
    float StartHeading() const;

    const Track* track_;
    VehicleDynamics vehicle_;
    TrackPositionTracker trackPosition_;
    float accumulator_;
    float currentLapTime_;
    float bestLapTime_;
    float totalTime_;
    float previousStartProjection_;
    int completedLaps_;
    bool paused_;
    bool finished_;
    bool ready_;
    GhostReplay ghostReplay_;
    VerificationState verification_;
    const char* statusMessage_;
};

const char* FormatRaceTime(float seconds);
