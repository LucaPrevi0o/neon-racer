#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "track_contracts.hpp"

// The editor's world is intentionally large, but a finite envelope keeps
// connector and road-geometry arithmetic safely inside a signed int. These
// are domain invariants rather than viewport limits: ordinary editor movement
// and persisted layouts may use any value in this range.
namespace TrackLimits {

const int kMaximumGridCoordinate = 1000000;
const int kMaximumElevationDelta = 1000000;

} // namespace TrackLimits

// All layout data is deliberately integer based. One unit is one metre in the
// editor grid, so saved layouts never accumulate floating-point placement drift.
struct GridPosition {
    int x;
    int y;
    int z;

    bool operator==(const GridPosition& other) const;
    bool operator<(const GridPosition& other) const;
};

enum class Heading {
    North,
    East,
    South,
    West,
};

enum class TrackPieceType {
    Straight,
    Curve,
    Loop,
    Twist,
    Branch,
    Merge,
};

enum class CurveTurn {
    Left,
    Right,
};

enum class RaceDirection {
    Forward,
    Reverse,
};

struct TrackConnector {
    GridPosition position;
    Heading heading;
    int width;
};

struct TrackPathPoint {
    float x;
    float y;
    float z;
};

struct TrackPiece {
    std::uint32_t id;
    TrackPieceType type;
    GridPosition entryPosition;
    Heading entryHeading;
    // `width` is the entry width retained for compatibility with milestone 1.
    // `exitWidth` makes a piece a linear road-width transition when different.
    int width;
    int exitWidth;
    int length;
    CurveTurn curveTurn;
    int curveRadius;
    int curveDegrees;
    int bankAngleDegrees;
    int elevationDelta;
    int lateralOffset;
    SurfaceMaterial material;

    TrackConnector EntryConnector() const;
    TrackConnector ExitConnector() const;
    // Current components expose one connector at each end. Branch/merge
    // components will return multiple entries or exits through this API.
    std::vector<TrackConnector> EntryConnectors() const;
    std::vector<TrackConnector> ExitConnectors() const;
    std::vector<GridPosition> CenterlineCells() const;
    std::vector<TrackPathPoint> PathPoints(int curveSubdivisions = 0) const;
    std::vector<TrackSurfaceSample> SurfaceSamples(int subdivisions = 0) const;
    float WidthAt(float progress) const;
};

enum class TrackIssueKind {
    DisconnectedEntry,
    DisconnectedExit,
    OverlappingGeometry,
    NotOneClosedLoop,
    MissingStartFinish,
    InvalidStartFinish,
};

struct TrackIssue {
    TrackIssueKind kind;
    std::string message;
    std::vector<std::uint32_t> affectedPieceIds;
};

struct TrackValidation {
    bool raceReady;
    std::vector<TrackIssue> issues;
};

struct TrackConnectorRef {
    std::uint32_t pieceId;
    std::size_t connectorIndex;
};

struct TrackConnection {
    TrackConnectorRef exit;
    TrackConnectorRef entry;
};

class Track {
public:
    Track();

    std::uint32_t AddStraight(GridPosition entry, Heading heading, int length, int width = 5,
                              int exitWidth = -1, int elevationDelta = 0, int lateralOffset = 0,
                              SurfaceMaterial material = SurfaceMaterial::Regular);
    std::uint32_t AddCurve(GridPosition entry, Heading heading, CurveTurn turn, int radius,
                           int width = 5, int exitWidth = -1, int elevationDelta = 0,
                           SurfaceMaterial material = SurfaceMaterial::Regular, int curveDegrees = 90,
                           int bankAngleDegrees = 0);
    std::uint32_t AddLoop(GridPosition entry, Heading heading, int radius, int width = 5,
                          int exitWidth = -1, int elevationDelta = 0,
                          SurfaceMaterial material = SurfaceMaterial::Regular, int lateralOffset = 0);
    std::uint32_t AddTwist(GridPosition entry, Heading heading, int length, int width = 5,
                           int exitWidth = -1, int elevationDelta = 0, int lateralOffset = 0,
                           SurfaceMaterial material = SurfaceMaterial::Regular);
    std::uint32_t AddBranch(GridPosition entry, Heading heading, int length, int armOffset = 3,
                            int width = 5, SurfaceMaterial material = SurfaceMaterial::Regular);
    std::uint32_t AddMerge(GridPosition exit, Heading heading, int length, int armOffset = 3,
                           int width = 5, SurfaceMaterial material = SurfaceMaterial::Regular);
    std::uint32_t Add(const TrackPiece& piece);
    bool RemovePiece(std::uint32_t id);
    void Clear();

    const std::vector<TrackPiece>& Pieces() const;
    const TrackPiece* GetPiece(std::uint32_t id) const;
    bool HasOverlappingGeometry(const TrackPiece& candidate) const;
    bool ReplacePiece(const TrackPiece& replacement);

    bool SetStartFinish(std::uint32_t pieceId, RaceDirection direction);
    bool HasStartFinish() const;
    std::uint32_t StartFinishPieceId() const;
    RaceDirection SelectedRaceDirection() const;
    bool IsLayoutValid() const;
    TrackValidation Validate() const;
    std::vector<TrackConnection> Connections() const;
    TrackContact QuerySurface(float x, float y, float z, float maxDistance = 4.0f) const;
    std::uint32_t LayoutRevision() const;

    static Track CreateSampleCircuit();

private:
    std::uint32_t AddPiece(const TrackPiece& piece);
    const TrackPiece* FindPiece(std::uint32_t id) const;
    static bool IsValidPiece(const TrackPiece& piece);
    void InvalidateValidation();

    std::vector<TrackPiece> pieces_;
    std::uint32_t nextPieceId_;
    std::uint32_t startFinishPieceId_;
    RaceDirection raceDirection_;
    std::uint32_t layoutRevision_;
    mutable bool validationDirty_;
    mutable TrackValidation cachedValidation_;
};

const char* HeadingName(Heading heading);
