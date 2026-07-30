#include "draft_io.hpp"
#include "format_reading.hpp"
#include "internal/atomic_file_writer.hpp"
#include "storage_paths.hpp"
#include "track_layout_codec.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <ostream>

namespace {

std::string CustomDraftDirectory() { return StoragePaths::DataDirectory() + "/tracks"; }
const char* kLegacyCustomDraftDirectory = "tracks/custom";
const std::size_t kMaximumDraftFileBytes = 2u * 1024u * 1024u;

bool ImportLegacyDrafts(std::string& error) {
    DIR* legacy = opendir(kLegacyCustomDraftDirectory);
    if (legacy == 0) {
        if (errno == ENOENT) return true;
        error = std::string("Could not read legacy draft folder: ") + std::strerror(errno);
        return false;
    }
    dirent* entry = 0;
    while ((entry = readdir(legacy)) != 0) {
        const std::string filename(entry->d_name);
        const std::string suffix = ".draft";
        if (filename.size() <= suffix.size() ||
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
            continue;
        }

        const std::string source = std::string(kLegacyCustomDraftDirectory) + "/" + filename;
        const std::string destination = CustomDraftDirectory() + "/" + filename;
        bool sourceExists = false;
        if (!StoragePaths::InspectRegularFile(source, kMaximumDraftFileBytes, sourceExists, error)) {
            closedir(legacy);
            return false;
        }
        if (!sourceExists) continue;

        bool destinationExists = false;
        if (!StoragePaths::InspectRegularFile(destination, kMaximumDraftFileBytes, destinationExists, error)) {
            closedir(legacy);
            return false;
        }
        if (destinationExists) continue;
        if (!StoragePaths::EnsureDirectory(CustomDraftDirectory(), error)) {
            closedir(legacy);
            return false;
        }

        std::ifstream input(source.c_str(), std::ios::binary);
        if (!input) {
            closedir(legacy);
            error = "Could not import legacy draft: " + filename;
            return false;
        }
        if (!PersistenceInternal::WriteAtomically(
                destination,
                kMaximumDraftFileBytes,
                "draft",
                [&input, &filename](std::ostream& output, std::string& writeError) {
                    output << input.rdbuf();
                    if (input.bad()) {
                        writeError = "Could not read legacy draft during import: " + filename;
                        return false;
                    }
                    if (!output) {
                        writeError = "Could not finish importing legacy draft: " + filename;
                        return false;
                    }
                    return true;
                },
                error)) {
            closedir(legacy);
            return false;
        }
    }
    closedir(legacy);
    return true;
}

} // namespace

namespace DraftIO {

const char* DefaultCustomDraftPath() {
    static const std::string path = CustomDraftDirectory() + "/untitled.draft";
    return path.c_str();
}

std::string CustomDraftPath(const std::string& name) {
    return CustomDraftDirectory() + "/" + StoragePaths::SafeFileStem(name) + ".draft";
}

bool ListCustomDrafts(std::vector<std::string>& names, std::string& error) {
    names.clear();
    if (!ImportLegacyDrafts(error)) return false;
    const std::string path = CustomDraftDirectory();
    DIR* directory = opendir(path.c_str());
    if (directory == 0) {
        if (errno == ENOENT) return true;
        error = std::string("Could not read custom draft folder: ") + std::strerror(errno);
        return false;
    }
    dirent* entry = 0;
    while ((entry = readdir(directory)) != 0) {
        const std::string filename(entry->d_name);
        const std::string suffix = ".draft";
        if (filename.size() > suffix.size() &&
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
            names.push_back(filename.substr(0, filename.size() - suffix.size()));
        }
    }
    closedir(directory);
    std::sort(names.begin(), names.end());
    return true;
}

bool Save(const Track& track, const std::string& path, std::string& error) {
    if (track.Pieces().size() > TrackLayoutCodec::kMaximumPieceCount) {
        error = "Track layout has too many pieces to persist.";
        return false;
    }
    if (!StoragePaths::EnsureParentDirectory(path, error)) return false;
    bool destinationExists = false;
    if (!StoragePaths::InspectRegularFile(path, kMaximumDraftFileBytes, destinationExists, error)) return false;

    return PersistenceInternal::WriteAtomically(
        path,
        kMaximumDraftFileBytes,
        "draft",
        [&track](std::ostream& output, std::string& writeError) {
            // Version 6 records an explicit corkscrew radius while preserving the
            // original flat-Twist semantics when importing version 5 and older drafts.
            // Drafts remain editable regardless of validity and never contain replay data.
            output << "NEON_RACER_DRAFT " << TrackLayoutCodec::kCurrentVersion << "\n";
            output << "STATUS DRAFT\n";
            if (TrackLayoutCodec::Write(output, track, writeError)) return true;
            if (writeError.empty()) writeError = "Could not finish writing draft.";
            return false;
        },
        error);
}

bool Load(const std::string& path, Track& track, std::string& error) {
    bool exists = false;
    if (!StoragePaths::InspectRegularFile(path, kMaximumDraftFileBytes, exists, error)) {
        error = "Draft is invalid: " + error;
        return false;
    }
    if (!exists) {
        error = "No custom draft found at " + path;
        return false;
    }
    std::ifstream file(path.c_str());
    if (!file) {
        error = "No custom draft found at " + path;
        return false;
    }

    std::string header;
    int version = 0;
    if (!FormatReading::ReadToken(file, header) || !(file >> version) || header != "NEON_RACER_DRAFT" ||
        (version < 1 || version > TrackLayoutCodec::kCurrentVersion)) {
        error = "Draft format is not supported.";
        return false;
    }

    if (version >= 2) {
        std::string statusLabel;
        std::string status;
        if (!FormatReading::ReadToken(file, statusLabel) || !FormatReading::ReadToken(file, status) ||
            statusLabel != "STATUS" || status != "DRAFT") {
            error = "Draft status data is invalid.";
            return false;
        }
    }

    if (TrackLayoutCodec::Read(file, version, track, error)) return true;
    error = "Draft is invalid: " + error;
    return false;
}

} // namespace DraftIO
