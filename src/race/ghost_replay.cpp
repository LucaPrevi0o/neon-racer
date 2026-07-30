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

} // namespace

GhostReplay::GhostReplay() : nextSampleTime_(0.0f), verifiedDuration_(0.0f) {}

void GhostReplay::ResetCandidate() {
    candidateSamples_.clear();
    nextSampleTime_ = 0.0f;
}

void GhostReplay::ClearVerified() {
    verifiedSamples_.clear();
    verifiedDuration_ = 0.0f;
}

void GhostReplay::Capture(const RaceCar& car, float elapsedSeconds) {
    if (elapsedSeconds < nextSampleTime_) return;
    candidateSamples_.push_back(GhostSample{car, elapsedSeconds});
    nextSampleTime_ += kSampleInterval;
}

bool GhostReplay::PromoteCandidateIfFaster(float durationSeconds) {
    if (!verifiedSamples_.empty() && durationSeconds >= verifiedDuration_) return false;
    verifiedSamples_ = candidateSamples_;
    verifiedDuration_ = durationSeconds;
    return !verifiedSamples_.empty();
}

bool GhostReplay::HasVerified() const { return !verifiedSamples_.empty() && verifiedDuration_ > 0.0f; }

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
