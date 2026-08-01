#include "neon_racer/persistence/playable_track_io.hpp"

#include "../internal/atomic_file_writer.hpp"
#include "../internal/playable_package_codec.hpp"
#include "../internal/playable_package_validation.hpp"
#include "../storage_paths.hpp"
#include "../../playable/playable_track.hpp"

#include <fstream>
#include <sstream>

namespace {

bool OpenReadableExistingPackage(const std::string& path, std::string& error) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (input) return true;
    error = "Cannot safely replace the existing playable package.";
    return false;
}

bool SerializeAndVerify(const PlayableTrack& playable, std::string& bytes, std::string& error) {
    std::ostringstream output;
    if (!PlayablePackageCodec::Write(output, playable, error)) return false;
    bytes = output.str();
    if (bytes.size() > PlayableTrackIO::kMaximumPackageFileBytes) {
        error = "Playable package exceeds the supported size limit.";
        return false;
    }

    PlayableTrack roundTrip;
    std::istringstream input(bytes);
    std::string readError;
    if (!PlayablePackageCodec::Read(input, roundTrip, readError)) {
        error = "Playable package failed its save verification: " + readError;
        return false;
    }
    if (roundTrip.metadata.name != playable.metadata.name ||
        roundTrip.metadata.creator != playable.metadata.creator ||
        roundTrip.metadata.description != playable.metadata.description ||
        roundTrip.metadata.playableExportVersion != playable.metadata.playableExportVersion ||
        roundTrip.layoutFingerprint != playable.layoutFingerprint ||
        roundTrip.verificationGhost.samples.size() != playable.verificationGhost.samples.size() ||
        roundTrip.verificationGhost.durationSeconds != playable.verificationGhost.durationSeconds) {
        error = "Playable package changed during save verification.";
        return false;
    }
    return true;
}

} // namespace

namespace PlayableTrackIO {

bool Save(const PlayableTrack& playable, const std::string& path, std::string& error) {
    if (!PlayablePackageValidation::Validate(playable, error)) return false;

    std::string serialized;
    if (!SerializeAndVerify(playable, serialized, error)) return false;
    if (!StoragePaths::EnsureParentDirectory(path, error)) return false;

    bool destinationExists = false;
    if (!StoragePaths::InspectRegularFile(path, kMaximumPackageFileBytes, destinationExists, error)) return false;
    if (destinationExists && !OpenReadableExistingPackage(path, error)) return false;

    return PersistenceInternal::WriteAtomically(
        path,
        kMaximumPackageFileBytes,
        "playable package",
        [&serialized](std::ostream& output, std::string& writeError) {
            output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
            if (output) return true;
            writeError = "Could not finish writing playable package.";
            return false;
        },
        error);
}

bool Load(const std::string& path, PlayableTrack& playable, std::string& error) {
    bool exists = false;
    if (!StoragePaths::InspectRegularFile(path, kMaximumPackageFileBytes, exists, error)) return false;
    if (!exists) {
        error = "No playable track found at " + path;
        return false;
    }
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        error = "Could not read playable track at " + path;
        return false;
    }

    PlayableTrack loaded;
    if (!PlayablePackageCodec::Read(input, loaded, error)) return false;
    playable = loaded;
    return true;
}

} // namespace PlayableTrackIO
