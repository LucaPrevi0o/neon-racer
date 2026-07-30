#include "track_road_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace {

float Length(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);
}

bool RoadSide(const TrackSurfaceSample& sample, float& sideX, float& sideY, float& sideZ) {
    // The road's side direction must remain three-dimensional for loops.
    TrackRoadAxis axis;
    if (!TrackRoadGeometry::NormalizedSurfaceRightAxis(sample, axis)) return false;
    sideX = axis.x;
    sideY = axis.y;
    sideZ = axis.z;
    return true;
}

// A branch or merge is physically the union of its two road arms. A rail on
// one arm must not become an obstacle where the other arm still provides road.
bool PointLiesInsideAnyRoadArm(const std::vector<TrackSurfaceSample>& samples,
                               float x, float y, float z, float maxDistance) {
    for (std::vector<TrackSurfaceSample>::const_iterator sample = samples.begin(); sample != samples.end(); ++sample) {
        float sideX = 0.0f;
        float sideY = 0.0f;
        float sideZ = 0.0f;
        if (!RoadSide(*sample, sideX, sideY, sideZ)) continue;
        const float lateral = (x - sample->x) * sideX + (y - sample->y) * sideY + (z - sample->z) * sideZ;
        if (std::fabs(lateral) > sample->halfWidth) continue;
        const float distance = Length(x - (sample->x + sideX * lateral),
                                      y - (sample->y + sideY * lateral),
                                      z - (sample->z + sideZ * lateral));
        if (distance < maxDistance) return true;
    }
    return false;
}

} // namespace

TrackContact Track::QuerySurface(float x, float y, float z, float maxDistance) const {
    TrackContact result = {false, TrackSurfaceSample{}, maxDistance, false, false, 0.0f};
    for (std::vector<TrackPiece>::const_iterator piece = pieces_.begin(); piece != pieces_.end(); ++piece) {
        const std::vector<TrackSurfaceSample> samples = TrackRoadGeometry::PhysicalRoadSurfaceSamples(*piece);
        const bool insideAnyRoadArm = (piece->type == TrackPieceType::Branch || piece->type == TrackPieceType::Merge) &&
            PointLiesInsideAnyRoadArm(samples, x, y, z, maxDistance);
        for (std::vector<TrackSurfaceSample>::const_iterator sample = samples.begin(); sample != samples.end(); ++sample) {
            float sideX = 0.0f;
            float sideY = 0.0f;
            float sideZ = 0.0f;
            if (!RoadSide(*sample, sideX, sideY, sideZ)) continue;
            const float lateral = (x - sample->x) * sideX + (y - sample->y) * sideY + (z - sample->z) * sideZ;
            // The rail needs a wider query band than its visible thickness:
            // a fast fixed step can otherwise move the car beyond 0.35 m in
            // one update and lose collision before resolution can occur.
            const float railLimit = sample->halfWidth + std::min(maxDistance, 1.0f);
            if (std::fabs(lateral) > railLimit) continue;
            const bool guardrailHit = std::fabs(lateral) > sample->halfWidth;
            // Select the containing arm's surface rather than merely clearing
            // a competing arm's rail flag. Its tangent and normal drive the
            // vehicle response, so they must match the drivable road.
            if (guardrailHit && insideAnyRoadArm) continue;
            const float clampedLateral = std::max(-sample->halfWidth, std::min(sample->halfWidth, lateral));
            const float distance = Length(x - (sample->x + sideX * clampedLateral),
                                          y - (sample->y + sideY * clampedLateral),
                                          z - (sample->z + sideZ * clampedLateral));
            if (distance >= result.distance) continue;
            result.found = true;
            result.distance = distance;
            result.guardrailHit = guardrailHit;
            result.twistGuide = piece->type == TrackPieceType::Twist &&
                !TrackLimits::IsLegacyFlatTwistRadius(piece->curveRadius);
            result.lateralOffset = lateral;
            result.surface = *sample;
            result.surface.x += sideX * clampedLateral;
            result.surface.y += sideY * clampedLateral;
            result.surface.z += sideZ * clampedLateral;
        }
    }
    return result;
}
