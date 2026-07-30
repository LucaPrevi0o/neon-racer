#include "playable_track_io.hpp"

#include "format_reading.hpp"
#include "storage_paths.hpp"
#include "track_layout_codec.hpp"
#include "../race/ghost_replay.hpp"
#include "../track/track_fingerprint.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <streambuf>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

const int kPlayableFormatVersion = 1;
const int kGhostFormatVersion = 1;
const std::size_t kMaximumMetadataBytes = 4096u;

std::string CustomPlayableDirectory() { return StoragePaths::DataDirectory() + "/playables"; }

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

bool OpenReadableExistingPackage(const std::string& path, std::string& error) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (input) return true;
    error = "Cannot safely replace the existing playable package.";
    return false;
}

bool PlayableFileNameLess(const PlayableTrackIO::PlayableTrackFile& first,
                          const PlayableTrackIO::PlayableTrackFile& second) {
    return first.displayName < second.displayName;
}

bool HasLineBreak(const std::string& value) {
    return value.find('\n') != std::string::npos || value.find('\r') != std::string::npos;
}

bool HasUnsupportedControlCharacter(const std::string& value) {
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        const unsigned char valueByte = static_cast<unsigned char>(*character);
        if (valueByte < 32u || valueByte == 127u) return true;
    }
    return false;
}

bool ConsumeLineBreak(std::istream& input) {
    char character = '\0';
    if (!input.get(character)) return false;
    if (character == '\n') return true;
    if (character != '\r') return false;
    return input.get(character) && character == '\n';
}

bool WriteTextField(std::ostream& output, const char* label, const std::string& value, std::string& error) {
    if (value.size() > kMaximumMetadataBytes || HasLineBreak(value) || HasUnsupportedControlCharacter(value)) {
        error = "Playable metadata contains an unsupported value.";
        return false;
    }
    output << label << " " << value.size() << "\n";
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output << "\n";
    if (output) return true;
    error = "Could not write playable metadata.";
    return false;
}

bool ReadTextField(std::istream& input, const char* expectedLabel, std::string& value, std::string& error) {
    std::string label;
    std::size_t byteCount = 0;
    if (!FormatReading::ReadToken(input, label) || !(input >> byteCount) || label != expectedLabel ||
        byteCount > kMaximumMetadataBytes ||
        !ConsumeLineBreak(input)) {
        error = std::string("Playable ") + expectedLabel + " metadata is invalid.";
        return false;
    }

    std::string loaded(byteCount, '\0');
    if (byteCount != 0) input.read(&loaded[0], static_cast<std::streamsize>(byteCount));
    if (!input || !ConsumeLineBreak(input) || HasLineBreak(loaded) || HasUnsupportedControlCharacter(loaded)) {
        error = std::string("Playable ") + expectedLabel + " metadata is truncated or invalid.";
        return false;
    }
    value = loaded;
    return true;
}

bool HasVisibleText(const std::string& value) {
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        if (*character != ' ' && *character != '\t') return true;
    }
    return false;
}

bool ValidatePackage(const PlayableTrack& playable, std::string& error) {
    if (!playable.layout.Validate().raceReady) {
        error = "Playable package layout is not race-ready.";
        return false;
    }
    if (!HasVisibleText(playable.metadata.name) || !HasVisibleText(playable.metadata.creator) ||
        !HasVisibleText(playable.metadata.description) || playable.metadata.playableExportVersion == 0 ||
        HasLineBreak(playable.metadata.name) || HasLineBreak(playable.metadata.creator) ||
        HasLineBreak(playable.metadata.description) || HasUnsupportedControlCharacter(playable.metadata.name) ||
        HasUnsupportedControlCharacter(playable.metadata.creator) ||
        HasUnsupportedControlCharacter(playable.metadata.description)) {
        error = "Playable package metadata is incomplete or invalid.";
        return false;
    }
    const std::uint64_t fingerprint = TrackFingerprint::Calculate(playable.layout);
    if (playable.layoutFingerprint != fingerprint || playable.verificationGhost.layoutFingerprint != fingerprint) {
        error = "Playable package verification does not match its layout.";
        return false;
    }
    if (!GhostReplay::IsValidVerifiedData(playable.verificationGhost)) {
        error = "Playable package contains an invalid verification ghost.";
        return false;
    }
    return true;
}

