#include "neon_racer/editor/draft_library_flow.hpp"
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

void TestDraftLibraryPagination() {
    DraftLibraryFlow flow;
    Expect(flow.ItemCount() == 0 && flow.VisibleCount() == 0 &&
               flow.CurrentPage() == 0 && flow.PageCount() == 0,
           "an empty draft library exposes no pages");

    flow.Reset(DraftLibraryFlow::kPageSize * 2u + 2u);
    Expect(flow.CurrentPage() == 1 && flow.PageCount() == 3 &&
               flow.VisibleCount() == DraftLibraryFlow::kPageSize &&
               !flow.HasPreviousPage() && flow.HasNextPage(),
           "a long draft list starts on the first complete page");

    flow.NextPage();
    Expect(flow.FirstVisibleIndex() == DraftLibraryFlow::kPageSize &&
               flow.CurrentPage() == 2 && flow.HasPreviousPage() && flow.HasNextPage(),
           "next page advances by exactly one visible group");

    flow.NextPage();
    Expect(flow.CurrentPage() == 3 && flow.VisibleCount() == 2 &&
               flow.HasPreviousPage() && !flow.HasNextPage(),
           "the final draft page exposes its remaining files");

    flow.Reset(DraftLibraryFlow::kPageSize + 1u);
    Expect(flow.CurrentPage() == 2 &&
               flow.FirstVisibleIndex() == DraftLibraryFlow::kPageSize &&
               flow.VisibleCount() == 1,
           "refreshing after deletions clamps pagination to the new final page");

    flow.PreviousPage();
    Expect(flow.CurrentPage() == 1,
           "previous page returns to the first group");
    flow.Reset(0);
    Expect(flow.CurrentPage() == 0 && flow.FirstVisibleIndex() == 0,
           "removing all drafts resets pagination completely");
}

} // namespace

int main() {
    TestCatalogIdentityAndShortcutMapping();
    TestThumbnailPiecesAreValidAndRepresentBranches();
    TestDraftLibraryPagination();
    if (failures == 0) std::cout << "Neon Racer editor catalogue tests passed.\n";
    return failures == 0 ? 0 : 1;
}
