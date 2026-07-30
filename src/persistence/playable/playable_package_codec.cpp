#include "../internal/playable_package_codec.hpp"

#include "../format_reading.hpp"
#include "../track_layout_codec.hpp"
#include "../internal/playable_package_validation.hpp"
#include "../../playable/playable_track.hpp"
#include "../../race/ghost_replay.hpp"
#include "../../track/track_fingerprint.hpp"

#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>
#include <vector>

namespace {

const int kPlayableFormatVersion = 1;
const int kGhostFormatVersion = 1;
const std::size_t kMaximumMetadataBytes = 4096u;

bool ConsumeLineBreak(std::istream& input) {
    char character = '\0';
    if (!input.get(character)) return false;
    if (character == '\n') return true;
    if (character != '\r') return false;
    return input.get(character) && character == '\n';
}

bool WriteTextField(std::ostream& output, const char* label, const std::string& value, std::string& error) {
    if (value.size() > kMaximumMetadataBytes || PlayablePackageValidation::HasLineBreak(value) ||
        PlayablePackageValidation::HasUnsupportedControlCharacter(value)) {
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
        byteCount > kMaximumMetadataBytes || !ConsumeLineBreak(input)) {
        error = std::string("Playable ") + expectedLabel + " metadata is invalid.";
        return false;
    }

    std::string loaded(byteCount, '\0');
    if (byteCount != 0) input.read(&loaded[0], static_cast<std::streamsize>(byteCount));
    if (!input || !ConsumeLineBreak(input) || PlayablePackageValidation::HasLineBreak(loaded) ||
        PlayablePackageValidation::HasUnsupportedControlCharacter(loaded)) {
        error = std::string("Playable ") + expectedLabel + " metadata is truncated or invalid.";
        return false;
    }
    value = loaded;
    return true;
}

} // namespace

namespace PlayablePackageCodec {

bool Write(std::ostream& output, const PlayableTrack& playable, std::string& error) {
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

bool Read(std::istream& input, PlayableTrack& playable, std::string& error) {
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
    if (!PlayablePackageValidation::Validate(loaded, error)) return false;
    playable = loaded;
    return true;
}

} // namespace PlayablePackageCodec
