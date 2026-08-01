#pragma once

#include <raylib.h>

class Track;
class TimeTrial;

// Raylib-facing presentation for the time-trial screen. It reads the race
// coordinator but never changes simulation, lap, or verification policy.
class TimeTrialRenderer {
public:
    TimeTrialRenderer();

    void Update(const TimeTrial& timeTrial);
    void SnapTo(const TimeTrial& timeTrial);
    void Draw(const Track& track, const TimeTrial& timeTrial) const;

private:
    void DrawHud(const TimeTrial& timeTrial) const;

    Camera3D camera_;
};
