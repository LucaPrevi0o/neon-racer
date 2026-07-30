#include "../../src/persistence/playable_track_io.hpp"
#include "../../src/persistence/track_layout_codec.hpp"
#include "../../src/playable/playable_export.hpp"
#include "../../src/race/time_trial.hpp"
#include "../../src/track/track_fingerprint.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    ++failures;
}

bool NearlyEqual(float first, float second) { return std::fabs(first - second) < 0.0001f; }

RaceCar CarAt(float positionX, float speed) {
    return RaceCar{RaceVector3{positionX, 1.0f, -2.0f}, RaceVector3{speed, 0.0f, 0.0f},
                   RaceVector3{1.0f, 0.0f, 0.0f}, RaceVector3{0.0f, 1.0f, 0.0f}, 0.0f, speed};
}

VerifiedGhostData VerifiedGhostFor(const Track& track) {
    VerifiedGhostData ghost;
    ghost.samples.push_back(GhostSample{CarAt(4.0f, 12.0f), 0.0f});
    ghost.samples.push_back(GhostSample{CarAt(8.0f, 18.0f), 1.0f / 30.0f});
    ghost.durationSeconds = 0.10f;
    ghost.layoutFingerprint = TrackFingerprint::Calculate(track);
    return ghost;
}

TrackMetadata Metadata(std::uint32_t version) {
    TrackMetadata metadata;
    metadata.name = "Night Run";
    metadata.creator = "Neon Racer Player";
    metadata.description = "A verified banked circuit for evening time trials.";
    metadata.playableExportVersion = version;
    return metadata;
}

VerificationState VerifiedFor(const Track& track) {
    VerificationState verification;
    verification.hasSavedGhost = true;
    verification.isVerifiedForPlayableExport = true;
    verification.verifiedLayoutRevision = track.LayoutRevision();
    verification.verifiedLayoutFingerprint = TrackFingerprint::Calculate(track);
    return verification;
}

Track LegacyFlatTwistCircuit() {
    Track track;
    track.AddTwist(GridPosition{0, 0, 0}, Heading::East, 4, 5, 5, 0, 0,
                   SurfaceMaterial::Regular, TrackLimits::kLegacyFlatTwistRadius);
    track.AddCurve(GridPosition{4, 0, 0}, Heading::East, CurveTurn::Right, 4);
    const std::uint32_t start = track.AddStraight(GridPosition{8, 0, 4}, Heading::South, 4);
    track.AddCurve(GridPosition{8, 0, 8}, Heading::South, CurveTurn::Right, 4);
    track.AddStraight(GridPosition{4, 0, 12}, Heading::West, 4);
    track.AddCurve(GridPosition{0, 0, 12}, Heading::West, CurveTurn::Right, 4);
    track.AddStraight(GridPosition{-4, 0, 8}, Heading::North, 4);
    track.AddCurve(GridPosition{-4, 0, 4}, Heading::North, CurveTurn::Right, 4);
    track.SetStartFinish(start, RaceDirection::Forward);
    return track;
}

void TestPreviewAndVerifiedExportPolicy() {
    Track incomplete;
    incomplete.AddStraight(GridPosition{0, 0, 0}, Heading::East, 4);
    PlayableTrack preview;
    std::string error;
    Expect(!PlayableExport::BuildPreview(incomplete, "Broken", preview, error),
           "a transient preview rejects an incomplete layout");

    const Track layout = Track::CreateSampleCircuit();
    Expect(PlayableExport::BuildPreview(layout, "Preview", preview, error) &&
               preview.metadata.name == "Preview" && preview.metadata.playableExportVersion == 0 &&
               preview.layoutFingerprint == TrackFingerprint::Calculate(layout) && preview.verificationGhost.samples.empty(),
           "a preview freezes a race-ready layout without inventing verification data");

    const VerifiedGhostData ghost = VerifiedGhostFor(layout);
    const VerificationState verification = VerifiedFor(layout);
    PlayableTrack verified;
    Expect(PlayableExport::BuildVerified(layout, Metadata(3), ghost, verification, verified, error) &&
               verified.metadata.playableExportVersion == 3 && verified.verificationGhost.samples.size() == 2 &&
               verified.layoutFingerprint == ghost.layoutFingerprint,
           "a verified export requires and preserves an exact-layout ghost");

    TrackMetadata missingDescription = Metadata(1);
    missingDescription.description.clear();
    Expect(!PlayableExport::BuildVerified(layout, missingDescription, ghost, verification, verified, error),
           "a playable export requires complete metadata");

    TrackMetadata controlMetadata = Metadata(1);
    controlMetadata.name = std::string("Night\0Run", 9u);
    Expect(!PlayableExport::BuildVerified(layout, controlMetadata, ghost, verification, verified, error),
           "a playable export rejects control characters that cannot be rendered safely");

    VerifiedGhostData foreignGhost = ghost;
    foreignGhost.layoutFingerprint += 1;
    Expect(!PlayableExport::BuildVerified(layout, Metadata(1), foreignGhost, verification, verified, error),
           "a playable export rejects a ghost for a different layout");

    VerificationState staleVerification = verification;
    staleVerification.verifiedLayoutRevision += 1;
    Expect(!PlayableExport::BuildVerified(layout, Metadata(1), ghost, staleVerification, verified, error),
           "a playable export rejects a ghost after its source layout changed");
}

