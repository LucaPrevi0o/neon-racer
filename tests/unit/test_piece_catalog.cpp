#include "neon_racer/editor/piece_catalog.hpp"
#include "track/track_road_geometry.hpp"

#include <iostream>
#include <set>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestCatalogIdentityAndShortcutMapping() {
    std::size_t count = 0;
    const EditorPieceCatalog::Item* items = EditorPieceCatalog::Items(count);
    Expect(items != 0 && count == 6, "the piece catalogue exposes all six supported component types");

    std::set<int> shortcuts;
    std::set<TrackPieceType> types;
    for (std::size_t index = 0; index < count; ++index) {
        const EditorPieceCatalog::Item& item = items[index];
        Expect(item.name[0] != '\0' && item.description[0] != '\0',
               "every catalogue component has a visible name and description");
        shortcuts.insert(item.shortcut);
        types.insert(item.type);
        Expect(EditorPieceCatalog::Find(item.type) == &item,
               "type lookup returns the canonical catalogue entry");
        Expect(EditorPieceCatalog::FindShortcut(item.shortcut) == &item,
               "shortcut lookup returns the matching catalogue entry");
    }
    Expect(shortcuts.size() == count, "catalogue shortcuts are unique");
    Expect(types.size() == count, "catalogue component types are unique");
    Expect(EditorPieceCatalog::FindShortcut(0) == 0 && EditorPieceCatalog::FindShortcut(7) == 0,
           "unsupported shortcut digits have no catalogue entry");
}

void TestThumbnailPiecesAreValidAndRepresentBranches() {
    std::size_t count = 0;
    const EditorPieceCatalog::Item* items = EditorPieceCatalog::Items(count);
    for (std::size_t index = 0; index < count; ++index) {
        const TrackPiece thumbnail = EditorPieceCatalog::Thumbnail(items[index].type);
        Track track;
        Expect(track.Add(thumbnail) != 0, "each thumbnail is a valid standalone track component");

        const std::vector<TrackPiece> arms = TrackRoadGeometry::PhysicalRoadArms(thumbnail);
        const std::size_t expectedArms = (thumbnail.type == TrackPieceType::Branch ||
                                          thumbnail.type == TrackPieceType::Merge) ? 2u : 1u;
        Expect(arms.size() == expectedArms, "thumbnail topology matches its physical road-arm count");
    }
}

void TestLoadedHalfTurnCannotPoisonLaterPreviews() {
    Track loadedLayout;
    const std::uint32_t halfTurnId = loadedLayout.AddCurve(GridPosition{0, 0, 0}, Heading::East,
                                                            CurveTurn::Right, 4, 5, 5, 0,
                                                            SurfaceMaterial::Regular, 180);
    const TrackPiece* loadedHalfTurnPointer = loadedLayout.GetPiece(halfTurnId);
    Expect(loadedHalfTurnPointer != 0 && loadedHalfTurnPointer->length == 0 &&
               loadedHalfTurnPointer->curveDegrees == 180,
           "loaded-curve setup uses the domain representation that has no straight length");
    if (loadedHalfTurnPointer == 0) return;
    const TrackPiece loadedHalfTurn = *loadedHalfTurnPointer;

    TrackPiece staleStraight = loadedHalfTurn;
    staleStraight.type = TrackPieceType::Straight;
    staleStraight.entryPosition = GridPosition{100, 0, 100};
    Track standalone;
    Expect(standalone.Add(staleStraight) == 0,
           "blindly changing a loaded curve type leaves an invalid zero-length straight");

    const TrackPiece repairedStraight =
        EditorPieceCatalog::RetargetPreview(staleStraight, TrackPieceType::Straight);
    Expect(repairedStraight.type == TrackPieceType::Straight && repairedStraight.length == 4 &&
               repairedStraight.entryPosition == staleStraight.entryPosition &&
               repairedStraight.entryHeading == staleStraight.entryHeading &&
               repairedStraight.width == staleStraight.width && repairedStraight.exitWidth == staleStraight.exitWidth,
           "retargeting repairs incompatible fields while preserving common placement data");
    Expect(!loadedLayout.HasOverlappingGeometry(repairedStraight) && loadedLayout.Add(repairedStraight) != 0,
           "a repaired far-away preview remains placeable after a loaded 180-degree turn");

    TrackPiece tunedHalfTurn = loadedHalfTurn;
    tunedHalfTurn.bankAngleDegrees = 25;
    const TrackPiece unchanged = EditorPieceCatalog::RetargetPreview(tunedHalfTurn, TrackPieceType::Curve);
    Expect(unchanged.curveDegrees == 180 && unchanged.bankAngleDegrees == 25 && unchanged.length == 0,
           "a valid same-type half-turn keeps its tuned curve fields unchanged");

    TrackPiece staleBranch = loadedHalfTurn;
    staleBranch.type = TrackPieceType::Branch;
    staleBranch.elevationDelta = 3;
    const TrackPiece repairedBranch = EditorPieceCatalog::RetargetPreview(staleBranch, TrackPieceType::Branch);
    Expect(repairedBranch.length == 4 && repairedBranch.lateralOffset == 3 &&
               repairedBranch.elevationDelta == 0 && repairedBranch.exitWidth == repairedBranch.width,
           "retargeting resets branch-only invariants instead of reporting a false overlap");
}

} // namespace

int main() {
    TestCatalogIdentityAndShortcutMapping();
    TestThumbnailPiecesAreValidAndRepresentBranches();
    TestLoadedHalfTurnCannotPoisonLaterPreviews();
    if (failures == 0) std::cout << "Neon Racer piece-catalog tests passed.\n";
    return failures == 0 ? 0 : 1;
}
