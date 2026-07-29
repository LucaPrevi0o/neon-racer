#include "track_road_geometry.hpp"

#include <cmath>

namespace {

float Length(const TrackRoadAxis& axis) {
    return std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
}

} // namespace

namespace TrackRoadGeometry {

std::vector<TrackPiece> PhysicalRoadArms(const TrackPiece& piece) {
    if (piece.type == TrackPieceType::Branch) {
        TrackPiece rightArm = piece;
        rightArm.type = TrackPieceType::Straight;
        TrackPiece leftArm = rightArm;
        leftArm.lateralOffset = -leftArm.lateralOffset;
        return std::vector<TrackPiece>{rightArm, leftArm};
    }

    if (piece.type == TrackPieceType::Merge) {
        std::vector<TrackPiece> arms;
        const std::vector<TrackConnector> entries = piece.EntryConnectors();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            TrackPiece arm = piece;
            arm.type = TrackPieceType::Straight;
            arm.entryPosition = entries[index].position;
            arm.lateralOffset = index == 0 ? -std::abs(piece.lateralOffset) : std::abs(piece.lateralOffset);
            arms.push_back(arm);
        }
        return arms;
    }

    return std::vector<TrackPiece>(1, piece);
}

std::vector<TrackSurfaceSample> PhysicalRoadSurfaceSamples(const TrackPiece& piece) {
    std::vector<TrackSurfaceSample> samples;
    const std::vector<TrackPiece> arms = PhysicalRoadArms(piece);
    for (std::vector<TrackPiece>::const_iterator arm = arms.begin(); arm != arms.end(); ++arm) {
        const std::vector<TrackSurfaceSample> armSamples = arm->SurfaceSamples();
        samples.insert(samples.end(), armSamples.begin(), armSamples.end());
    }
    return samples;
}

TrackRoadAxis SurfaceRightAxis(const TrackSurfaceSample& sample) {
    return TrackRoadAxis{sample.tangentY * sample.normalZ - sample.tangentZ * sample.normalY,
                         sample.tangentZ * sample.normalX - sample.tangentX * sample.normalZ,
                         sample.tangentX * sample.normalY - sample.tangentY * sample.normalX};
}

bool NormalizedSurfaceRightAxis(const TrackSurfaceSample& sample, TrackRoadAxis& axis) {
    axis = SurfaceRightAxis(sample);
    const float length = Length(axis);
    if (length < 0.0001f) {
        axis = TrackRoadAxis{0.0f, 0.0f, 0.0f};
        return false;
    }
    axis.x /= length;
    axis.y /= length;
    axis.z /= length;
    return true;
}

} // namespace TrackRoadGeometry
