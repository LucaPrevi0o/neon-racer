#include "track.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace {

struct RoadPoint {
    float x;
    float y;
    float z;
};

struct RoadQuad {
    RoadPoint corners[4];
    float minimumY;
    float maximumY;
};

float Cross2D(const RoadPoint& first, const RoadPoint& second, const RoadPoint& third) {
    return (second.x - first.x) * (third.z - first.z) - (second.z - first.z) * (third.x - first.x);
}

bool ProperSegmentsIntersect(const RoadPoint& firstStart, const RoadPoint& firstEnd,
                             const RoadPoint& secondStart, const RoadPoint& secondEnd) {
    const float firstA = Cross2D(firstStart, firstEnd, secondStart);
    const float firstB = Cross2D(firstStart, firstEnd, secondEnd);
    const float secondA = Cross2D(secondStart, secondEnd, firstStart);
    const float secondB = Cross2D(secondStart, secondEnd, firstEnd);
    const float epsilon = 0.0001f;
    return ((firstA > epsilon && firstB < -epsilon) || (firstA < -epsilon && firstB > epsilon)) &&
        ((secondA > epsilon && secondB < -epsilon) || (secondA < -epsilon && secondB > epsilon));
}

bool IsStrictlyInsideQuad(const RoadPoint& point, const RoadQuad& quad) {
    bool positive = false;
    bool negative = false;
    const float epsilon = 0.0001f;
    for (int index = 0; index < 4; ++index) {
        const float cross = Cross2D(quad.corners[index], quad.corners[(index + 1) % 4], point);
        if (cross > epsilon) positive = true;
        if (cross < -epsilon) negative = true;
    }
    return !(positive && negative) && (positive || negative);
}

bool FootprintsOverlap(const RoadQuad& first, const RoadQuad& second) {
    for (int firstIndex = 0; firstIndex < 4; ++firstIndex) {
        const RoadPoint& firstStart = first.corners[firstIndex];
        const RoadPoint& firstEnd = first.corners[(firstIndex + 1) % 4];
        for (int secondIndex = 0; secondIndex < 4; ++secondIndex) {
            if (ProperSegmentsIntersect(firstStart, firstEnd, second.corners[secondIndex],
                                        second.corners[(secondIndex + 1) % 4])) return true;
        }
    }
    for (int index = 0; index < 4; ++index) {
        if (IsStrictlyInsideQuad(first.corners[index], second) || IsStrictlyInsideQuad(second.corners[index], first)) return true;
    }
    return false;
}

RoadPoint OffsetRoadPoint(const TrackPiece& piece, const TrackSurfaceSample& sample, float side) {
    float sideX = 0.0f;
    float sideY = 0.0f;
    float sideZ = 0.0f;
    if (piece.type == TrackPieceType::Straight) {
        switch (piece.entryHeading) {
        case Heading::North: sideX = 1.0f; break;
        case Heading::East: sideZ = 1.0f; break;
        case Heading::South: sideX = -1.0f; break;
        case Heading::West: sideZ = -1.0f; break;
        }
    } else {
        sideX = sample.tangentY * sample.normalZ - sample.tangentZ * sample.normalY;
        sideY = sample.tangentZ * sample.normalX - sample.tangentX * sample.normalZ;
        sideZ = sample.tangentX * sample.normalY - sample.tangentY * sample.normalX;
    }
    return RoadPoint{sample.x + sideX * side, sample.y + sideY * side, sample.z + sideZ * side};
}