void TestPackageRoundTripAndRaceImport() {
    const Track layout = Track::CreateSampleCircuit();
    const VerifiedGhostData ghost = VerifiedGhostFor(layout);
    PlayableTrack original;
    std::string error;
    Expect(PlayableExport::BuildVerified(layout, Metadata(7), ghost, VerifiedFor(layout), original, error),
           "round-trip setup builds a verified package");

    const std::string path = "/tmp/neon_racer_playable_round_trip.nrplay";
    std::remove(path.c_str());
    Expect(PlayableTrackIO::Save(original, path, error), "a verified package saves atomically");

    PlayableTrack loaded;
    Expect(PlayableTrackIO::Load(path, loaded, error) && loaded.metadata.name == original.metadata.name &&
               loaded.metadata.creator == original.metadata.creator && loaded.metadata.description == original.metadata.description &&
               loaded.metadata.playableExportVersion == 7 && loaded.layout.Pieces().size() == layout.Pieces().size() &&
               loaded.layoutFingerprint == original.layoutFingerprint && loaded.verificationGhost.samples.size() == 2 &&
               NearlyEqual(loaded.verificationGhost.durationSeconds, ghost.durationSeconds) &&
               NearlyEqual(loaded.verificationGhost.samples[1].car.position.x, 8.0f),
           "a playable package round-trips layout, metadata, and verification samples");

    TimeTrial trial;
    trial.Start(loaded.layout);
    Expect(trial.IsReady() && trial.ImportVerifiedGhost(loaded.verificationGhost) && trial.HasVerifiedGhost() &&
               NearlyEqual(trial.GhostCar().position.x, 4.0f),
           "a loaded package seeds a race-ready time trial with its verified ghost");

    std::remove(path.c_str());
}

void TestLegacyFlatTwistPackageMigration() {
    const Track legacy = LegacyFlatTwistCircuit();
    std::string error;
    Expect(legacy.Validate().raceReady,
           "legacy playable migration setup forms a race-ready circuit");

    std::ostringstream layoutStream;
    Expect(TrackLayoutCodec::Write(layoutStream, legacy, error),
           "legacy playable migration setup writes its structural layout");
    std::string layout = layoutStream.str();
    const std::string runtimeSentinel = " -2 ";
    const std::string::size_type sentinel = layout.find(runtimeSentinel);
    Expect(sentinel != std::string::npos,
           "legacy playable migration setup finds the flat-Twist sentinel");
    if (sentinel != std::string::npos) layout.replace(sentinel, runtimeSentinel.size(), " 0 ");

    const std::string path = "/tmp/neon_racer_legacy_flat_twist.nrplay";
    const std::string name = "Legacy Twist";
    const std::string creator = "Neon Racer";
    const std::string description = "Flat rolling Twist migration fixture.";
    {
        std::ofstream file(path.c_str());
        file << "NEON_RACER_PLAYABLE 1\nSTATUS PLAYABLE\n";
        file << "NAME_BYTES " << name.size() << "\n" << name << "\n";
        file << "CREATOR_BYTES " << creator.size() << "\n" << creator << "\n";
        file << "DESCRIPTION_BYTES " << description.size() << "\n" << description << "\n";
        file << "EXPORT_VERSION 1\n";
        file << "LAYOUT_FINGERPRINT " << TrackFingerprint::CalculateLegacyFlatTwist(legacy) << "\n";
        file << "LAYOUT 5\n" << layout;
        file << "VERIFICATION_GHOST 1 2 0.1\n";
        file << "GHOST_SAMPLE 0 4 1 -2 12 0 0 1 0 0 0 1 0 0 12\n";
        file << "GHOST_SAMPLE 0.1 8 1 -2 18 0 0 1 0 0 0 1 0 0 18\n";
        file << "END_PLAYABLE\n";
    }

    PlayableTrack loaded;
    Expect(PlayableTrackIO::Load(path, loaded, error) && loaded.layout.Validate().raceReady &&
               !loaded.layout.Pieces().empty() &&
               TrackLimits::IsLegacyFlatTwistRadius(loaded.layout.Pieces().front().curveRadius) &&
               loaded.layoutFingerprint == TrackFingerprint::Calculate(loaded.layout) &&
               loaded.verificationGhost.layoutFingerprint == loaded.layoutFingerprint,
           "a v5 playable package migrates its flat Twist and rebinds its verified ghost");
    std::remove(path.c_str());
}

