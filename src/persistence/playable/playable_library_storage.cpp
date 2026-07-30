#include "neon_racer/persistence/playable_track_io.hpp"

#include "../storage_paths.hpp"
#include "../../playable/playable_track.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <limits>

namespace {

std::string CustomPlayableDirectory() { return StoragePaths::DataDirectory() + "/playables"; }

bool PlayableFileNameLess(const PlayableTrackIO::PlayableTrackFile& first,
                          const PlayableTrackIO::PlayableTrackFile& second) {
    return first.displayName < second.displayName;
}

} // namespace

namespace PlayableTrackIO {

std::string CustomPlayablePath(const std::string& name) {
    return CustomPlayableDirectory() + "/" + StoragePaths::SafeFileStem(name) + ".nrplay";
}

bool ListCustomPlayableTracks(std::vector<PlayableTrackFile>& files, std::string& error) {
    files.clear();
    const std::string directoryPath = CustomPlayableDirectory();
    DIR* directory = opendir(directoryPath.c_str());
    if (directory == 0) {
        if (errno == ENOENT) return true;
        error = std::string("Could not read playable track folder: ") + std::strerror(errno);
        return false;
    }

    dirent* entry = 0;
    const std::string suffix = ".nrplay";
    while ((entry = readdir(directory)) != 0) {
        const std::string filename(entry->d_name);
        if (filename.size() > suffix.size() &&
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
            const std::string path = directoryPath + "/" + filename;
            bool exists = false;
            std::string ignoredError;
            if (StoragePaths::InspectRegularFile(path, kMaximumPackageFileBytes, exists, ignoredError) && exists) {
                files.push_back(PlayableTrackFile{filename.substr(0, filename.size() - suffix.size()), path});
            }
        }
    }
    closedir(directory);
    std::sort(files.begin(), files.end(), PlayableFileNameLess);
    return true;
}

bool NextExportVersion(const std::string& name, std::uint32_t& version, std::string& error) {
    const std::string path = CustomPlayablePath(name);
    bool exists = false;
    if (!StoragePaths::InspectRegularFile(path, kMaximumPackageFileBytes, exists, error)) {
        error = "Cannot replace existing playable package: " + error;
        return false;
    }
    if (!exists) {
        version = 1;
        return true;
    }

    PlayableTrack existing;
    if (!Load(path, existing, error)) {
        error = "Cannot replace existing playable package: " + error;
        return false;
    }
    if (existing.metadata.name != name) {
        error = "The chosen export name collides with a different saved playable package.";
        return false;
    }
    if (existing.metadata.playableExportVersion == std::numeric_limits<std::uint32_t>::max()) {
        error = "Playable export version has reached its maximum value.";
        return false;
    }
    version = existing.metadata.playableExportVersion + 1;
    return true;
}

} // namespace PlayableTrackIO
