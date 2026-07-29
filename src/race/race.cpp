#include "race.hpp"

#include <cmath>
#include <cstdio>

namespace {

const float kFixedStep = 1.0f / 120.0f;
const float kMaxFrameTime = 0.10f;
const float kMaxForwardSpeed = 32.0f;
const float kMaxReverseSpeed = 11.0f;
const float kCarRideHeight = 0.16f;
const float kGhostSampleInterval = 1.0f / 30.0f;
const float kGravity = 18.0f;
const float kSuspensionSpring = 115.0f;
const float kSuspensionDamping = 20.0f;
const float kAirDrag = 0.018f;

float HeadingRadians(Heading heading) {
    switch (heading) {
    case Heading::East: return 0.0f;
    case Heading::South: return PI * 0.5f;
    case Heading::West: return PI;
    case Heading::North: return -PI * 0.5f;
    }
    return 0.0f;
}

Vector3 Forward(float heading) {
    return Vector3{std::cos(heading), 0.0f, std::sin(heading)};
}

Vector3 Add(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 Scale(Vector3 vector, float amount) { return Vector3{vector.x * amount, vector.y * amount, vector.z * amount}; }
float Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3 Cross(Vector3 a, Vector3 b) {
    return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Magnitude(Vector3 vector) { return std::sqrt(Dot(vector, vector)); }
Vector3 Normalize(Vector3 vector, Vector3 fallback) {
    const float length = Magnitude(vector);
    return length > 0.0001f ? Scale(vector, 1.0f / length) : fallback;
}
Vector3 ProjectOnPlane(Vector3 vector, Vector3 normal) { return Add(vector, Scale(normal, -Dot(vector, normal))); }
Vector3 RotateAround(Vector3 vector, Vector3 axis, float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return Add(Add(Scale(vector, cosine), Scale(Cross(axis, vector), sine)),
               Scale(axis, Dot(axis, vector) * (1.0f - cosine)));
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

TimeTrial::TimeTrial()
    : track_(0), car_{Vector3{0.0f, 0.16f, 0.0f}, Vector3{0.0f, 0.0f, 0.0f},
                     Vector3{1.0f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}, 0.0f, 0.0f}, accumulator_(0.0f),
      currentLapTime_(0.0f), bestLapTime_(0.0f), totalTime_(0.0f), previousStartProjection_(0.0f),
      completedLaps_(0), hasLeftStart_(false), paused_(false), finished_(false), ready_(false),
      onTrack_(true), surfaceMaterial_(SurfaceMaterial::Regular),
      nextGhostSampleTime_(0.0f), verifiedGhostDuration_(0.0f),
      statusMessage_("Open a race-ready track in the editor.") {
}

void TimeTrial::Start(const Track& track) {
    if (verification_.hasSavedGhost && verification_.verifiedLayoutRevision != track.LayoutRevision()) {
        verifiedGhostSamples_.clear();
        verifiedGhostDuration_ = 0.0f;
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

void TimeTrial::Update(float frameTime) {
    if (!ready_) return;
    if (IsKeyPressed(KEY_P) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) TogglePause();
    if (IsKeyPressed(KEY_R) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT)) Reset();
    if (paused_ || finished_) return;

    accumulator_ += std::fmin(frameTime, kMaxFrameTime);
    while (accumulator_ >= kFixedStep) {
        FixedUpdate(kFixedStep);
        accumulator_ -= kFixedStep;
    }
}

void TimeTrial::Reset() {
    if (!ready_) return;
    accumulator_ = 0.0f;
    currentLapTime_ = 0.0f;
    bestLapTime_ = 0.0f;
    totalTime_ = 0.0f;
    recordingSamples_.clear();
    nextGhostSampleTime_ = 0.0f;
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
const RaceCar& TimeTrial::Car() const { return car_; }
bool TimeTrial::IsOnTrack() const { return onTrack_; }
SurfaceMaterial TimeTrial::CurrentSurfaceMaterial() const { return surfaceMaterial_; }
bool TimeTrial::HasVerifiedGhost() const { return verification_.isVerifiedForPlayableExport; }
const VerificationState& TimeTrial::Verification() const { return verification_; }

RaceCar TimeTrial::GhostCar() const {
    if (verifiedGhostSamples_.empty() || verifiedGhostDuration_ <= 0.0f) return car_;
    const float playbackTime = std::fmod(totalTime_, verifiedGhostDuration_);
    for (std::size_t index = 1; index < verifiedGhostSamples_.size(); ++index) {
        const GhostSample& next = verifiedGhostSamples_[index];
        if (next.time < playbackTime) continue;
        const GhostSample& previous = verifiedGhostSamples_[index - 1];
        const float span = next.time - previous.time;
        const float amount = span > 0.0f ? (playbackTime - previous.time) / span : 0.0f;
        return RaceCar{Vector3{previous.car.position.x + (next.car.position.x - previous.car.position.x) * amount,
                               previous.car.position.y + (next.car.position.y - previous.car.position.y) * amount,
                               previous.car.position.z + (next.car.position.z - previous.car.position.z) * amount},
                       Vector3{previous.car.velocity.x + (next.car.velocity.x - previous.car.velocity.x) * amount,
                               previous.car.velocity.y + (next.car.velocity.y - previous.car.velocity.y) * amount,
                               previous.car.velocity.z + (next.car.velocity.z - previous.car.velocity.z) * amount},
                       Normalize(Vector3{previous.car.forward.x + (next.car.forward.x - previous.car.forward.x) * amount,
                                         previous.car.forward.y + (next.car.forward.y - previous.car.forward.y) * amount,
                                         previous.car.forward.z + (next.car.forward.z - previous.car.forward.z) * amount},
                                 previous.car.forward),
                       Normalize(Vector3{previous.car.up.x + (next.car.up.x - previous.car.up.x) * amount,
                                         previous.car.up.y + (next.car.up.y - previous.car.up.y) * amount,
                                         previous.car.up.z + (next.car.up.z - previous.car.up.z) * amount}, previous.car.up),
                       previous.car.headingRadians + (next.car.headingRadians - previous.car.headingRadians) * amount,
                       previous.car.speed + (next.car.speed - previous.car.speed) * amount};
    }
    return verifiedGhostSamples_.back().car;
}
const char* TimeTrial::StatusMessage() const { return statusMessage_; }

void TimeTrial::FixedUpdate(float deltaTime) {
    float steering = 0.0f;
    float accelerate = 0.0f;
    float brake = 0.0f;
    float reverse = 0.0f;

    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) steering -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) steering += 1.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) accelerate = 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) brake = 1.0f;
    if (IsKeyDown(KEY_X)) reverse = 1.0f;

    if (IsGamepadAvailable(0)) {
        const float stick = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        const float rightTrigger = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER);
        const float leftTrigger = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER);
        if (std::fabs(stick) > 0.12f) steering = stick;
        if (rightTrigger > 0.05f) accelerate = rightTrigger;
        if (leftTrigger > 0.05f) brake = leftTrigger;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) accelerate = 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) brake = 1.0f;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) reverse = 1.0f;
    }

    const TrackContact contact = track_->QuerySurface(car_.position.x, car_.position.y, car_.position.z, 1.4f);
    const float grip = onTrack_ ? GripFor(surfaceMaterial_) : 0.20f;
    Vector3 acceleration = Vector3{0.0f, -kGravity, 0.0f};

    if (contact.found) {
        const Vector3 normal = Normalize(Vector3{contact.surface.normalX, contact.surface.normalY, contact.surface.normalZ},
                                         Vector3{0.0f, 1.0f, 0.0f});
        const Vector3 surfacePoint = Vector3{contact.surface.x, contact.surface.y, contact.surface.z};
        const float height = Dot(Add(car_.position, Scale(surfacePoint, -1.0f)), normal);
        const float normalVelocity = Dot(car_.velocity, normal);
        const float support = std::max(0.0f, (kCarRideHeight - height) * kSuspensionSpring -
                                             normalVelocity * kSuspensionDamping);
        acceleration = Add(acceleration, Scale(normal, support));

        const Vector3 roadForward = Normalize(Vector3{contact.surface.tangentX, contact.surface.tangentY,
                                                       contact.surface.tangentZ}, car_.forward);
        car_.forward = Normalize(ProjectOnPlane(car_.forward, normal), roadForward);
        const float forwardSpeed = Dot(car_.velocity, car_.forward);
        const float steeringRate = -steering * 2.35f * grip * std::min(1.0f, std::fabs(forwardSpeed) / 8.0f) *
                                   (forwardSpeed < 0.0f ? -1.0f : 1.0f);
        car_.forward = Normalize(RotateAround(car_.forward, normal, steeringRate * deltaTime), roadForward);
        car_.up = normal;

        const Vector3 right = Normalize(Cross(car_.forward, normal), Vector3{0.0f, 0.0f, 1.0f});
        const float lateralSpeed = Dot(car_.velocity, right);
        const float lateralForce = std::max(-kGravity * grip, std::min(kGravity * grip, -lateralSpeed / deltaTime));
        acceleration = Add(acceleration, Scale(right, lateralForce));

        const float driveInput = accelerate - reverse;
        const float driveForce = driveInput >= 0.0f ? 22.0f : 14.0f;
        if (std::fabs(forwardSpeed) < (driveInput >= 0.0f ? kMaxForwardSpeed : kMaxReverseSpeed)) {
            acceleration = Add(acceleration, Scale(car_.forward, driveInput * driveForce * DriveFor(contact.surface.material)));
        }
        if (brake > 0.0f) acceleration = Add(acceleration, Scale(car_.velocity, -brake * 8.0f * grip));
        acceleration = Add(acceleration, Scale(car_.velocity, -0.75f));
        if (contact.guardrailHit) {
            const Vector3 edgePoint = surfacePoint;
            const Vector3 outward = Normalize(ProjectOnPlane(Add(car_.position, Scale(edgePoint, -1.0f)), normal), right);
            const float outwardSpeed = Dot(car_.velocity, outward);
            if (outwardSpeed > 0.0f) car_.velocity = Add(car_.velocity, Scale(outward, -outwardSpeed));
            acceleration = Add(acceleration, Scale(outward, -65.0f));
            statusMessage_ = "Guardrail impact.";
        }
        surfaceMaterial_ = contact.surface.material;
        onTrack_ = true;
    } else {
        onTrack_ = false;
        car_.up = Normalize(Add(Scale(car_.up, 0.98f), Vector3{0.0f, 0.02f, 0.0f}), Vector3{0.0f, 1.0f, 0.0f});
        statusMessage_ = "Airborne / off track: steering and traction reduced.";
    }

    const float velocityLength = Magnitude(car_.velocity);
    acceleration = Add(acceleration, Scale(car_.velocity, -kAirDrag * velocityLength));
    car_.velocity = Add(car_.velocity, Scale(acceleration, deltaTime));
    car_.position = Add(car_.position, Scale(car_.velocity, deltaTime));
    car_.headingRadians = std::atan2(car_.forward.z, car_.forward.x);
    car_.speed = Dot(car_.velocity, car_.forward);

    if (totalTime_ >= nextGhostSampleTime_) {
        recordingSamples_.push_back(GhostSample{car_, totalTime_});
        nextGhostSampleTime_ += kGhostSampleInterval;
    }

    currentLapTime_ += deltaTime;
    totalTime_ += deltaTime;

    const TrackPiece* startPiece = track_->GetPiece(track_->StartFinishPieceId());
    if (startPiece == 0) return;
    const TrackConnector start = startPiece->EntryConnector();
    const Vector3 startDirection = Forward(StartHeading());
    const float dx = car_.position.x - static_cast<float>(start.position.x);
    const float dz = car_.position.z - static_cast<float>(start.position.z);
    const float projection = dx * startDirection.x + dz * startDirection.z;
    const float lateral = std::fabs(dx * -startDirection.z + dz * startDirection.x);

    if (projection > 4.0f) hasLeftStart_ = true;
    if (hasLeftStart_ && previousStartProjection_ < 0.0f && projection >= 0.0f && lateral < 2.0f && car_.speed > 1.0f) {
        CompleteLap();
    }
    previousStartProjection_ = projection;
}

