#pragma once

#include <cstddef>
#include <vector>

#include "race_contracts.hpp"

// A verified replay is persisted and rendered as trusted gameplay data. These
// caps keep imports and interpolation within a practical, bounded envelope.
namespace GhostReplayLimits {

const std::size_t kMaximumVerifiedSampleCount = 250000u;
const float kMaximumVerifiedDurationSeconds = 7200.0f;
const float kMaximumReplayScalar = 1000000.0f;

} // namespace GhostReplayLimits

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

    // Shared validation for persistence and import boundaries.  It verifies
    // the replay payload only; layout ownership belongs to TimeTrial.
    static bool IsValidVerifiedData(const VerifiedGhostData& data);

    // Copies the verified replay into a value-only transfer object.  The
    // replay component has no Track dependency, so the caller owns its layout
    // fingerprint.  Returns false without changing output when no verified
    // replay is available.
    bool ExportVerified(VerifiedGhostData& output) const;

    // Replaces the verified replay from a validated transfer object.  Invalid
    // input leaves both candidate and verified state unchanged; a successful
    // import starts a fresh candidate recording.
    bool ImportVerified(const VerifiedGhostData& input);

    // Samples the verified replay at a looping playback time, or returns the
    // fallback car when no replay is available.
    RaceCar SampleAt(float playbackSeconds, const RaceCar& fallback) const;

private:
    std::vector<GhostSample> candidateSamples_;
    std::vector<GhostSample> verifiedSamples_;
    float nextSampleTime_;
    float verifiedDuration_;
    bool candidateTruncated_;
};
