#include "neon_racer/persistence/playable_track_io.hpp"

#include "../internal/atomic_file_writer.hpp"
#include "../internal/playable_package_codec.hpp"
#include "../internal/playable_package_validation.hpp"
#include "../storage_paths.hpp"
#include "../../playable/playable_track.hpp"

#include <fstream>

namespace {

bool OpenReadableExistingPackage(const std::string& path, std::string& error) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (input) return true;
    error = "Cannot safely replace the existing playable package.";
    return false;
}

} // namespace

namespace PlayableTrackIO {

bool Save(const PlayableTrack& playable, const std::string& path, std::string& error) {
    if (!PlayablePackageValidation::Validate(playable, error)) return false;
    if (!StoragePaths::EnsureParentDirectory(path, error)) return false;

    bool destinationExists = false;
    if (!StoragePaths::InspectRegularFile(path, kMaximumPackageFileBytes, destinationExists, error)) return false;
    if (destinationExists && !OpenReadableExistingPackage(path, error)) return false;

    return PersistenceInternal::WriteAtomically(
        path,
        kMaximumPackageFileBytes,
        "playable package",
        [&playable](std::ostream& output, std::string& writeError) {
            return PlayablePackageCodec::Write(output, playable, writeError);
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
