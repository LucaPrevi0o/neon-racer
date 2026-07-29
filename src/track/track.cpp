#include "track.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace {

const float kPi = 3.14159265359f;

GridPosition Add(GridPosition position, int dx, int dz) {
    position.x += dx;
    position.z += dz;
    return position;
}

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

GridPosition Forward(Heading heading) {
    switch (heading) {
    case Heading::North: return GridPosition{0, 0, -1};
    case Heading::East: return GridPosition{1, 0, 0};
    case Heading::South: return GridPosition{0, 0, 1};
    case Heading::West: return GridPosition{-1, 0, 0};
    }
    return GridPosition{0, 0, 0};
}

Heading Turn(Heading heading, CurveTurn turn) {
    const int direction = turn == CurveTurn::Right ? 1 : 3;
    return static_cast<Heading>((static_cast<int>(heading) + direction) % 4);
}

GridPosition Right(Heading heading) {
    return Forward(Turn(heading, CurveTurn::Right));
}

bool Connects(const TrackConnector& exit, const TrackConnector& entry) {
    return exit.position == entry.position && exit.heading == entry.heading && exit.width == entry.width;
}

} // namespace

bool GridPosition::operator==(const GridPosition& other) const {
    return x == other.x && y == other.y && z == other.z;
}

bool GridPosition::operator<(const GridPosition& other) const {
    if (x != other.x) return x < other.x;
    if (y != other.y) return y < other.y;
    return z < other.z;
}

TrackConnector TrackPiece::EntryConnector() const {
    if (type == TrackPieceType::Merge) return EntryConnectors().front();
    return TrackConnector{entryPosition, entryHeading, width};
}

TrackConnector TrackPiece::ExitConnector() const {
    if (type == TrackPieceType::Branch) return ExitConnectors().front();
    if (type == TrackPieceType::Merge) {
        const GridPosition forward = Forward(entryHeading);
        GridPosition exit = Add(entryPosition, forward.x * length, forward.z * length);
        exit.y += elevationDelta;
        return TrackConnector{exit, entryHeading, exitWidth};
    }
    if (type == TrackPieceType::Straight || type == TrackPieceType::Twist) {
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        GridPosition exit = Add(entryPosition, forward.x * length + side.x * lateralOffset,
                                forward.z * length + side.z * lateralOffset);
        exit.y += elevationDelta;
        return TrackConnector{exit, entryHeading, exitWidth};
    }

    if (type == TrackPieceType::Loop) {
        GridPosition exit = entryPosition;
        const GridPosition side = Right(entryHeading);
        exit.x += side.x * lateralOffset;
        exit.z += side.z * lateralOffset;
        exit.y += elevationDelta;
        return TrackConnector{exit, entryHeading, exitWidth};
    }

    const GridPosition forward = Forward(entryHeading);
    const GridPosition side = Right(entryHeading);
    const float sideSign = curveTurn == CurveTurn::Right ? 1.0f : -1.0f;
    // Arc progress is always forward along the entry heading. The turn side
    // only changes the lateral displacement; signing the angle made left
    // curves exit behind their entry, unlike their rendered path.
    const float angle = static_cast<float>(curveDegrees) * kPi / 180.0f;
    GridPosition exit = Add(entryPosition,
                            static_cast<int>(std::round(static_cast<float>(forward.x) * curveRadius * std::sin(angle) +
                                                        static_cast<float>(side.x) * curveRadius * (1.0f - std::cos(angle)) * sideSign)),
                            static_cast<int>(std::round(static_cast<float>(forward.z) * curveRadius * std::sin(angle) +
                                                        static_cast<float>(side.z) * curveRadius * (1.0f - std::cos(angle)) * sideSign)));
    exit.y += elevationDelta;
    const int quarterTurns = (curveDegrees / 90) % 4;
    const int signedTurns = curveTurn == CurveTurn::Right ? quarterTurns : (4 - quarterTurns) % 4;
    const Heading exitHeading = static_cast<Heading>((static_cast<int>(entryHeading) + signedTurns) % 4);
    return TrackConnector{exit, exitHeading, exitWidth};
}