void TestMalformedPackageLeavesDestinationUnchanged() {
    const std::string path = "/tmp/neon_racer_playable_malformed.nrplay";
    {
        std::ofstream file(path.c_str());
        file << "NEON_RACER_PLAYABLE 99\n";
    }

    PlayableTrack untouched;
    untouched.metadata.name = "Keep me";
    std::string error;
    Expect(!PlayableTrackIO::Load(path, untouched, error) && untouched.metadata.name == "Keep me",
           "unsupported playable files fail without replacing the caller's current package");
    std::remove(path.c_str());
}

void TestPackageIntegrityAndNoClobber() {
    const Track layout = Track::CreateSampleCircuit();
    PlayableTrack original;
    std::string error;
    Expect(PlayableExport::BuildVerified(layout, Metadata(4), VerifiedGhostFor(layout), VerifiedFor(layout), original, error),
           "integrity-test setup builds a valid package");

    const std::string path = "/tmp/neon_racer_playable_integrity.nrplay";
    std::remove(path.c_str());
    Expect(PlayableTrackIO::Save(original, path, error), "integrity-test setup saves a valid package");

    PlayableTrack invalidReplacement = original;
    invalidReplacement.verificationGhost.durationSeconds = 0.0f;
    Expect(!PlayableTrackIO::Save(invalidReplacement, path, error),
           "an invalid replacement is rejected before it can clobber a saved package");
    PlayableTrack retained;
    Expect(PlayableTrackIO::Load(path, retained, error) && retained.metadata.playableExportVersion == 4,
           "a rejected replacement preserves the existing package and version");

    std::ifstream input(path.c_str(), std::ios::binary);
    const std::string originalBytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::string fingerprintRecord = "LAYOUT_FINGERPRINT " + std::to_string(original.layoutFingerprint);
    const std::string alteredFingerprintRecord = "LAYOUT_FINGERPRINT " +
                                                std::to_string(original.layoutFingerprint + 1u);
    const std::string::size_type fingerprintOffset = originalBytes.find(fingerprintRecord);
    Expect(fingerprintOffset != std::string::npos, "integrity-test setup finds the package fingerprint record");
    std::string alteredBytes = originalBytes;
    if (fingerprintOffset != std::string::npos) {
        alteredBytes.replace(fingerprintOffset, fingerprintRecord.size(), alteredFingerprintRecord);
        std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
        output.write(alteredBytes.data(), static_cast<std::streamsize>(alteredBytes.size()));
    }

    PlayableTrack untouched;
    untouched.metadata.name = "Keep current package";
    Expect(!PlayableTrackIO::Load(path, untouched, error) && untouched.metadata.name == "Keep current package",
           "a mismatched layout fingerprint fails without replacing the caller package");

    Expect(PlayableTrackIO::Save(original, path, error),
           "integrity-test setup restores a valid package after fingerprint validation");

    std::ifstream restoredInput(path.c_str(), std::ios::binary);
    const std::string validBytes((std::istreambuf_iterator<char>(restoredInput)), std::istreambuf_iterator<char>());
    const std::string firstSamplePrefix = "GHOST_SAMPLE 0 ";
    const std::string::size_type sampleOffset = validBytes.find(firstSamplePrefix);
    Expect(sampleOffset != std::string::npos, "integrity-test setup finds the initial ghost sample");
    std::string invalidGhostBytes = validBytes;
    if (sampleOffset != std::string::npos) {
        invalidGhostBytes.replace(sampleOffset, firstSamplePrefix.size(), "GHOST_SAMPLE 0.05 ");
        std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
        output.write(invalidGhostBytes.data(), static_cast<std::streamsize>(invalidGhostBytes.size()));
    }
    untouched.metadata.name = "Keep invalid ghost package";
    Expect(!PlayableTrackIO::Load(path, untouched, error) && untouched.metadata.name == "Keep invalid ghost package",
           "an invalid persisted ghost timestamp fails without replacing the caller package");

    Expect(PlayableTrackIO::Save(original, path, error),
           "integrity-test setup restores a valid package before replay-range validation");
    std::ifstream rangeInput(path.c_str(), std::ios::binary);
    const std::string rangeBytes((std::istreambuf_iterator<char>(rangeInput)), std::istreambuf_iterator<char>());
    const std::string positionPrefix = "GHOST_SAMPLE 0 4 ";
    const std::string::size_type positionOffset = rangeBytes.find(positionPrefix);
    Expect(positionOffset != std::string::npos, "integrity-test setup finds a persisted replay position");
    std::string extremeReplayBytes = rangeBytes;
    if (positionOffset != std::string::npos) {
        extremeReplayBytes.replace(positionOffset, positionPrefix.size(), "GHOST_SAMPLE 0 1000001 ");
        std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
        output.write(extremeReplayBytes.data(), static_cast<std::streamsize>(extremeReplayBytes.size()));
    }
    untouched.metadata.name = "Keep extreme ghost package";
    Expect(!PlayableTrackIO::Load(path, untouched, error) && untouched.metadata.name == "Keep extreme ghost package",
           "finite but unsafe persisted replay values are rejected transactionally");

    Expect(PlayableTrackIO::Save(original, path, error),
           "integrity-test setup restores a valid package before trailing-data validation");

    std::ofstream trailing(path.c_str(), std::ios::binary | std::ios::app);
    trailing << "UNEXPECTED_TRAILING_DATA\n";
    trailing.close();
    untouched.metadata.name = "Keep trailing package";
    Expect(!PlayableTrackIO::Load(path, untouched, error) && untouched.metadata.name == "Keep trailing package",
           "non-whitespace data after END_PLAYABLE is rejected transactionally");
    std::remove(path.c_str());
}

