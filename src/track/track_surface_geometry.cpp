#include "track.hpp"

#include <algorithm>
#include <cmath>

namespace {
const float kPi = 3.14159265359f;
float Clamp(float value, float minimum, float maximum) { return std::max(minimum, std::min(maximum, value)); }
float Length(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }
GridPosition Right(Heading heading) {
    const int value = (static_cast<int>(heading) + 1) % 4;
    switch (static_cast<Heading>(value)) {
    case Heading::North: return GridPosition{0, 0, -1}; case Heading::East: return GridPosition{1, 0, 0};
    case Heading::South: return GridPosition{0, 0, 1}; case Heading::West: return GridPosition{-1, 0, 0};
    }
    return GridPosition{0, 0, 0};
}
} // namespace

float TrackPiece::WidthAt(float progress) const {
    const float clamped = Clamp(progress, 0.0f, 1.0f);
    return static_cast<float>(width) + static_cast<float>(exitWidth - width) * clamped;
}

std::vector<TrackSurfaceSample> TrackPiece::SurfaceSamples(int subdivisions) const {
    int count = subdivisions;
    if (count <= 0) {
        if (type == TrackPieceType::Straight) count = std::max(2, length);
        // The wide corkscrew is sampled densely enough that surface queries
        // cannot skip an inverted or side-facing contact frame.
        else if (type == TrackPieceType::Twist) count = std::max(128, length * 2);
        else if (type == TrackPieceType::Loop) count = std::max(24, curveRadius * 12);
        else count = std::max(12, curveRadius * 8);
    }
    const std::vector<TrackPathPoint> points = PathPoints(count); std::vector<TrackSurfaceSample> samples;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const TrackPathPoint& current = points[index]; const TrackPathPoint& before = points[index == 0 ? index : index - 1]; const TrackPathPoint& after = points[index + 1 < points.size() ? index + 1 : index];
        float tx = after.x - before.x, ty = after.y - before.y, tz = after.z - before.z; float length = Length(tx, ty, tz);
        if (length > 0.0001f) { tx /= length; ty /= length; tz /= length; } else { tx = 1.0f; ty = tz = 0.0f; }
        const float progress = points.size() <= 1 ? 0.0f : static_cast<float>(index) / static_cast<float>(points.size() - 1);
        float nx = 0.0f;
        float ny = 0.0f;
        float nz = 0.0f;
        const bool legacyFlatTwist = type == TrackPieceType::Twist &&
            TrackLimits::IsLegacyFlatTwistRadius(curveRadius);
        if (type == TrackPieceType::Twist && !legacyFlatTwist) {
            const GridPosition side = Right(entryHeading);
            const float angle = TrackLimits::TwistAngle(progress);
            // Face inward toward the corkscrew axis. Removing its tangent
            // component keeps the road frame orthogonal when offsets or a
            // custom endpoint elevation are present.
            nx = static_cast<float>(side.x) * std::sin(angle);
            ny = std::cos(angle);
            nz = static_cast<float>(side.z) * std::sin(angle);
            const float tangentProjection = nx * tx + ny * ty + nz * tz;
            nx -= tx * tangentProjection;
            ny -= ty * tangentProjection;
            nz -= tz * tangentProjection;
            length = Length(nx, ny, nz);
            if (length > 0.0001f) { nx /= length; ny /= length; nz /= length; }
            else { nx = 0.0f; ny = 1.0f; nz = 0.0f; }
        } else {
            const float horizontal = std::sqrt(tx * tx + tz * tz);
            nx = -ty * tx;
            ny = horizontal;
            nz = -ty * tz;
            length = Length(nx, ny, nz);
            if (length > 0.0001f) { nx /= length; ny /= length; nz /= length; }
            if (type == TrackPieceType::Loop) {
                const GridPosition lateral = Right(entryHeading);
                nx = -static_cast<float>(lateral.z) * ty;
                ny = static_cast<float>(lateral.z) * tx - static_cast<float>(lateral.x) * tz;
                nz = static_cast<float>(lateral.x) * ty;
                length = Length(nx, ny, nz);
                if (length > 0.0001f) { nx /= length; ny /= length; nz /= length; }
            }
            const float bank = type == TrackPieceType::Curve ?
                static_cast<float>(bankAngleDegrees) * kPi / 180.0f * std::sin(kPi * progress) :
                (legacyFlatTwist ? 2.0f * kPi * progress : 0.0f);
            const float crossX = ty * nz - tz * ny;
            const float crossY = tz * nx - tx * nz;
            const float crossZ = tx * ny - ty * nx;
            const float cosine = std::cos(bank);
            const float sine = std::sin(bank);
            nx = nx * cosine + crossX * sine;
            ny = ny * cosine + crossY * sine;
            nz = nz * cosine + crossZ * sine;
        }
        samples.push_back(TrackSurfaceSample{current.x, current.y, current.z, tx, ty, tz, nx, ny, nz, WidthAt(progress) * 0.5f, material, id});
    }
    return samples;
}
