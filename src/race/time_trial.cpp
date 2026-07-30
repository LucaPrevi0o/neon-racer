#include "time_trial.hpp"

#include "../track/track_fingerprint.hpp"

#include <cmath>
#include <cstdio>

namespace {

const float kPi = 3.14159265358979323846f;
const float kFixedStep = 1.0f / 120.0f;
const float kMaxFrameTime = 0.10f;

float HeadingRadians(Heading heading) {
    switch (heading) {
    case Heading::East: return 0.0f;
    case Heading::South: return kPi * 0.5f;
    case Heading::West: return kPi;
    case Heading::North: return -kPi * 0.5f;
    }
    return 0.0f;
}

RaceVector3 Forward(float heading) {
    return RaceVector3{std::cos(heading), 0.0f, std::sin(heading)};
}

// TimeTrial owns the selected Track but adapts it at the vehicle boundary so
// vehicle dynamics can be tested against exact scripted surface contacts.
class TrackVehicleSurfaceQuery : public VehicleSurfaceQuery {
public:
    explicit TrackVehicleSurfaceQuery(const Track& track) : track_(track) {}

    TrackContact QuerySurface(RaceVector3 position, float maxDistance) const override {
        return track_.QuerySurface(position.x, position.y, position.z, maxDistance);
    }

private:
    const Track& track_;
};

} // namespace

TimeTrial::TimeTrial()
    : track_(0), accumulator_(0.0f), currentLapTime_(0.0f), bestLapTime_(0.0f), totalTime_(0.0f),
      previousStartProjection_(0.0f), completedLaps_(0), hasLeftStart_(false), paused_(false),
      finished_(false), ready_(false), statusMessage_("Open a race-ready track in the editor.") {
}

void TimeTrial::Start(const Track& track) {
    const std::uint64_t layoutFingerprint = TrackFingerprint::Calculate(track);
    if (verification_.hasSavedGhost &&
        (verification_.verifiedLayoutRevision != track.LayoutRevision() ||
         verification_.verifiedLayoutFingerprint != layoutFingerprint)) {
        ghostReplay_.ClearVerified();
        verification_ = VerificationState();
    }
    track_ = &track;
    ready_ = track.Validate().raceReady;
    if (!ready_) {
        statusMessage_ = "Track is not race-ready. Complete and validate it in the editor.";
        return;
    }
    Reset();
}

void TimeTrial::Update(float frameTime, const RaceInput& input) {
    if (!ready_) return;
    // Preserve the existing ordering when both actions arrive together: reset
    // follows pause and therefore resumes the time trial before simulation.
    if (input.pausePressed) TogglePause();
    if (input.resetPressed) Reset();
    if (paused_ || finished_) return;

    accumulator_ += std::fmin(frameTime, kMaxFrameTime);
    while (accumulator_ >= kFixedStep) {
        FixedUpdate(kFixedStep, input);
        accumulator_ -= kFixedStep;
    }
}

void TimeTrial::Reset() {
    if (!ready_) return;
    accumulator_ = 0.0f;
    currentLapTime_ = 0.0f;
    bestLapTime_ = 0.0f;
    totalTime_ = 0.0f;
    ghostReplay_.ResetCandidate();
    completedLaps_ = 0;
    hasLeftStart_ = false;
    paused_ = false;
    finished_ = false;
    statusMessage_ = "Time trial in progress.";
    ResetCarToStart();
}

void TimeTrial::TogglePause() {
    if (!ready_ || finished_) return;
    paused_ = !paused_;
    statusMessage_ = paused_ ? "Paused." : "Time trial resumed.";
}

bool TimeTrial::IsReady() const { return ready_; }
bool TimeTrial::IsPaused() const { return paused_; }
bool TimeTrial::IsFinished() const { return finished_; }
int TimeTrial::CurrentLap() const { return finished_ ? 3 : completedLaps_ + 1; }
float TimeTrial::CurrentLapTime() const { return currentLapTime_; }
float TimeTrial::BestLapTime() const { return bestLapTime_; }
float TimeTrial::TotalTime() const { return totalTime_; }
const RaceCar& TimeTrial::Car() const { return vehicle_.Car(); }
bool TimeTrial::IsOnTrack() const { return vehicle_.IsOnTrack(); }
SurfaceMaterial TimeTrial::CurrentSurfaceMaterial() const { return vehicle_.CurrentSurfaceMaterial(); }
bool TimeTrial::HasVerifiedGhost() const {
    return track_ != 0 && ready_ && verification_.hasSavedGhost && verification_.isVerifiedForPlayableExport &&
           verification_.verifiedLayoutRevision == track_->LayoutRevision() &&
           verification_.verifiedLayoutFingerprint == TrackFingerprint::Calculate(*track_);
}
const VerificationState& TimeTrial::Verification() const { return verification_; }

bool TimeTrial::ExportVerifiedGhost(VerifiedGhostData& output) const {
    if (track_ == 0 || !ready_ || !verification_.hasSavedGhost || !verification_.isVerifiedForPlayableExport ||
        verification_.verifiedLayoutRevision != track_->LayoutRevision() ||
        verification_.verifiedLayoutFingerprint != TrackFingerprint::Calculate(*track_)) {
        return false;
    }

    VerifiedGhostData exported;
    if (!ghostReplay_.ExportVerified(exported)) return false;
    exported.layoutFingerprint = TrackFingerprint::Calculate(*track_);
    output = exported;
    return true;
}