void TimeTrial::CompleteLap() {
    ++completedLaps_;
    if (bestLapTime_ == 0.0f || currentLapTime_ < bestLapTime_) bestLapTime_ = currentLapTime_;
    if (completedLaps_ >= 3) {
        finished_ = true;
        if (verifiedGhostSamples_.empty() || totalTime_ < verifiedGhostDuration_) {
            verifiedGhostSamples_ = recordingSamples_;
            verifiedGhostDuration_ = totalTime_;
            verification_.hasSavedGhost = !verifiedGhostSamples_.empty();
            verification_.isVerifiedForPlayableExport = verification_.hasSavedGhost;
            verification_.verifiedLayoutRevision = track_->LayoutRevision();
        }
        statusMessage_ = "Three laps complete. Ghost verified. Press R to race it.";
        return;
    }
    currentLapTime_ = 0.0f;
    statusMessage_ = "Lap complete.";
}

void TimeTrial::ResetCarToStart() {
    car_.position = StartPosition();
    car_.headingRadians = StartHeading();
    car_.velocity = Vector3{0.0f, 0.0f, 0.0f};
    car_.forward = Forward(car_.headingRadians);
    car_.up = Vector3{0.0f, 1.0f, 0.0f};
    car_.speed = 0.0f;
    previousStartProjection_ = 0.0f;
}

Vector3 TimeTrial::StartPosition() const {
    const TrackPiece* startPiece = track_->GetPiece(track_->StartFinishPieceId());
    const TrackConnector start = startPiece->EntryConnector();
    return Vector3{static_cast<float>(start.position.x), static_cast<float>(start.position.y) + kCarRideHeight,
                   static_cast<float>(start.position.z)};
}

float TimeTrial::StartHeading() const {
    const TrackPiece* startPiece = track_->GetPiece(track_->StartFinishPieceId());
    const float heading = HeadingRadians(startPiece->EntryConnector().heading);
    return track_->SelectedRaceDirection() == RaceDirection::Forward ? heading : heading + PI;
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
