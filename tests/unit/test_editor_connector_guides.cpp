#include "../../src/editor/presentation/editor_connector_guides.hpp"

#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestOrdinaryPieceGuides() {
    Track track;
    const std::uint32_t pieceId = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 6);
    const TrackPiece* piece = track.GetPiece(pieceId);
    Expect(piece != 0, "straight guide setup creates a piece");
    if (piece == 0) return;

    const EditorPresentation::ConnectorGuideSet guides =
        EditorPresentation::ConnectorGuidesFor(*piece);
    Expect(guides.entries.size() == 1, "a straight exposes one entry guide");
    Expect(guides.exits.size() == 1, "a straight exposes one exit guide");
}

void TestBranchGuides() {
    Track track;
    const std::uint32_t pieceId =
        track.AddBranch(GridPosition{0, 0, 0}, Heading::East, 8, 3, 5);
    const TrackPiece* piece = track.GetPiece(pieceId);
    Expect(piece != 0, "branch guide setup creates a piece");
    if (piece == 0) return;

    const EditorPresentation::ConnectorGuideSet guides =
        EditorPresentation::ConnectorGuidesFor(*piece);
    Expect(guides.entries.size() == 1, "a branch exposes its shared entry guide");
    Expect(guides.exits.size() == 2, "a branch exposes both exit guides");
    Expect(!(guides.exits[0].position == guides.exits[1].position),
           "branch exit guides remain spatially distinct");
}

void TestMergeGuides() {
    Track track;
    const std::uint32_t pieceId =
        track.AddMerge(GridPosition{8, 0, 0}, Heading::East, 8, 3, 5);
    const TrackPiece* piece = track.GetPiece(pieceId);
    Expect(piece != 0, "merge guide setup creates a piece");
    if (piece == 0) return;

    const EditorPresentation::ConnectorGuideSet guides =
        EditorPresentation::ConnectorGuidesFor(*piece);
    Expect(guides.entries.size() == 2, "a merge exposes both entry guides");
    Expect(guides.exits.size() == 1, "a merge exposes its shared exit guide");
    Expect(!(guides.entries[0].position == guides.entries[1].position),
           "merge entry guides remain spatially distinct");
}

} // namespace

int main() {
    TestOrdinaryPieceGuides();
    TestBranchGuides();
    TestMergeGuides();

    if (failures != 0) {
        std::cerr << failures << " editor connector guide test(s) failed\n";
        return 1;
    }

    std::cout << "All editor connector guide tests passed\n";
    return 0;
}
