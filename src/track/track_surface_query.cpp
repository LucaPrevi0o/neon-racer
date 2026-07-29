#include "track.hpp"

#include <algorithm>
#include <cmath>

namespace {

// Branches and merges are expanded into physical straight arms for contact,
// exactly as they are when rendered. A visible route must be drivable.
std::vector<TrackSurfaceSample> RoadSurfaceSamples(const TrackPiece& piece) {
    if (piece.type == TrackPieceType::Branch) {
        TrackPiece rightArm = piece;
        rightArm.type = TrackPieceType::Straight;
        TrackPiece leftArm = rightArm;
        leftArm.lateralOffset = -leftArm.lateralOffset;
        std::vector<TrackSurfaceSample> samples = rightArm.SurfaceSamples();
        const std::vector<TrackSurfaceSample> leftSamples = leftArm.SurfaceSamples();
        samples.insert(samples.end(), leftSamples.begin(), leftSamples.end());
        return samples;
    }
    if (piece.type == TrackPieceType::Merge) {
        std::vector<TrackSurfaceSample> samples;
        const std::vector<TrackConnector> entries = piece.EntryConnectors();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            TrackPiece arm = piece;
            arm.type = TrackPieceType::Straight;
            arm.entryPosition = entries[index].position;
            arm.lateralOffset = index == 0 ? -std::abs(piece.lateralOffset) : std::abs(piece.lateralOffset);
            const std::vector<TrackSurfaceSample> armSamples = arm.SurfaceSamples();
            samples.insert(samples.end(), armSamples.begin(), armSamples.end());
        }
        return samples;
    }
    return piece.SurfaceSamples();
}

float Length(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

TrackContact Track::QuerySurface(float x, float y, float z, float maxDistance) const {
    TrackContact result = {false, TrackSurfaceSample{}, maxDistance, false};
    for (std::vector<TrackPiece>::const_iterator piece = pieces_.begin(); piece != pieces_.end(); ++piece) {
        const std::vector<TrackSurfaceSample> samples = RoadSurfaceSamples(*piece);
        for (std::vector<TrackSurfaceSample>::const_iterator sample = samples.begin(); sample != samples.end(); ++sample) {
            // The road's side direction must remain three-dimensional for loops.
            float sideX = sample->tangentY * sample->normalZ - sample->tangentZ * sample->normalY;
            float sideY = sample->tangentZ * sample->normalX - sample->tangentX * sample->normalZ;
            float sideZ = sample->tangentX * sample->normalY - sample->tangentY * sample->normalX;
            const float sideLength = Length(sideX, sideY, sideZ);
            if (sideLength < 0.0001f) continue;
            sideX /= sideLength;
            sideY /= sideLength;
            sideZ /= sideLength;
            const float lateral = (x - sample->x) * sideX + (y - sample->y) * sideY + (z - sample->z) * sideZ;
            const float railLimit = sample->halfWidth + 0.35f;
            if (std::fabs(lateral) > railLimit) continue;
            const float clampedLateral = std::max(-sample->halfWidth, std::min(sample->halfWidth, lateral));
            const float distance = Length(x - (sample->x + sideX * clampedLateral),
                                          y - (sample->y + sideY * clampedLateral),
                                          z - (sample->z + sideZ * clampedLateral));
            if (distance >= result.distance) continue;
            result.found = true;
            result.distance = distance;
            result.guardrailHit = std::fabs(lateral) > sample->halfWidth;
            result.surface = *sample;
            result.surface.x += sideX * clampedLateral;
            result.surface.y += sideY * clampedLateral;
            result.surface.z += sideZ * clampedLateral;
        }
    }
    return result;
}
