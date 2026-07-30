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
    const Vector3 behind = Vector3{-std::cos(car.headingRadians) * 8.0f, 4.5f,
                                   -std::sin(car.headingRadians) * 8.0f};
    const Vector3 desiredPosition = Vector3{car.position.x + behind.x, car.position.y + behind.y,
                                            car.position.z + behind.z};
    const Vector3 desiredTarget = Vector3{car.position.x, car.position.y + 0.2f, car.position.z};
    camera_.position = LerpVector3(camera_.position, desiredPosition, 0.10f);
    camera_.target = LerpVector3(camera_.target, desiredTarget, 0.14f);
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
    Neon::DrawOverlayPanel(Rectangle{28.0f, 80.0f, 300.0f, 198.0f});
    if (!timeTrial.IsReady()) {
        DrawText("TRACK NOT READY", 46, 100, 20, Neon::Orange);
        DrawText(timeTrial.StatusMessage(), 46, 134, 15, Fade(RAYWHITE, 0.80f));
        DrawText("Press Tab to return to the editor.", 46, 166, 15, Neon::Yellow);
        return;
    }

    DrawText(TextFormat("LAP %i / 3", timeTrial.CurrentLap()), 46, 100, 21, Neon::Cyan);
    DrawText(TextFormat("CURRENT  %s", FormatRaceTime(timeTrial.CurrentLapTime())), 46, 132, 18, RAYWHITE);
    DrawText(TextFormat("BEST     %s",
                        timeTrial.BestLapTime() > 0.0f ? FormatRaceTime(timeTrial.BestLapTime()) : "--:--.--"),
             46, 158, 18, Neon::Green);
    DrawText(TextFormat("TOTAL    %s", FormatRaceTime(timeTrial.TotalTime())), 46, 184, 18, RAYWHITE);
    DrawText(TextFormat("SPEED    %03i km/h", static_cast<int>(std::fabs(timeTrial.Car().speed) * 8.0f)), 46,
             210, 18, Neon::Pink);

    const char* surface = timeTrial.CurrentSurfaceMaterial() == SurfaceMaterial::Slippery
                              ? "SLIPPERY"
                              : timeTrial.CurrentSurfaceMaterial() == SurfaceMaterial::HighResistance
                                    ? "HIGH RESISTANCE"
                                    : "REGULAR";
    DrawText(TextFormat("SURFACE  %s", timeTrial.IsOnTrack() ? surface : "OFF TRACK"), 46, 244, 14,
             timeTrial.IsOnTrack() ? Neon::Cyan : Neon::Orange);
    DrawText(timeTrial.StatusMessage(), 46, 264, 14,
             timeTrial.IsPaused() ? Neon::Yellow : Fade(RAYWHITE, 0.78f));
    if (timeTrial.HasVerifiedGhost()) DrawText("VERIFIED GHOST ACTIVE", 46, 282, 13, Neon::Green);
    Neon::DrawOverlayPanel(Rectangle{28.0f, 296.0f, 420.0f, 34.0f}, 0.72f);
    DrawText("WASD / arrows drive  X reverse  R reset  P pause", 42, 305, 16, Neon::Yellow);
}
