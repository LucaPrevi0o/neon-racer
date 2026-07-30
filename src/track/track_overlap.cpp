#include "track_road_geometry.hpp"

#include <algorithm>

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

struct RoadBounds {
    float minimumX;
    float maximumX;
    float minimumY;
    float maximumY;
    float minimumZ;
    float maximumZ;
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
        const TrackRoadAxis axis = TrackRoadGeometry::SurfaceRightAxis(sample);
        sideX = axis.x;
        sideY = axis.y;
        sideZ = axis.z;
    }
    return RoadPoint{sample.x + sideX * side, sample.y + sideY * side, sample.z + sideZ * side};
}

std::vector<RoadQuad> RoadQuadsForArm(const TrackPiece& piece) {
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

std::vector<RoadQuad> RoadQuads(const TrackPiece& piece) {
    std::vector<RoadQuad> quads;
    const std::vector<TrackPiece> arms = TrackRoadGeometry::PhysicalRoadArms(piece);
    for (std::vector<TrackPiece>::const_iterator arm = arms.begin(); arm != arms.end(); ++arm) {
        const std::vector<RoadQuad> armQuads = RoadQuadsForArm(*arm);
        quads.insert(quads.end(), armQuads.begin(), armQuads.end());
    }
    return quads;
}

RoadBounds BoundsFor(const std::vector<RoadQuad>& quads) {
    const RoadPoint& first = quads.front().corners[0];
    RoadBounds bounds = {first.x, first.x, first.y, first.y, first.z, first.z};
    for (std::vector<RoadQuad>::const_iterator quad = quads.begin(); quad != quads.end(); ++quad) {
        bounds.minimumY = std::min(bounds.minimumY, quad->minimumY);
        bounds.maximumY = std::max(bounds.maximumY, quad->maximumY);
        for (int corner = 0; corner < 4; ++corner) {
            bounds.minimumX = std::min(bounds.minimumX, quad->corners[corner].x);
            bounds.maximumX = std::max(bounds.maximumX, quad->corners[corner].x);
            bounds.minimumZ = std::min(bounds.minimumZ, quad->corners[corner].z);
            bounds.maximumZ = std::max(bounds.maximumZ, quad->corners[corner].z);
        }
    }
    return bounds;
}

bool BoundsMayOverlap(const RoadBounds& first, const RoadBounds& second, float roadThickness) {
    return !(first.maximumX < second.minimumX || second.maximumX < first.minimumX ||
             first.maximumZ < second.minimumZ || second.maximumZ < first.minimumZ ||
             first.maximumY + roadThickness < second.minimumY ||
             second.maximumY + roadThickness < first.minimumY);
}

bool RoadSurfacesOverlap(const TrackPiece& first, const TrackPiece& second) {
    const std::vector<RoadQuad> firstQuads = RoadQuads(first);
    const std::vector<RoadQuad> secondQuads = RoadQuads(second);
    const float roadThickness = 0.20f;
    if (firstQuads.empty() || secondQuads.empty() ||
        !BoundsMayOverlap(BoundsFor(firstQuads), BoundsFor(secondQuads), roadThickness)) {
        return false;
    }
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
    // Callers use this as a placement predicate. Treat malformed candidates as
    // blocked before asking their geometry for connectors, which also keeps
    // arbitrary external data from reaching integer grid arithmetic.
    if (!IsValidPiece(candidate)) return true;
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
