#include "ghost_replay.hpp"

#include <cmath>

namespace {

const float kSampleInterval = 1.0f / 30.0f;

float Dot(RaceVector3 first, RaceVector3 second) {
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

RaceVector3 Scale(RaceVector3 vector, float amount) {
    return RaceVector3{vector.x * amount, vector.y * amount, vector.z * amount};
}

float Magnitude(RaceVector3 vector) { return std::sqrt(Dot(vector, vector)); }

RaceVector3 Normalize(RaceVector3 vector, RaceVector3 fallback) {
    const float length = Magnitude(vector);
    return length > 0.0001f ? Scale(vector, 1.0f / length) : fallback;
}

RaceVector3 Interpolate(RaceVector3 first, RaceVector3 second, float amount) {
    return RaceVector3{first.x + (second.x - first.x) * amount, first.y + (second.y - first.y) * amount,
                       first.z + (second.z - first.z) * amount};
}

bool IsFiniteAndBounded(float value) {
    return std::isfinite(value) && std::fabs(value) <= GhostReplayLimits::kMaximumReplayScalar;
}

bool IsFiniteVector(RaceVector3 vector) {
    return IsFiniteAndBounded(vector.x) && IsFiniteAndBounded(vector.y) && IsFiniteAndBounded(vector.z);
}

bool HasNonZeroMagnitude(RaceVector3 vector) {
    const float squaredMagnitude = Dot(vector, vector);
    return std::isfinite(squaredMagnitude) && squaredMagnitude > 0.00000001f;
}

bool IsValidSample(const GhostSample& sample) {
    const RaceCar& car = sample.car;
    return std::isfinite(sample.time) && IsFiniteVector(car.position) && IsFiniteVector(car.velocity) &&
           IsFiniteVector(car.forward) && IsFiniteVector(car.up) && HasNonZeroMagnitude(car.forward) &&
           HasNonZeroMagnitude(car.up) && IsFiniteAndBounded(car.headingRadians) && IsFiniteAndBounded(car.speed);
}

bool HasValidSamples(const std::vector<GhostSample>& samples, float durationSeconds) {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0f ||
        durationSeconds > GhostReplayLimits::kMaximumVerifiedDurationSeconds || samples.empty() ||
        samples.size() > GhostReplayLimits::kMaximumVerifiedSampleCount || samples.front().time != 0.0f) {
        return false;
    }

    float previousTime = -1.0f;
    for (std::vector<GhostSample>::const_iterator sample = samples.begin(); sample != samples.end(); ++sample) {
        if (!IsValidSample(*sample) || sample->time < 0.0f || sample->time > durationSeconds ||
            sample->time <= previousTime) {
            return false;
        }
        previousTime = sample->time;
    }
    return true;
}

} // namespace

GhostReplay::GhostReplay() : nextSampleTime_(0.0f), verifiedDuration_(0.0f), candidateTruncated_(false) {}

bool GhostReplay::IsValidVerifiedData(const VerifiedGhostData& data) {
    return HasValidSamples(data.samples, data.durationSeconds);
}

void GhostReplay::ResetCandidate() {
    candidateSamples_.clear();
    nextSampleTime_ = 0.0f;
    candidateTruncated_ = false;
}

void GhostReplay::ClearVerified() {
    verifiedSamples_.clear();
    verifiedDuration_ = 0.0f;
}

void GhostReplay::Capture(const RaceCar& car, float elapsedSeconds) {
    if (elapsedSeconds < nextSampleTime_) return;
    if (!std::isfinite(elapsedSeconds) || candidateSamples_.size() >= GhostReplayLimits::kMaximumVerifiedSampleCount) {
        candidateTruncated_ = true;
        return;
    }
    candidateSamples_.push_back(GhostSample{car, elapsedSeconds});
    nextSampleTime_ += kSampleInterval;
}

bool GhostReplay::PromoteCandidateIfFaster(float durationSeconds) {
    if (candidateTruncated_ || !HasValidSamples(candidateSamples_, durationSeconds)) return false;
    if (!verifiedSamples_.empty() && durationSeconds >= verifiedDuration_) return false;
    verifiedSamples_ = candidateSamples_;
    verifiedDuration_ = durationSeconds;
    return !verifiedSamples_.empty();
}

bool GhostReplay::HasVerified() const { return !verifiedSamples_.empty() && verifiedDuration_ > 0.0f; }

bool GhostReplay::ExportVerified(VerifiedGhostData& output) const {
    if (!HasVerified()) return false;

    VerifiedGhostData exported;
    exported.samples = verifiedSamples_;
    exported.durationSeconds = verifiedDuration_;
    if (!IsValidVerifiedData(exported)) return false;
    output = exported;
    return true;
}

bool GhostReplay::ImportVerified(const VerifiedGhostData& input) {
    if (!IsValidVerifiedData(input)) return false;

    // Copy before changing either replay collection so malformed input and a
    // failed allocation cannot leave a partial verified replay behind.
    std::vector<GhostSample> importedSamples = input.samples;
    candidateSamples_.clear();
    nextSampleTime_ = 0.0f;
    verifiedSamples_.swap(importedSamples);
    verifiedDuration_ = input.durationSeconds;
    return true;
}

RaceCar GhostReplay::SampleAt(float playbackSeconds, const RaceCar& fallback) const {
    if (!HasVerified()) return fallback;

    const float playbackTime = std::fmod(playbackSeconds, verifiedDuration_);
    for (std::size_t index = 1; index < verifiedSamples_.size(); ++index) {
        const GhostSample& next = verifiedSamples_[index];
        if (next.time < playbackTime) continue;

        const GhostSample& previous = verifiedSamples_[index - 1];
        const float span = next.time - previous.time;
        const float amount = span > 0.0f ? (playbackTime - previous.time) / span : 0.0f;
        return RaceCar{Interpolate(previous.car.position, next.car.position, amount),
                       Interpolate(previous.car.velocity, next.car.velocity, amount),
                       Normalize(Interpolate(previous.car.forward, next.car.forward, amount), previous.car.forward),
                       Normalize(Interpolate(previous.car.up, next.car.up, amount), previous.car.up),
                       previous.car.headingRadians + (next.car.headingRadians - previous.car.headingRadians) * amount,
                       previous.car.speed + (next.car.speed - previous.car.speed) * amount};
    }

    return verifiedSamples_.back().car;
}