bool TimeTrial::ImportVerifiedGhost(const VerifiedGhostData& input) {
    if (track_ == 0 || !ready_ || input.layoutFingerprint != TrackFingerprint::Calculate(*track_)) return false;
    if (!ghostReplay_.ImportVerified(input)) return false;

    verification_.hasSavedGhost = ghostReplay_.HasVerified();
    verification_.isVerifiedForPlayableExport = verification_.hasSavedGhost;
    verification_.verifiedLayoutRevision = track_->LayoutRevision();
    verification_.verifiedLayoutFingerprint = input.layoutFingerprint;
    return true;
}

std::uint64_t TimeTrial::ActiveLayoutFingerprint() const {
    return track_ == 0 ? 0 : TrackFingerprint::Calculate(*track_);
}

RaceCar TimeTrial::GhostCar() const {
    return HasVerifiedGhost() ? ghostReplay_.SampleAt(totalTime_, vehicle_.Car()) : vehicle_.Car();
}

const char* TimeTrial::StatusMessage() const { return statusMessage_; }

void TimeTrial::FixedUpdate(float deltaTime, const RaceInput& input) {
    TrackVehicleSurfaceQuery surfaceQuery(*track_);
    const VehicleStepResult vehicleStep = vehicle_.Step(surfaceQuery, deltaTime, input);
    if (vehicleStep.guardrailImpact) {
        statusMessage_ = "Guardrail impact.";
    } else if (!vehicle_.IsOnTrack()) {
        statusMessage_ = "Airborne / off track: steering and traction reduced.";
    }

    const RaceCar& car = vehicle_.Car();
    ghostReplay_.Capture(car, totalTime_);

    currentLapTime_ += deltaTime;
    totalTime_ += deltaTime;

    const TrackPiece* startPiece = track_->GetPiece(track_->StartFinishPieceId());
    if (startPiece == 0) return;
    const TrackConnector start = startPiece->EntryConnector();
    const RaceVector3 startDirection = Forward(StartHeading());
    const float dx = car.position.x - static_cast<float>(start.position.x);
    const float dz = car.position.z - static_cast<float>(start.position.z);
    const float projection = dx * startDirection.x + dz * startDirection.z;
    const float lateral = std::fabs(dx * -startDirection.z + dz * startDirection.x);

    if (projection > 4.0f) hasLeftStart_ = true;
    if (hasLeftStart_ && previousStartProjection_ < 0.0f && projection >= 0.0f && lateral < 2.0f && car.speed > 1.0f) {
        CompleteLap();
    }
    previousStartProjection_ = projection;
}

void TimeTrial::CompleteLap() {
    ++completedLaps_;
    if (bestLapTime_ == 0.0f || currentLapTime_ < bestLapTime_) bestLapTime_ = currentLapTime_;
    if (completedLaps_ >= 3) {
        finished_ = true;
        if (ghostReplay_.PromoteCandidateIfFaster(totalTime_)) {
            verification_.hasSavedGhost = ghostReplay_.HasVerified();
            verification_.isVerifiedForPlayableExport = verification_.hasSavedGhost;
            verification_.verifiedLayoutRevision = track_->LayoutRevision();
            verification_.verifiedLayoutFingerprint = TrackFingerprint::Calculate(*track_);
        }
        statusMessage_ = ghostReplay_.HasVerified()
            ? "Three laps complete. Ghost verified. Press R to race it."
            : "Three laps complete, but this run could not be verified for replay.";
        return;
    }
    currentLapTime_ = 0.0f;
    statusMessage_ = "Lap complete.";
}

void TimeTrial::ResetCarToStart() {
    vehicle_.ResetPose(StartPosition(), StartHeading());
    previousStartProjection_ = 0.0f;
}

RaceVector3 TimeTrial::StartPosition() const {
    const TrackPiece* startPiece = track_->GetPiece(track_->StartFinishPieceId());
    const TrackConnector start = startPiece->EntryConnector();
    return RaceVector3{static_cast<float>(start.position.x), static_cast<float>(start.position.y),
                       static_cast<float>(start.position.z)};
}

float TimeTrial::StartHeading() const {
    const TrackPiece* startPiece = track_->GetPiece(track_->StartFinishPieceId());
    const float heading = HeadingRadians(startPiece->EntryConnector().heading);
    return track_->SelectedRaceDirection() == RaceDirection::Forward ? heading : heading + kPi;
}

const char* FormatRaceTime(float seconds) {
    static char text[24];
    const int wholeSeconds = static_cast<int>(seconds);
    const int minutes = wholeSeconds / 60;
    const int remainingSeconds = wholeSeconds % 60;
    const int centiseconds = static_cast<int>((seconds - static_cast<float>(wholeSeconds)) * 100.0f);
    std::snprintf(text, sizeof(text), "%02i:%02i.%02i", minutes, remainingSeconds, centiseconds);
    return text;
}
