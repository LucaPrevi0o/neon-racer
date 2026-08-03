#include "time_trial_renderer.hpp"

#include "car_renderer.hpp"
#include "race_scene.hpp"
#include "../race/time_trial.hpp"
#include "../track/track.hpp"
#include "../ui/neon.hpp"

#include <cmath>

namespace {

const char* kUnavailableTime = "--:--.---";
const float kSplitBannerDuration = 2.25f;

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

void DrawHudRow(const char* label, const char* value, int x, int y, Color valueColor) {
    DrawText(label, x, y, 16, Fade(RAYWHITE, 0.66f));
    DrawText(value, x + 118, y - 1, 18, valueColor);
}

void DrawPanelTitle(const char* title, int x, int y, Color color) {
    DrawText(title, x, y, 18, color);
    DrawRectangle(x, y + 25, 72, 2, Fade(color, 0.75f));
}

} // namespace

TimeTrialRenderer::TimeTrialRenderer()
    : camera_{}, observedSplitSequence_(0u), splitBannerSeconds_(0.0f),
      splitBannerSector_(0), splitBannerTime_(0.0f), splitBannerWasBest_(false) {
    camera_.position = Vector3{0.0f, 5.0f, 10.0f};
    camera_.target = Vector3{0.0f, 0.0f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 55.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void TimeTrialRenderer::Update(const TimeTrial& timeTrial) {
    if (!timeTrial.IsReady()) {
        observedSplitSequence_ = 0u;
        splitBannerSeconds_ = 0.0f;
        return;
    }

    const RaceCar& car = timeTrial.Car();
    camera_.position = LerpVector3(camera_.position, DesiredCameraPosition(car), 0.10f);
    camera_.target = LerpVector3(camera_.target, DesiredCameraTarget(car), 0.14f);

    const RaceTimingSnapshot& timing = timeTrial.Timing();
    if (timing.splitSequence < observedSplitSequence_) {
        observedSplitSequence_ = timing.splitSequence;
        splitBannerSeconds_ = 0.0f;
    } else if (timing.splitSequence != observedSplitSequence_) {
        observedSplitSequence_ = timing.splitSequence;
        splitBannerSector_ = timing.lastCompletedSectorIndex + 1;
        splitBannerTime_ = timing.lastCompletedSectorTime;
        splitBannerWasBest_ = timing.lastCompletedSectorImprovedBest;
        splitBannerSeconds_ = kSplitBannerDuration;
    }

    if (splitBannerSeconds_ > 0.0f) {
        splitBannerSeconds_ -= GetFrameTime();
        if (splitBannerSeconds_ < 0.0f) splitBannerSeconds_ = 0.0f;
    }
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

    const RaceTimingSnapshot& timing = timeTrial.Timing();
    const int screenWidth = GetScreenWidth();
    const int leftX = 28;
    const int rightX = screenWidth - 378;

    Neon::DrawOverlayPanel(Rectangle{static_cast<float>(leftX), 80.0f, 310.0f, 206.0f});
    Neon::DrawOverlayPanel(Rectangle{static_cast<float>(rightX), 80.0f, 350.0f, 246.0f});

    DrawPanelTitle("RACE", leftX + 18, 98, Neon::Cyan);
    DrawHudRow("LAP", TextFormat("%i / 3", timeTrial.CurrentLap()),
               leftX + 18, 136, Neon::Cyan);
    DrawHudRow("SECTOR", timing.sectorsAvailable ?
                   TextFormat("%i / 3", timeTrial.CurrentSector()) : "-- / 3",
               leftX + 18, 165, timing.sectorsAvailable ? Neon::Yellow : Fade(RAYWHITE, 0.42f));
    DrawHudRow("SPEED", TextFormat("%03i km/h",
                   static_cast<int>(std::fabs(timeTrial.Car().speed) * 8.0f)),
               leftX + 18, 194, Neon::Pink);
    DrawHudRow("TOTAL", FormatRaceTime(timeTrial.TotalTime()),
               leftX + 18, 223, RAYWHITE);
    DrawHudRow("GHOST", timeTrial.HasVerifiedGhost() ? "ACTIVE" : "NO REFERENCE",
               leftX + 18, 252,
               timeTrial.HasVerifiedGhost() ? Neon::Green : Fade(RAYWHITE, 0.48f));

    DrawPanelTitle("TIMING", rightX + 18, 98, Neon::Pink);
    if (timing.sectorsAvailable) {
        const int sectorIndex = timing.currentSectorIndex;
        const float bestSector = timing.bestSectorTimes[static_cast<std::size_t>(sectorIndex)];
        DrawHudRow(TextFormat("SECTOR %i", sectorIndex + 1),
                   FormatRaceTime(timing.currentSectorTime),
                   rightX + 18, 136, Neon::Yellow);
        DrawHudRow(TextFormat("BEST S%i", sectorIndex + 1),
                   bestSector > 0.0f ? FormatRaceTime(bestSector) : kUnavailableTime,
                   rightX + 18, 165, bestSector > 0.0f ? Neon::Green : Fade(RAYWHITE, 0.42f));
    } else {
        DrawHudRow("SECTOR", kUnavailableTime,
                   rightX + 18, 136, Fade(RAYWHITE, 0.42f));
        DrawHudRow("BEST SPLIT", kUnavailableTime,
                   rightX + 18, 165, Fade(RAYWHITE, 0.42f));
    }

    DrawHudRow("LAP", FormatRaceTime(timeTrial.CurrentLapTime()),
               rightX + 18, 206, RAYWHITE);
    DrawHudRow("LAST LAP", timeTrial.LastLapTime() > 0.0f ?
                   FormatRaceTime(timeTrial.LastLapTime()) : kUnavailableTime,
               rightX + 18, 235,
               timeTrial.LastLapTime() > 0.0f ? Neon::Cyan : Fade(RAYWHITE, 0.42f));
    DrawHudRow("BEST LAP", timeTrial.BestLapTime() > 0.0f ?
                   FormatRaceTime(timeTrial.BestLapTime()) : kUnavailableTime,
               rightX + 18, 264,
               timeTrial.BestLapTime() > 0.0f ? Neon::Green : Fade(RAYWHITE, 0.42f));
    DrawHudRow("STATUS", timeTrial.IsPaused() ? "PAUSED" : "RUNNING",
               rightX + 18, 293, timeTrial.IsPaused() ? Neon::Orange : Neon::Cyan);

    DrawSplitBanner();
}

void TimeTrialRenderer::DrawSplitBanner() const {
    if (splitBannerSeconds_ <= 0.0f || splitBannerSector_ < 1 || splitBannerSector_ > 3) return;

    const int width = 364;
    const int x = (GetScreenWidth() - width) / 2;
    const int y = 348;
    const Color accent = splitBannerWasBest_ ? Neon::Green : Neon::Cyan;
    const float fadeAmount = splitBannerSeconds_ < 0.35f ? splitBannerSeconds_ / 0.35f : 1.0f;

    Neon::DrawOverlayPanel(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                     static_cast<float>(width), 82.0f},
                           0.88f * fadeAmount);
    DrawRectangle(x, y, width, 3, Fade(accent, fadeAmount));

    const char* title = TextFormat("SECTOR %i COMPLETE", splitBannerSector_);
    DrawText(title, x + 18, y + 14, 17, Fade(accent, fadeAmount));
    DrawText(FormatRaceTime(splitBannerTime_), x + 18, y + 40, 23,
             Fade(RAYWHITE, fadeAmount));
    DrawText(splitBannerWasBest_ ? "NEW BEST" : "SPLIT RECORDED",
             x + 214, y + 47, 14, Fade(accent, fadeAmount));
}
