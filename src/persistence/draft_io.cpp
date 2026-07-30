#include "draft_io.hpp"
#include "format_reading.hpp"
#include "storage_paths.hpp"
#include "track_layout_codec.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <ostream>
#include <streambuf>
#include <unistd.h>
#include <vector>

namespace {

std::string CustomDraftDirectory() { return StoragePaths::DataDirectory() + "/tracks"; }
const char* kLegacyCustomDraftDirectory = "tracks/custom";
const std::size_t kMaximumDraftFileBytes = 2u * 1024u * 1024u;

class FileDescriptorStreamBuffer : public std::streambuf {
public:
    FileDescriptorStreamBuffer(int descriptor, std::size_t maximumBytes)
        : descriptor_(descriptor), maximumBytes_(maximumBytes), writtenBytes_(0), failed_(false) {}

    ~FileDescriptorStreamBuffer() { Close(); }

    bool Close() {
        if (descriptor_ < 0) return !failed_;
        const int descriptor = descriptor_;
        descriptor_ = -1;
        if (close(descriptor) == 0) return !failed_;
        failed_ = true;
        return false;
    }

protected:
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        if (count <= 0) return 0;
        const std::size_t requested = static_cast<std::size_t>(count);
        if (requested > maximumBytes_ - writtenBytes_) {
            errno = EFBIG;
            failed_ = true;
            return 0;
        }

        std::streamsize completed = 0;
        while (completed < count) {
            const ssize_t result = write(descriptor_, data + completed,
                                         static_cast<std::size_t>(count - completed));
            if (result > 0) {
                completed += static_cast<std::streamsize>(result);
                writtenBytes_ += static_cast<std::size_t>(result);
                continue;
            }
            if (result < 0 && errno == EINTR) continue;
            failed_ = true;
            break;
        }
        return completed;
    }

    int_type overflow(int_type character = traits_type::eof()) override {
        if (character == traits_type::eof()) return traits_type::not_eof(character);
        const char value = static_cast<char>(character);
        return xsputn(&value, 1) == 1 ? character : traits_type::eof();
    }

    int sync() override {
        if (descriptor_ < 0 || fsync(descriptor_) != 0) {
            failed_ = true;
            return -1;
        }
        return 0;
    }

private:
    int descriptor_;
    std::size_t maximumBytes_;
    std::size_t writtenBytes_;
    bool failed_;
};

bool WriteAtomically(const std::string& path,
                     const std::function<bool(std::ostream&, std::string&)>& writer,
                     std::string& error) {
    std::string temporaryPattern = path + ".tmp.XXXXXX";
    std::vector<char> temporaryCharacters(temporaryPattern.begin(), temporaryPattern.end());
    temporaryCharacters.push_back('\0');
    const int descriptor = mkstemp(&temporaryCharacters[0]);
    if (descriptor < 0) {
        error = std::string("Could not create temporary draft: ") + std::strerror(errno);
        return false;
    }

    const std::string temporaryPath(&temporaryCharacters[0]);
    bool completed = false;
    bool closed = false;
    {
        FileDescriptorStreamBuffer buffer(descriptor, kMaximumDraftFileBytes);
        {
            std::ostream output(&buffer);
            if (writer(output, error)) {
                output.flush();
                completed = static_cast<bool>(output);
            }
        }
        closed = buffer.Close();
    }

    if (!completed || !closed) {
        std::remove(temporaryPath.c_str());
        if (error.empty()) error = "Could not finish writing draft.";
        return false;
    }
    if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
        error = std::string("Could not finalize draft: ") + std::strerror(errno);
        std::remove(temporaryPath.c_str());
        return false;
    }
    return true;
}

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
        if (!WriteAtomically(destination,
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

    return WriteAtomically(path,
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