bool WritePackage(std::ostream& output, const PlayableTrack& playable, std::string& error) {
    output << "NEON_RACER_PLAYABLE " << kPlayableFormatVersion << "\n";
    output << "STATUS PLAYABLE\n";
    if (!WriteTextField(output, "NAME_BYTES", playable.metadata.name, error) ||
        !WriteTextField(output, "CREATOR_BYTES", playable.metadata.creator, error) ||
        !WriteTextField(output, "DESCRIPTION_BYTES", playable.metadata.description, error)) {
        return false;
    }
    output << "EXPORT_VERSION " << playable.metadata.playableExportVersion << "\n";
    output << "LAYOUT_FINGERPRINT " << playable.layoutFingerprint << "\n";
    output << "LAYOUT " << TrackLayoutCodec::kCurrentVersion << "\n";
    if (!TrackLayoutCodec::Write(output, playable.layout, error)) return false;

    output << std::setprecision(std::numeric_limits<float>::max_digits10);
    output << "VERIFICATION_GHOST " << kGhostFormatVersion << " "
           << playable.verificationGhost.samples.size() << " " << playable.verificationGhost.durationSeconds << "\n";
    for (std::vector<GhostSample>::const_iterator sample = playable.verificationGhost.samples.begin();
         sample != playable.verificationGhost.samples.end(); ++sample) {
        const RaceCar& car = sample->car;
        output << "GHOST_SAMPLE " << sample->time << " "
               << car.position.x << " " << car.position.y << " " << car.position.z << " "
               << car.velocity.x << " " << car.velocity.y << " " << car.velocity.z << " "
               << car.forward.x << " " << car.forward.y << " " << car.forward.z << " "
               << car.up.x << " " << car.up.y << " " << car.up.z << " "
               << car.headingRadians << " " << car.speed << "\n";
    }
    output << "END_PLAYABLE\n";
    if (output) return true;
    error = "Could not finish writing playable package.";
    return false;
}

