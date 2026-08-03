#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// Describes how confidently a reference time maps to the player's current
// progress. ExactRoute is used when the shared or selected route agrees;
// NormalizedProgress is the branch fallback planned for the live-delta HUD.
enum class TimingComparisonQuality {
    Unavailable,
    ExactRoute,
    NormalizedProgress,
};

struct TimingTracePoint {
    float progress;
    float elapsedTime;
    std::uint32_t routeVariantId;

    TimingTracePoint()
        : progress(0.0f), elapsedTime(0.0f), routeVariantId(0u) {}

    TimingTracePoint(float pointProgress, float pointElapsedTime,
                     std::uint32_t pointRouteVariantId)
        : progress(pointProgress), elapsedTime(pointElapsedTime),
          routeVariantId(pointRouteVariantId) {}
};

struct TimingTraceLookup {
    bool available;
    float elapsedTime;
    TimingComparisonQuality quality;

    TimingTraceLookup()
        : available(false), elapsedTime(0.0f),
          quality(TimingComparisonQuality::Unavailable) {}
};

// A monotonic progress-to-time curve. Repeated or backwards progress samples
// are ignored so waiting, reversing, and recovery never make a reference trace
// ambiguous. The first time a progress point is reached remains authoritative.
class TimingTrace {
public:
    TimingTrace() : points_() {}

    void Reset() { points_.clear(); }

    void Begin(std::uint32_t routeVariantId = 0u) {
        points_.clear();
        points_.push_back(TimingTracePoint(0.0f, 0.0f, routeVariantId));
    }

    bool Append(float progress, float elapsedTime, std::uint32_t routeVariantId) {
        if (!std::isfinite(progress) || !std::isfinite(elapsedTime)) return false;
        progress = std::max(0.0f, std::min(1.0f, progress));
        elapsedTime = std::max(0.0f, elapsedTime);
        if (points_.empty()) Begin(routeVariantId);

        TimingTracePoint& last = points_.back();
        if (progress + ProgressEpsilon() < last.progress ||
            elapsedTime + TimeEpsilon() < last.elapsedTime) {
            return false;
        }

        if (progress <= last.progress + ProgressEpsilon()) {
            if (last.routeVariantId == 0u && routeVariantId != 0u) {
                last.routeVariantId = routeVariantId;
            }
            return false;
        }

        points_.push_back(TimingTracePoint(progress, elapsedTime, routeVariantId));
        return true;
    }

    void Complete(float elapsedTime, std::uint32_t routeVariantId) {
        if (!std::isfinite(elapsedTime)) return;
        elapsedTime = std::max(0.0f, elapsedTime);
        if (points_.empty()) Begin(routeVariantId);

        TimingTracePoint& last = points_.back();
        if (last.progress >= 1.0f - ProgressEpsilon()) {
            last.progress = 1.0f;
            if (elapsedTime >= last.elapsedTime) last.elapsedTime = elapsedTime;
            if (last.routeVariantId == 0u && routeVariantId != 0u) {
                last.routeVariantId = routeVariantId;
            }
            return;
        }
        Append(1.0f, elapsedTime, routeVariantId);
    }

    bool IsComplete() const {
        return points_.size() >= 2u &&
               points_.front().progress <= ProgressEpsilon() &&
               points_.back().progress >= 1.0f - ProgressEpsilon();
    }

    bool Empty() const { return points_.empty(); }
    const std::vector<TimingTracePoint>& Points() const { return points_; }

    TimingTraceLookup Lookup(float progress, std::uint32_t routeVariantId,
                             bool allowNormalizedFallback) const {
        TimingTraceLookup result;
        if (!IsComplete() || !std::isfinite(progress)) return result;
        progress = std::max(0.0f, std::min(1.0f, progress));

        std::size_t upperIndex = 0u;
        while (upperIndex < points_.size() &&
               points_[upperIndex].progress < progress) {
            ++upperIndex;
        }

        if (upperIndex == 0u) {
            result.elapsedTime = points_.front().elapsedTime;
            result.quality = RouteMatches(points_.front().routeVariantId, routeVariantId)
                ? TimingComparisonQuality::ExactRoute
                : (allowNormalizedFallback
                    ? TimingComparisonQuality::NormalizedProgress
                    : TimingComparisonQuality::Unavailable);
        } else if (upperIndex >= points_.size()) {
            result.elapsedTime = points_.back().elapsedTime;
            result.quality = RouteMatches(points_.back().routeVariantId, routeVariantId)
                ? TimingComparisonQuality::ExactRoute
                : (allowNormalizedFallback
                    ? TimingComparisonQuality::NormalizedProgress
                    : TimingComparisonQuality::Unavailable);
        } else {
            const TimingTracePoint& lower = points_[upperIndex - 1u];
            const TimingTracePoint& upper = points_[upperIndex];
            const float span = upper.progress - lower.progress;
            const float amount = span > ProgressEpsilon()
                ? (progress - lower.progress) / span
                : 0.0f;
            result.elapsedTime = lower.elapsedTime +
                (upper.elapsedTime - lower.elapsedTime) * amount;
            const bool exact = RouteMatches(lower.routeVariantId, routeVariantId) &&
                               RouteMatches(upper.routeVariantId, routeVariantId);
            result.quality = exact
                ? TimingComparisonQuality::ExactRoute
                : (allowNormalizedFallback
                    ? TimingComparisonQuality::NormalizedProgress
                    : TimingComparisonQuality::Unavailable);
        }

        result.available = result.quality != TimingComparisonQuality::Unavailable;
        if (!result.available) result.elapsedTime = 0.0f;
        return result;
    }

private:
    static bool RouteMatches(std::uint32_t storedRouteVariantId,
                             std::uint32_t requestedRouteVariantId) {
        if (storedRouteVariantId == 0u) return true;
        if (requestedRouteVariantId == 0u) return false;
        return storedRouteVariantId == requestedRouteVariantId;
    }

    static float ProgressEpsilon() { return 0.0001f; }
    static float TimeEpsilon() { return 0.0001f; }

    std::vector<TimingTracePoint> points_;
};

struct SectorTimingProfile {
    bool valid;
    int sectorIndex;
    float sectorTime;
    std::uint32_t routeVariantId;
    TimingTrace trace;

    SectorTimingProfile()
        : valid(false), sectorIndex(-1), sectorTime(0.0f),
          routeVariantId(0u), trace() {}
};

struct LapTimingProfile {
    bool valid;
    float lapTime;
    std::uint32_t routeVariantId;
    std::array<float, 3> sectorTimes;
    TimingTrace trace;

    LapTimingProfile()
        : valid(false), lapTime(0.0f), routeVariantId(0u),
          sectorTimes(), trace() {
        sectorTimes.fill(0.0f);
    }
};

struct RaceTimingReferences {
    LapTimingProfile bestLap;
    std::array<SectorTimingProfile, 3> bestSectors;
};