void TestCustomLibraryVersioning() {
    const char* previousDataDirectory = std::getenv("NEON_RACER_DATA_DIR");
    const bool hadPreviousDataDirectory = previousDataDirectory != 0;
    const std::string previousValue = hadPreviousDataDirectory ? previousDataDirectory : "";
    const std::string dataDirectory = "/tmp/neon_racer_playable_io_test_data";
    const std::string playablePath = dataDirectory + "/playables/Night_Run.nrplay";
    std::remove(playablePath.c_str());
    setenv("NEON_RACER_DATA_DIR", dataDirectory.c_str(), 1);

    std::uint32_t version = 0;
    std::string error;
    Expect(PlayableTrackIO::NextExportVersion("Night Run", version, error) && version == 1,
           "a new playable name starts at export version one");

    const Track layout = Track::CreateSampleCircuit();
    PlayableTrack first;
    Expect(PlayableExport::BuildVerified(layout, Metadata(version), VerifiedGhostFor(layout), VerifiedFor(layout), first, error) &&
               PlayableTrackIO::Save(first, PlayableTrackIO::CustomPlayablePath("Night Run"), error),
           "the first version saves in the dedicated playable library");

    const std::string obsoleteTemporaryPath = playablePath + ".tmp";
    std::remove(obsoleteTemporaryPath.c_str());
    Expect(mkfifo(obsoleteTemporaryPath.c_str(), 0600) == 0,
           "temporary-file safety setup creates an obsolete predictable FIFO path");
    Expect(PlayableTrackIO::Save(first, playablePath, error),
           "a save uses a unique temporary file instead of a predictable special-file path");
    struct stat temporaryStatus;
    Expect(lstat(obsoleteTemporaryPath.c_str(), &temporaryStatus) == 0 && S_ISFIFO(temporaryStatus.st_mode),
           "a save leaves an unrelated predictable temporary path untouched");
    std::remove(obsoleteTemporaryPath.c_str());

    Expect(PlayableTrackIO::NextExportVersion("Night Run", version, error) && version == 2,
           "re-exporting the same name advances its persistent version");

    std::vector<PlayableTrackIO::PlayableTrackFile> files;
    Expect(PlayableTrackIO::ListCustomPlayableTracks(files, error) && files.size() == 1 &&
               files.front().displayName == "Night_Run" && files.front().path == playablePath,
           "the dedicated playable library lists exact package paths alongside their display names");

    Expect(!PlayableTrackIO::NextExportVersion("Night_Run", version, error),
           "different names that sanitize to the same path are rejected instead of overwriting each other");

    TrackMetadata externalMetadata = Metadata(1);
    externalMetadata.name = "External Track";
    PlayableTrack external;
    const std::string externalPath = dataDirectory + "/playables/Cool Track.nrplay";
    Expect(PlayableExport::BuildVerified(layout, externalMetadata, VerifiedGhostFor(layout), VerifiedFor(layout), external, error) &&
               PlayableTrackIO::Save(external, externalPath, error),
           "an externally named playable package can be placed in the library folder");
    Expect(PlayableTrackIO::ListCustomPlayableTracks(files, error) && files.size() == 2,
           "the library discovers externally named regular package files");
    bool loadedExternalPath = false;
    for (std::vector<PlayableTrackIO::PlayableTrackFile>::const_iterator file = files.begin(); file != files.end(); ++file) {
        if (file->displayName != "Cool Track") continue;
        PlayableTrack loadedExternal;
        loadedExternalPath = file->path == externalPath && PlayableTrackIO::Load(file->path, loadedExternal, error) &&
                             loadedExternal.metadata.name == "External Track";
    }
    Expect(loadedExternalPath,
           "library entries preserve an exact external filename through the eventual load path");

    const std::string fifoPath = dataDirectory + "/playables/blocked.nrplay";
    std::remove(fifoPath.c_str());
    Expect(mkfifo(fifoPath.c_str(), 0600) == 0, "special-file safety setup creates a package-named FIFO");
    PlayableTrack untouched;
    untouched.metadata.name = "Keep loaded package";
    Expect(!PlayableTrackIO::Load(fifoPath, untouched, error) && untouched.metadata.name == "Keep loaded package" &&
               !PlayableTrackIO::NextExportVersion("blocked", version, error),
           "special package paths are rejected without blocking or being treated as absent");
    std::remove(fifoPath.c_str());

    {
        std::ofstream corrupt(playablePath.c_str(), std::ios::binary | std::ios::trunc);
        corrupt << "NEON_RACER_PLAYABLE 999\n";
    }
    Expect(!PlayableTrackIO::NextExportVersion("Night Run", version, error),
           "a corrupt existing package cannot be silently version-reset or overwritten");

    std::remove(playablePath.c_str());
    std::remove(externalPath.c_str());
    rmdir((dataDirectory + "/playables").c_str());
    rmdir(dataDirectory.c_str());
    if (hadPreviousDataDirectory) {
        setenv("NEON_RACER_DATA_DIR", previousValue.c_str(), 1);
    } else {
        unsetenv("NEON_RACER_DATA_DIR");
    }
}