bool ReadPackage(std::istream& input, PlayableTrack& playable, std::string& error) {
    std::string header;
    int version = 0;
    if (!FormatReading::ReadToken(input, header) || !(input >> version) || header != "NEON_RACER_PLAYABLE" ||
        version != kPlayableFormatVersion) {
        error = "Playable format is not supported.";
        return false;
    }

    std::string statusLabel;
    std::string status;
    if (!FormatReading::ReadToken(input, statusLabel) || !FormatReading::ReadToken(input, status) ||
        statusLabel != "STATUS" || status != "PLAYABLE") {
        error = "Playable status data is invalid.";
        return false;
    }

    PlayableTrack loaded;
    if (!ReadTextField(input, "NAME_BYTES", loaded.metadata.name, error) ||
        !ReadTextField(input, "CREATOR_BYTES", loaded.metadata.creator, error) ||
        !ReadTextField(input, "DESCRIPTION_BYTES", loaded.metadata.description, error)) {
        return false;
    }

    std::string exportVersionLabel;
    if (!FormatReading::ReadToken(input, exportVersionLabel) || !(input >> loaded.metadata.playableExportVersion) ||
        exportVersionLabel != "EXPORT_VERSION" || loaded.metadata.playableExportVersion == 0) {
        error = "Playable export version is invalid.";
        return false;
    }

    std::string fingerprintLabel;
    if (!FormatReading::ReadToken(input, fingerprintLabel) || !(input >> loaded.layoutFingerprint) ||
        fingerprintLabel != "LAYOUT_FINGERPRINT") {
        error = "Playable layout fingerprint is invalid.";
        return false;
    }

    std::string layoutLabel;
    int layoutVersion = 0;
    if (!FormatReading::ReadToken(input, layoutLabel) || !(input >> layoutVersion) || layoutLabel != "LAYOUT" ||
        layoutVersion < 1 || layoutVersion > TrackLayoutCodec::kCurrentVersion ||
        !TrackLayoutCodec::Read(input, layoutVersion, loaded.layout, error)) {
        error = "Playable layout is invalid: " + error;
        return false;
    }

    std::string ghostLabel;
    int ghostVersion = 0;
    std::size_t sampleCount = 0;
    if (!FormatReading::ReadToken(input, ghostLabel) ||
        !(input >> ghostVersion >> sampleCount >> loaded.verificationGhost.durationSeconds) ||
        ghostLabel != "VERIFICATION_GHOST" || ghostVersion != kGhostFormatVersion || sampleCount == 0 ||
        sampleCount > GhostReplayLimits::kMaximumVerifiedSampleCount ||
        !std::isfinite(loaded.verificationGhost.durationSeconds) ||
        loaded.verificationGhost.durationSeconds <= 0.0f ||
        loaded.verificationGhost.durationSeconds > GhostReplayLimits::kMaximumVerifiedDurationSeconds) {
        error = "Playable verification ghost header is invalid.";
        return false;
    }
    for (std::size_t index = 0; index < sampleCount; ++index) {
        std::string sampleLabel;
        GhostSample sample;
        RaceCar& car = sample.car;
        if (!FormatReading::ReadToken(input, sampleLabel) ||
            !(input >> sample.time >> car.position.x >> car.position.y >> car.position.z >>
              car.velocity.x >> car.velocity.y >> car.velocity.z >> car.forward.x >> car.forward.y >> car.forward.z >>
              car.up.x >> car.up.y >> car.up.z >> car.headingRadians >> car.speed) || sampleLabel != "GHOST_SAMPLE") {
            error = "Playable verification ghost sample is invalid.";
            return false;
        }
        loaded.verificationGhost.samples.push_back(sample);
    }

    std::string endLabel;
    if (!FormatReading::ReadToken(input, endLabel) || endLabel != "END_PLAYABLE") {
        error = "Playable package is truncated.";
        return false;
    }
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "Playable package contains unexpected trailing data.";
        return false;
    }

    const std::uint64_t serializedFingerprint = loaded.layoutFingerprint;
    const std::uint64_t expectedSerializedFingerprint = layoutVersion <= 5
        ? TrackFingerprint::CalculateLegacyFlatTwist(loaded.layout)
        : TrackFingerprint::Calculate(loaded.layout);
    if (serializedFingerprint != expectedSerializedFingerprint) {
        error = "Playable package verification does not match its layout.";
        return false;
    }

    // A v5 flat Twist is represented by a dedicated runtime sentinel after
    // import. Rebind the in-memory package and its ghost to the migrated
    // layout identity before normal validation and race use.
    const std::uint64_t migratedFingerprint = TrackFingerprint::Calculate(loaded.layout);
    loaded.layoutFingerprint = migratedFingerprint;
    loaded.verificationGhost.layoutFingerprint = migratedFingerprint;
    if (!ValidatePackage(loaded, error)) return false;
    playable = loaded;
    return true;
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

bool Save(const PlayableTrack& playable, const std::string& path, std::string& error) {
    if (!ValidatePackage(playable, error)) return false;
    if (!StoragePaths::EnsureParentDirectory(path, error)) return false;

    bool destinationExists = false;
    if (!StoragePaths::InspectRegularFile(path, kMaximumPackageFileBytes, destinationExists, error)) return false;
    if (destinationExists && !OpenReadableExistingPackage(path, error)) return false;

    std::string temporaryPattern = path + ".tmp.XXXXXX";
    std::vector<char> temporaryCharacters(temporaryPattern.begin(), temporaryPattern.end());
    temporaryCharacters.push_back('\0');
    const int descriptor = mkstemp(&temporaryCharacters[0]);
    if (descriptor < 0) {
        error = std::string("Could not create temporary playable package: ") + std::strerror(errno);
        return false;
    }
    const std::string temporaryPath(&temporaryCharacters[0]);
    bool completed = false;
    bool closed = false;
    {
        FileDescriptorStreamBuffer buffer(descriptor, kMaximumPackageFileBytes);
        {
            std::ostream output(&buffer);
            if (WritePackage(output, playable, error)) {
                output.flush();
                completed = static_cast<bool>(output);
            }
        }
        closed = buffer.Close();
    }
    if (!completed || !closed) {
        std::remove(temporaryPath.c_str());
        if (error.empty()) error = "Could not finish writing playable package.";
        return false;
    }
    if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
        error = std::string("Could not finalize playable package: ") + std::strerror(errno);
        std::remove(temporaryPath.c_str());
        return false;
    }
    return true;
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
    if (!ReadPackage(input, loaded, error)) return false;
    playable = loaded;
    return true;
}

} // namespace PlayableTrackIO
