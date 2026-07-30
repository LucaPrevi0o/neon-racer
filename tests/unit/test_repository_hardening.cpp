#include "../../src/persistence/draft_io.hpp"
#include "../../src/track/track_fingerprint.hpp"
#include "../../src/track/track.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

void TestSurfaceCacheInvalidation() {
    Track track;
    const std::uint32_t id = track.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    Expect(id != 0, "surface-cache setup creates a straight");

    const TrackContact first = track.QuerySurface(2.0f, 0.16f, 0.0f, 1.0f);
    const TrackContact repeated = track.QuerySurface(2.0f, 0.16f, 0.0f, 1.0f);
    Expect(first.found && repeated.found && first.surface.pieceId == id && repeated.surface.pieceId == id,
           "repeated surface queries return the cached road contact");

    TrackPiece moved = *track.GetPiece(id);
    moved.entryPosition = GridPosition{20, 0, 0};
    Expect(track.ReplacePiece(moved), "moving a piece invalidates its cached surface geometry");

    const TrackContact oldLocation = track.QuerySurface(2.0f, 0.16f, 0.0f, 1.0f);
    const TrackContact newLocation = track.QuerySurface(22.0f, 0.16f, 0.0f, 1.0f);
    Expect(!oldLocation.found && newLocation.found && newLocation.surface.pieceId == id,
           "the rebuilt surface cache follows the moved piece");

    Expect(track.RemovePiece(id), "surface-cache setup piece can be removed");
    Expect(!track.QuerySurface(22.0f, 0.16f, 0.0f, 1.0f).found,
           "removing a piece invalidates its cached surface geometry");
}

void TestAtomicDraftSavePreservesExistingFile() {
    char directoryPattern[] = "/tmp/neon_racer_hardening_XXXXXX";
    char* directory = mkdtemp(directoryPattern);
    Expect(directory != 0, "atomic-draft test creates a temporary directory");
    if (directory == 0) return;

    const std::string path = std::string(directory) + "/track.draft";
    Track original;
    original.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    const std::uint64_t originalFingerprint = TrackFingerprint::Calculate(original);

    std::string error;
    Expect(DraftIO::Save(original, path, error), "atomic-draft setup writes the original file");

    Track replacement;
    replacement.AddStraight(GridPosition{40, 0, 0}, Heading::East, 8);

    const bool permissionsChanged = chmod(directory, 0500) == 0;
    Expect(permissionsChanged, "atomic-draft test makes the destination directory read-only");
    error.clear();
    const bool replacementRejected = permissionsChanged && !DraftIO::Save(replacement, path, error);
    chmod(directory, 0700);

    Track preserved;
    error.clear();
    const bool preservedLoads = DraftIO::Load(path, preserved, error);
    Expect(replacementRejected && preservedLoads &&
               TrackFingerprint::Calculate(preserved) == originalFingerprint,
           "a failed atomic replacement preserves the previous valid draft");

    std::remove(path.c_str());
    rmdir(directory);
}

} // namespace

int main() {
    TestSurfaceCacheInvalidation();
    TestAtomicDraftSavePreservesExistingFile();

    if (failures != 0) {
        std::cerr << failures << " repository-hardening test(s) failed.\n";
        return 1;
    }
    std::cout << "Repository-hardening tests passed.\n";
    return 0;
}
