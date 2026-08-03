#include "../../src/persistence/draft_io.hpp"
#include "../../src/playable/playable_export.hpp"
#include "../../src/persistence/playable_track_io.hpp"
#include "../../src/track/track_fingerprint.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

RaceCar CarAt(float x, float speed) {
    return RaceCar{RaceVector3{x, 1.0f, 0.0f}, RaceVector3{speed, 0.0f, 0.0f},
                   RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f, speed};
}

VerifiedGhostData GhostFor(const Track& track, float offset, float duration) {
    VerifiedGhostData ghost;
    ghost.samples.push_back(GhostSample{CarAt(offset, 10.0f), 0.0f});
    ghost.samples.push_back(GhostSample{CarAt(offset + 4.0f, 14.0f), duration});
    ghost.durationSeconds = duration;
    ghost.layoutFingerprint = TrackFingerprint::Calculate(track);
    return ghost;
}

VerificationState VerifiedFor(const Track& track) {
    VerificationState verification;
    verification.hasSavedGhost = true;
    verification.isVerifiedForPlayableExport = true;
    verification.verifiedLayoutRevision = track.LayoutRevision();
    verification.verifiedLayoutFingerprint = TrackFingerprint::Calculate(track);
    return verification;
}

TrackMetadata Metadata(std::uint32_t version) {
    TrackMetadata metadata;
    metadata.name = "Persistence Run";
    metadata.creator = "Neon Racer Player";
    metadata.description = "A package whose replay can be updated without changing its identity.";
    metadata.playableExportVersion = version;
    return metadata;
}

void TestDraftOverwriteUpgradesCurrentFile() {
    const std::string path = "/tmp/neon_racer_draft_overwrite_workflow.draft";
    {
        std::ofstream file(path.c_str());
        file << "NEON_RACER_DRAFT 1\n";
        file << "START 0 0\n";
        file << "PIECES 1\n";
        file << "PIECE 1 0 0 0 0 1 5 4 0 0\n";
    }

    Track loaded;
    std::string error;
    Expect(DraftIO::Load(path, loaded, error), "an older editable draft loads before overwrite");
    Expect(loaded.AddStraight(GridPosition{20, 0, 0}, Heading::East, 4) != 0,
           "the loaded editable draft can be changed");
    Expect(DraftIO::Save(loaded, path, error),
           "saving to the established draft path replaces it atomically");

    std::ifstream input(path.c_str());
    std::string header;
    int version = 0;
    input >> header >> version;
    Track roundTrip;
    Expect(header == "NEON_RACER_DRAFT" && version == 6 &&
               DraftIO::Load(path, roundTrip, error) && roundTrip.Pieces().size() == 2,
           "same-path draft save upgrades the format and preserves the edit");
    std::remove(path.c_str());
}

void TestPlayableGhostReplacementPreservesPackageIdentity() {
    const Track layout = Track::CreateSampleCircuit();
    const std::string path = "/tmp/neon_racer_playable_update_workflow.nrplay";
    std::string error;

    PlayableTrack original;
    Expect(PlayableExport::BuildVerified(layout, Metadata(1), GhostFor(layout, 1.0f, 0.10f),
                                         VerifiedFor(layout), original, error),
           "playable update setup builds its first verified package");
    Expect(PlayableTrackIO::Save(original, path, error),
           "the first verified package passes save-time readback validation");

    PlayableTrack updated = original;
    updated.metadata.playableExportVersion = 2;
    updated.verificationGhost = GhostFor(layout, 9.0f, 0.08f);
    Expect(PlayableTrackIO::Save(updated, path, error),
           "a better replay replaces the exact existing package path");

    PlayableTrack loaded;
    Expect(PlayableTrackIO::Load(path, loaded, error) &&
               loaded.metadata.name == original.metadata.name &&
               loaded.metadata.creator == original.metadata.creator &&
               loaded.metadata.description == original.metadata.description &&
               loaded.metadata.playableExportVersion == 2 &&
               loaded.layoutFingerprint == original.layoutFingerprint &&
               loaded.verificationGhost.samples.size() == 2 &&
               loaded.verificationGhost.samples.front().car.position.x == 9.0f,
           "ghost replacement preserves package identity and stores only the new replay/version");
    std::remove(path.c_str());
}

} // namespace

int main() {
    TestDraftOverwriteUpgradesCurrentFile();
    TestPlayableGhostReplacementPreservesPackageIdentity();
    if (failures == 0) std::cout << "Neon Racer persistence workflow tests passed.\n";
    return failures == 0 ? 0 : 1;
}