std::vector<TrackConnector> TrackPiece::EntryConnectors() const {
    if (type == TrackPieceType::Merge) {
        const GridPosition side = Right(entryHeading);
        GridPosition right = Add(entryPosition, side.x * std::abs(lateralOffset),
                                 side.z * std::abs(lateralOffset));
        GridPosition left = Add(entryPosition, -side.x * std::abs(lateralOffset),
                                -side.z * std::abs(lateralOffset));
        return std::vector<TrackConnector>{TrackConnector{right, entryHeading, width}, TrackConnector{left, entryHeading, width}};
    }
    return std::vector<TrackConnector>(1, EntryConnector());
}

std::vector<TrackConnector> TrackPiece::ExitConnectors() const {
    if (type == TrackPieceType::Branch) {
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        const GridPosition centre = Add(entryPosition, forward.x * length, forward.z * length);
        const int spread = std::abs(lateralOffset);
        GridPosition right = Add(centre, side.x * spread, side.z * spread);
        GridPosition left = Add(centre, -side.x * spread, -side.z * spread);
        right.y += elevationDelta;
        left.y += elevationDelta;
        return std::vector<TrackConnector>{TrackConnector{right, entryHeading, exitWidth},
                                           TrackConnector{left, entryHeading, exitWidth}};
    }
    return std::vector<TrackConnector>(1, ExitConnector());
}

std::vector<GridPosition> TrackPiece::CenterlineCells() const {
    std::vector<GridPosition> cells;
    cells.push_back(entryPosition);

    if (type == TrackPieceType::Straight || type == TrackPieceType::Twist) {
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        for (int step = 1; step <= length; ++step) {
            const float progress = static_cast<float>(step) / static_cast<float>(length);
            GridPosition cell = Add(entryPosition,
                                    static_cast<int>(std::round(static_cast<float>(forward.x * step) +
                                                                static_cast<float>(side.x * lateralOffset) * progress)),
                                    static_cast<int>(std::round(static_cast<float>(forward.z * step) +
                                                                static_cast<float>(side.z * lateralOffset) * progress)));
            cell.y += static_cast<int>(std::round(static_cast<float>(elevationDelta) * progress));
            cells.push_back(cell);
        }
        return cells;
    }

    const std::vector<TrackPathPoint> points = PathPoints(type == TrackPieceType::Loop ? std::max(24, curveRadius * 12) :
        std::max(12, curveRadius * curveDegrees / 15));
    for (std::vector<TrackPathPoint>::const_iterator point = points.begin(); point != points.end(); ++point) {
        const GridPosition cell = GridPosition{static_cast<int>(std::round(point->x)),
                                               static_cast<int>(std::round(point->y)),
                                               static_cast<int>(std::round(point->z))};
        if (cells.empty() || !(cells.back() == cell)) cells.push_back(cell);
    }
    return cells;
}