void TestPackageInputBounds() {
    const std::string oversizedPath = "/tmp/neon_racer_playable_oversized_file.nrplay";
    {
        std::ofstream file(oversizedPath.c_str(), std::ios::binary | std::ios::trunc);
        file.seekp(static_cast<std::streamoff>(PlayableTrackIO::kMaximumPackageFileBytes));
        file.put('\0');
    }
    PlayableTrack untouched;
    untouched.metadata.name = "Keep bounded package";
    std::string error;
    Expect(!PlayableTrackIO::Load(oversizedPath, untouched, error) && untouched.metadata.name == "Keep bounded package",
           "oversized package files are rejected before parser allocation or work");
    std::remove(oversizedPath.c_str());

    const std::string tokenPath = "/tmp/neon_racer_playable_oversized_token.nrplay";
    {
        std::ofstream file(tokenPath.c_str(), std::ios::binary | std::ios::trunc);
        file << std::string(65u, 'X') << " 1\n";
    }
    Expect(!PlayableTrackIO::Load(tokenPath, untouched, error) && untouched.metadata.name == "Keep bounded package",
           "oversized parser labels are rejected without growing an unbounded token string");
    std::remove(tokenPath.c_str());
}

} // namespace

int main() {
    TestPreviewAndVerifiedExportPolicy();
    TestPackageRoundTripAndRaceImport();
    TestLegacyFlatTwistPackageMigration();
    TestMalformedPackageLeavesDestinationUnchanged();
    TestPackageIntegrityAndNoClobber();
    TestCustomLibraryVersioning();
    TestPackageInputBounds();
    if (failures == 0) std::cout << "Neon Racer playable-package tests passed.\n";
    return failures == 0 ? 0 : 1;
}
