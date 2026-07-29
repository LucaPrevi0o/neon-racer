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
    const int count = subdivisions > 0 ? subdivisions : ((type == TrackPieceType::Straight || type == TrackPieceType::Twist) ? std::max(2, length) : (type == TrackPieceType::Loop ? std::max(24, curveRadius * 12) : std::max(12, curveRadius * 8)));
    const std::vector<TrackPathPoint> points = PathPoints(count); std::vector<TrackSurfaceSample> samples;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const TrackPathPoint& current = points[index]; const TrackPathPoint& before = points[index == 0 ? index : index - 1]; const TrackPathPoint& after = points[index + 1 < points.size() ? index + 1 : index];
        float tx = after.x - before.x, ty = after.y - before.y, tz = after.z - before.z; float length = Length(tx, ty, tz);
        if (length > 0.0001f) { tx /= length; ty /= length; tz /= length; } else { tx = 1.0f; ty = tz = 0.0f; }
        const float horizontal = std::sqrt(tx * tx + tz * tz); float nx = -ty * tx, ny = horizontal, nz = -ty * tz; length = Length(nx, ny, nz);
        if (length > 0.0001f) { nx /= length; ny /= length; nz /= length; }
        if (type == TrackPieceType::Loop) { const GridPosition lateral = Right(entryHeading); nx = -static_cast<float>(lateral.z) * ty; ny = static_cast<float>(lateral.z) * tx - static_cast<float>(lateral.x) * tz; nz = static_cast<float>(lateral.x) * ty; length = Length(nx, ny, nz); if (length > 0.0001f) { nx /= length; ny /= length; nz /= length; } }
        const float progress = points.size() <= 1 ? 0.0f : static_cast<float>(index) / static_cast<float>(points.size() - 1);
        const float bank = type == TrackPieceType::Curve ? static_cast<float>(bankAngleDegrees) * kPi / 180.0f * std::sin(kPi * progress) : (type == TrackPieceType::Twist ? 2.0f * kPi * progress : 0.0f);
        const float crossX = ty * nz - tz * ny, crossY = tz * nx - tx * nz, crossZ = tx * ny - ty * nx, cosine = std::cos(bank), sine = std::sin(bank);
        nx = nx * cosine + crossX * sine; ny = ny * cosine + crossY * sine; nz = nz * cosine + crossZ * sine;
        samples.push_back(TrackSurfaceSample{current.x, current.y, current.z, tx, ty, tz, nx, ny, nz, WidthAt(progress) * 0.5f, material, id});
    }
    return samples;
}