std::vector<TrackPathPoint> TrackPiece::PathPoints(int curveSubdivisions) const {
    std::vector<TrackPathPoint> points;
    if (type == TrackPieceType::Straight || type == TrackPieceType::Twist) {
        const TrackConnector entry = EntryConnector();
        const TrackConnector exit = ExitConnector();
        const int segments = curveSubdivisions > 0 ? curveSubdivisions : 1;
        for (int index = 0; index <= segments; ++index) {
            const float progress = static_cast<float>(index) / static_cast<float>(segments);
            points.push_back(TrackPathPoint{
                static_cast<float>(entry.position.x) +
                    (static_cast<float>(exit.position.x) - static_cast<float>(entry.position.x)) * progress,
                static_cast<float>(entry.position.y) +
                    (static_cast<float>(exit.position.y) - static_cast<float>(entry.position.y)) * progress,
                static_cast<float>(entry.position.z) +
                    (static_cast<float>(exit.position.z) - static_cast<float>(entry.position.z)) * progress});
        }
        return points;
    }

    if (type == TrackPieceType::Loop) {
        const int segments = curveSubdivisions > 0 ? curveSubdivisions : std::max(24, curveRadius * 12);
        const GridPosition forward = Forward(entryHeading);
        const GridPosition side = Right(entryHeading);
        for (int index = 0; index <= segments; ++index) {
            const float progress = static_cast<float>(index) / static_cast<float>(segments);
            const float angle = 2.0f * kPi * progress;
            points.push_back(TrackPathPoint{
                static_cast<float>(entryPosition.x) + static_cast<float>(forward.x) * curveRadius * std::sin(angle),
                static_cast<float>(entryPosition.y) + curveRadius * (1.0f - std::cos(angle)) +
                    static_cast<float>(elevationDelta) * progress,
                static_cast<float>(entryPosition.z) + static_cast<float>(forward.z) * curveRadius * std::sin(angle) +
                    static_cast<float>(side.z * lateralOffset) * progress});
            points.back().x += static_cast<float>(side.x * lateralOffset) * progress;
        }
        return points;
    }

    const int segments = curveSubdivisions > 0 ? curveSubdivisions :
        std::max(12, curveRadius * curveDegrees / 15);
    const GridPosition forward = Forward(entryHeading);
    const GridPosition side = Right(entryHeading);
    const float sideSign = curveTurn == CurveTurn::Right ? 1.0f : -1.0f;
    for (int index = 0; index <= segments; ++index) {
        const float angle = (static_cast<float>(curveDegrees) * kPi / 180.0f * static_cast<float>(index)) /
            static_cast<float>(segments);
        const float forwardDistance = static_cast<float>(curveRadius) * std::sin(angle);
        const float sideDistance = static_cast<float>(curveRadius) * (1.0f - std::cos(angle)) * sideSign;
        points.push_back(TrackPathPoint{
            static_cast<float>(entryPosition.x) + static_cast<float>(forward.x) * forwardDistance +
                static_cast<float>(side.x) * sideDistance,
                static_cast<float>(entryPosition.y) + static_cast<float>(elevationDelta) *
                    static_cast<float>(index) / static_cast<float>(segments),
            static_cast<float>(entryPosition.z) + static_cast<float>(forward.z) * forwardDistance +
                static_cast<float>(side.z) * sideDistance});
    }
    return points;
}

Track::Track()
    : nextPieceId_(1), startFinishPieceId_(0), raceDirection_(RaceDirection::Forward),
      layoutRevision_(0), validationDirty_(true), cachedValidation_{false, std::vector<TrackIssue>()} {
}

std::uint32_t Track::AddStraight(GridPosition entry, Heading heading, int length, int width,
                                 int requestedExitWidth, int elevationDelta, int lateralOffset,
                                 SurfaceMaterial material) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Straight, entry, heading, width, resolvedExitWidth,
                               length, CurveTurn::Right, 0, 90, 0, elevationDelta, lateralOffset, material});
}

std::uint32_t Track::AddCurve(GridPosition entry, Heading heading, CurveTurn turn, int radius, int width,
                              int requestedExitWidth, int elevationDelta, SurfaceMaterial material, int curveDegrees,
                              int bankAngleDegrees) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Curve, entry, heading, width, resolvedExitWidth,
                               0, turn, radius, curveDegrees, bankAngleDegrees, elevationDelta, 0, material});
}

std::uint32_t Track::AddLoop(GridPosition entry, Heading heading, int radius, int width,
                             int requestedExitWidth, int elevationDelta, SurfaceMaterial material, int lateralOffset) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Loop, entry, heading, width, resolvedExitWidth,
                               0, CurveTurn::Right, radius, 360, 0, elevationDelta, lateralOffset, material});
}

std::uint32_t Track::AddTwist(GridPosition entry, Heading heading, int length, int width,
                              int requestedExitWidth, int elevationDelta, int lateralOffset,
                              SurfaceMaterial material) {
    const int resolvedExitWidth = requestedExitWidth < 0 ? width : requestedExitWidth;
    return AddPiece(TrackPiece{0, TrackPieceType::Twist, entry, heading, width, resolvedExitWidth,
                               length, CurveTurn::Right, 0, 90, 0, elevationDelta, lateralOffset, material});
}

