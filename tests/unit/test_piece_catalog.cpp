#include "../../src/editor/piece_catalog.hpp"
#include "../../src/track/track_road_geometry.hpp"

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

} // namespace

int main() {
    TestCatalogIdentityAndShortcutMapping();
    TestThumbnailPiecesAreValidAndRepresentBranches();
    if (failures == 0) std::cout << "Neon Racer piece-catalog tests passed.\n";
    return failures == 0 ? 0 : 1;
}
