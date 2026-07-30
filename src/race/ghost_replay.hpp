#pragma once

#include <vector>

#include "race_contracts.hpp"

// Stores the candidate run currently being recorded and the fastest completed
// run available for playback. It deliberately has no Track or UI dependency:
// TimeTrial decides when a run starts, finishes, or becomes invalid.
class GhostReplay {
public:
    GhostReplay();

    // Starts recording a new candidate without discarding the verified replay.
    void ResetCandidate();

    // Discards the verified replay when the owning track's layout changes.
    void ClearVerified();

    // Captures at the component's fixed replay cadence.
    void Capture(const RaceCar& car, float elapsedSeconds);

    // Replaces the verified replay only when the candidate is strictly faster.
    // Returns true only when a non-empty candidate became the verified replay.
    bool PromoteCandidateIfFaster(float durationSeconds);

    bool HasVerified() const;

    // Samples the verified replay at a looping playback time, or returns the
    // fallback car when no replay is available.
    RaceCar SampleAt(float playbackSeconds, const RaceCar& fallback) const;

private:
    std::vector<GhostSample> candidateSamples_;
    std::vector<GhostSample> verifiedSamples_;
    float nextSampleTime_;
    float verifiedDuration_;
};
