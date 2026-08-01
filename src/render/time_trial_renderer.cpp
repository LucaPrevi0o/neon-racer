#include "time_trial_renderer.hpp"

#include "car_renderer.hpp"
#include "race_scene.hpp"
#include "../race/time_trial.hpp"
#include "../track/track.hpp"
#include "../ui/neon.hpp"

#include <cmath>

namespace {

Vector3 LerpVector3(Vector3 from, Vector3 to, float amount) {
    return Vector3{from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
                   from.z + (to.z - from.z) * amount};
}

Vector3 DesiredCameraTarget(const RaceCar& car) {
    const RaceVector3 visualCenter = RaceCarVisualCenter(car);
    return Vector3{visualCenter.x, visualCenter.y, visualCenter.z};
}

Vector3 DesiredCameraPosition(const RaceCar& car) {
    const RaceVector3 visualCenter = RaceCarVisualCenter(car);
    return Vector3{visualCenter.x - std::cos(car.headingRadians) * 8.0f,
                   visualCenter.y + 4.5f,
                   visualCenter.z - std::sin(car.headingRadians) * 8.0f};
}

} // namespace

TimeTrialRenderer::TimeTrialRenderer() : camera_{} {
    camera_.position = Vector3{0.0f, 5.0f, 10.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 55.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void TimeTrialRenderer::Update(const TimeTrial& timeTrial) {
    if (!timeTrial.IsReady()) return;

    const RaceCar& car = timeTrial.Car();
    camera_.position = LerpVector3(camera_.position, DesiredCameraPosition(car), 0.10f);
    camera_.target = LerpVector3(camera_.target, DesiredCameraTarget(car), 0.14f);
}

void TimeTrialRenderer::SnapTo(const TimeTrial& timeTrial) {
    if (!timeTrial.IsReady()) return;
    camera_.position = DesiredCameraPosition(timeTrial.Car());
    camera_.target = DesiredCameraTarget(timeTrial.Car());
}

void TimeTrialRenderer::Draw(const Track& track, const TimeTrial& timeTrial) const {
    BeginMode3D(camera_);
    DrawGridFloor(40, 1.0f, Fade(Neon::Cyan, 0.15f));
    DrawRaceTrackScene(track);
    if (timeTrial.IsReady() && timeTrial.HasVerifiedGhost()) DrawGhostRaceCar(timeTrial.GhostCar());
    if (timeTrial.IsReady()) DrawRaceCar(timeTrial.Car());
    EndMode3D();

    DrawHud(timeTrial);
}

void TimeTrialRenderer::DrawHud(const TimeTrial& timeTrial) const {
    Neon::DrawHeader("NEON RACER  //  TIME TRIAL", GetScreenWidth());
    if (!timeTrial.IsReady()) {
        Neon::DrawOverlayPanel(Rectangle{28.0f, 80.0f, 300.0f, 112.0f});
        DrawText("TRACK NOT READY", 46, 100, 20, Neon::Orange);
        DrawText(timeTrial.StatusMessage(), 46, 134, 15, Fade(RAYWHITE, 0.80f));
        DrawText("Press Tab to return.", 46, 166, 15, Neon::Yellow);
        return;
    }

    Neon::DrawOverlayPanel(Rectangle{28.0f, 80.0f, 300.0f, 132.0f});
    DrawText(TextFormat("LAP      %i / 3", timeTrial.CurrentLap()), 46, 100, 20, Neon::Cyan);
    DrawText(TextFormat("TIME     %s", FormatRaceTime(timeTrial.CurrentLapTime())), 46, 130, 18, RAYWHITE);
    DrawText(TextFormat("BEST     %s",
                        timeTrial.BestLapTime() > 0.0f ? FormatRaceTime(timeTrial.BestLapTime()) : "--:--.--"),
             46, 156, 18, Neon::Green);
    DrawText(TextFormat("SPEED    %03i km/h", static_cast<int>(std::fabs(timeTrial.Car().speed) * 8.0f)), 46,
             182, 18, Neon::Pink);
}