std::uint32_t Track::AddBranch(GridPosition entry, Heading heading, int length, int armOffset,
                               int width, SurfaceMaterial material) {
    return AddPiece(TrackPiece{0, TrackPieceType::Branch, entry, heading, width, width,
                               length, CurveTurn::Right, 0, 90, 0, 0, armOffset, material});
}

std::uint32_t Track::AddMerge(GridPosition exit, Heading heading, int length, int armOffset,
                              int width, SurfaceMaterial material) {
    return AddPiece(TrackPiece{0, TrackPieceType::Merge, exit, heading, width, width,
                               length, CurveTurn::Right, 0, 90, 0, 0, armOffset, material});
}

bool Track::RemovePiece(std::uint32_t id) {
    const std::vector<TrackPiece>::iterator found = std::find_if(pieces_.begin(), pieces_.end(),
        [id](const TrackPiece& piece) { return piece.id == id; });
    if (found == pieces_.end()) return false;
    pieces_.erase(found);
    if (startFinishPieceId_ == id) startFinishPieceId_ = 0;
    InvalidateValidation();
    return true;
}

std::uint32_t Track::Add(const TrackPiece& piece) {
    return AddPiece(piece);
}

void Track::Clear() {
    pieces_.clear();
    startFinishPieceId_ = 0;
    nextPieceId_ = 1;
    InvalidateValidation();
}

const std::vector<TrackPiece>& Track::Pieces() const {
    return pieces_;
}

const TrackPiece* Track::GetPiece(std::uint32_t id) const {
    return FindPiece(id);
}

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

bool Track::ReplacePiece(const TrackPiece& replacement) {
    if (replacement.id == 0 || !IsValidPiece(replacement)) return false;
    if (FindPiece(replacement.id) == 0 || HasOverlappingGeometry(replacement)) return false;

    for (std::vector<TrackPiece>::iterator piece = pieces_.begin(); piece != pieces_.end(); ++piece) {
        if (piece->id == replacement.id) {
            *piece = replacement;
            InvalidateValidation();
            return true;
        }
    }
    return false;
}

std::uint32_t Track::LayoutRevision() const {
    return layoutRevision_;
}

bool Track::SetStartFinish(std::uint32_t pieceId, RaceDirection direction) {
    const TrackPiece* piece = FindPiece(pieceId);
    if (piece == 0 || piece->type != TrackPieceType::Straight) return false;
    startFinishPieceId_ = pieceId;
    raceDirection_ = direction;
    InvalidateValidation();
    return true;
}

bool Track::HasStartFinish() const {
    return startFinishPieceId_ != 0;
}

std::uint32_t Track::StartFinishPieceId() const {
    return startFinishPieceId_;
}

RaceDirection Track::SelectedRaceDirection() const {
    return raceDirection_;
}

bool Track::IsLayoutValid() const {
    const TrackValidation validation = Validate();
    for (std::vector<TrackIssue>::const_iterator issue = validation.issues.begin();
         issue != validation.issues.end(); ++issue) {
        if (issue->kind != TrackIssueKind::MissingStartFinish &&
            issue->kind != TrackIssueKind::InvalidStartFinish) {
            return false;
        }
    }
    return true;
}

std::uint32_t Track::AddPiece(const TrackPiece& piece) {
    TrackPiece candidate = piece;
    if (!IsValidPiece(candidate)) return 0;
    candidate.id = nextPieceId_;
    if (HasOverlappingGeometry(candidate)) return 0;
    pieces_.push_back(candidate);
    ++nextPieceId_;
    InvalidateValidation();
    return candidate.id;
}

void Track::InvalidateValidation() {
    validationDirty_ = true;
    ++layoutRevision_;
}

const TrackPiece* Track::FindPiece(std::uint32_t id) const {
    for (std::vector<TrackPiece>::const_iterator it = pieces_.begin(); it != pieces_.end(); ++it) {
        if (it->id == id) return &(*it);
    }
    return 0;
}