std::vector<RoadQuad> RoadQuads(const TrackPiece& piece) {
    if (piece.type == TrackPieceType::Branch) {
        TrackPiece rightArm = piece;
        rightArm.type = TrackPieceType::Straight;
        TrackPiece leftArm = rightArm;
        leftArm.lateralOffset = -leftArm.lateralOffset;
        std::vector<RoadQuad> quads = RoadQuads(rightArm);
        const std::vector<RoadQuad> leftQuads = RoadQuads(leftArm);
        quads.insert(quads.end(), leftQuads.begin(), leftQuads.end());
        return quads;
    }
    if (piece.type == TrackPieceType::Merge) {
        std::vector<RoadQuad> quads;
        const std::vector<TrackConnector> entries = piece.EntryConnectors();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            TrackPiece arm = piece;
            arm.type = TrackPieceType::Straight;
            arm.entryPosition = entries[index].position;
            arm.lateralOffset = index == 0 ? -std::abs(arm.lateralOffset) : std::abs(arm.lateralOffset);
            const std::vector<RoadQuad> armQuads = RoadQuads(arm);
            quads.insert(quads.end(), armQuads.begin(), armQuads.end());
        }
        return quads;
    }
    const std::vector<TrackSurfaceSample> samples = piece.SurfaceSamples();
    std::vector<RoadQuad> quads;
    for (std::size_t index = 0; index + 1 < samples.size(); ++index) {
        const RoadPoint fromRight = OffsetRoadPoint(piece, samples[index], samples[index].halfWidth);
        const RoadPoint fromLeft = OffsetRoadPoint(piece, samples[index], -samples[index].halfWidth);
        const RoadPoint toRight = OffsetRoadPoint(piece, samples[index + 1], samples[index + 1].halfWidth);
        const RoadPoint toLeft = OffsetRoadPoint(piece, samples[index + 1], -samples[index + 1].halfWidth);
        RoadQuad quad = {{fromRight, toRight, toLeft, fromLeft}, fromRight.y, fromRight.y};
        for (int corner = 1; corner < 4; ++corner) {
            quad.minimumY = std::min(quad.minimumY, quad.corners[corner].y);
            quad.maximumY = std::max(quad.maximumY, quad.corners[corner].y);
        }
        quads.push_back(quad);
    }
    return quads;
}

bool RoadSurfacesOverlap(const TrackPiece& first, const TrackPiece& second) {
    const std::vector<RoadQuad> firstQuads = RoadQuads(first);
    const std::vector<RoadQuad> secondQuads = RoadQuads(second);
    const float roadThickness = 0.20f;
    for (std::vector<RoadQuad>::const_iterator firstQuad = firstQuads.begin(); firstQuad != firstQuads.end(); ++firstQuad) {
        for (std::vector<RoadQuad>::const_iterator secondQuad = secondQuads.begin(); secondQuad != secondQuads.end(); ++secondQuad) {
            const bool verticallySeparated = firstQuad->maximumY + roadThickness < secondQuad->minimumY ||
                secondQuad->maximumY + roadThickness < firstQuad->minimumY;
            if (!verticallySeparated && FootprintsOverlap(*firstQuad, *secondQuad)) return true;
        }
    }
    return false;
}

bool Connects(const TrackConnector& exit, const TrackConnector& entry) {
    return exit.position == entry.position && exit.heading == entry.heading && exit.width == entry.width;
}

} // namespace

bool Track::HasOverlappingGeometry(const TrackPiece& candidate) const {
    for (std::vector<TrackPiece>::const_iterator it = pieces_.begin(); it != pieces_.end(); ++it) {
        if (candidate.id != 0 && candidate.id == it->id) continue;
        // Adjacent road ribbons intentionally share a small mitered area at a
        // compatible connector, especially where a straight meets a curve.
        // Treat the directly connected pair as one continuous road surface.
        bool directlyConnected = false;
        const std::vector<TrackConnector> candidateExits = candidate.ExitConnectors();
        const std::vector<TrackConnector> candidateEntries = candidate.EntryConnectors();
        const std::vector<TrackConnector> existingExits = it->ExitConnectors();
        const std::vector<TrackConnector> existingEntries = it->EntryConnectors();
        for (std::size_t exitIndex = 0; exitIndex < candidateExits.size(); ++exitIndex) {
            for (std::size_t entryIndex = 0; entryIndex < existingEntries.size(); ++entryIndex) {
                if (Connects(candidateExits[exitIndex], existingEntries[entryIndex])) directlyConnected = true;
            }
        }
        for (std::size_t exitIndex = 0; exitIndex < existingExits.size(); ++exitIndex) {
            for (std::size_t entryIndex = 0; entryIndex < candidateEntries.size(); ++entryIndex) {
                if (Connects(existingExits[exitIndex], candidateEntries[entryIndex])) directlyConnected = true;
            }
        }
        // The connected arm shares its boundary (and a small mitered area)
        // with the next piece. Other components still undergo normal checks.
        if (directlyConnected) continue;
        if (RoadSurfacesOverlap(candidate, *it)) return true;
    }
    return false;
}
