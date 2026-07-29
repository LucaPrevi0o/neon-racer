#include "draft_io.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sys/stat.h>

namespace {

std::string DataDirectory() {
    const char* overrideDirectory = std::getenv("NEON_RACER_DATA_DIR");
    if (overrideDirectory != 0 && *overrideDirectory != '\0') return overrideDirectory;
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData != 0 && *xdgData != '\0') return std::string(xdgData) + "/neon-racer";
    const char* home = std::getenv("HOME");
    if (home != 0 && *home != '\0') return std::string(home) + "/.local/share/neon-racer";
    return "/tmp/neon-racer";
}

std::string CustomDraftDirectory() { return DataDirectory() + "/tracks"; }

std::string ParentDirectory(const std::string& path) {
    const std::string::size_type separator = path.find_last_of('/');
    return separator == std::string::npos ? "." : (separator == 0 ? "/" : path.substr(0, separator));
}

bool EnsureDirectory(const std::string& path, std::string& error) {
    std::string current;
    for (std::size_t index = 0; index < path.size(); ++index) {
        current += path[index];
        if (path[index] != '/' && index + 1 != path.size()) continue;
        if (current.empty() || current == "/") continue;
        if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            error = std::string("Could not create draft directory: ") + std::strerror(errno);
            return false;
        }
    }
    return true;
}

std::string SafeDraftName(const std::string& name) {
    std::string result;
    for (std::string::const_iterator character = name.begin(); character != name.end(); ++character) {
        const unsigned char value = static_cast<unsigned char>(*character);
        if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '_' || value == '-') {
            result += static_cast<char>(value);
        } else if (value == ' ') {
            result += '_';
        }
    }
    return result.empty() ? "untitled" : result;
}

} // namespace

namespace DraftIO {

const char* DefaultCustomDraftPath() {
    static const std::string path = CustomDraftDirectory() + "/untitled.draft";
    return path.c_str();
}

std::string CustomDraftPath(const std::string& name) {
    return CustomDraftDirectory() + "/" + SafeDraftName(name) + ".draft";
}

bool ListCustomDrafts(std::vector<std::string>& names, std::string& error) {
    names.clear();
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
    if (!EnsureDirectory(ParentDirectory(path), error)) return false;
    std::ofstream file(path.c_str());
    if (!file) {
        error = "Could not open draft for writing: " + path;
        return false;
    }

    // Version 5 adds curve banking alongside the physics-relevant road parameters. Drafts remain
    // editable regardless of validity and deliberately never contain replay data.
    file << "NEON_RACER_DRAFT 5\n";
    file << "STATUS DRAFT\n";
    file << "START " << track.StartFinishPieceId() << " "
         << static_cast<int>(track.SelectedRaceDirection()) << "\n";
    file << "PIECES " << track.Pieces().size() << "\n";
    for (std::vector<TrackPiece>::const_iterator piece = track.Pieces().begin();
         piece != track.Pieces().end(); ++piece) {
        file << "PIECE " << piece->id << " " << static_cast<int>(piece->type) << " "
             << piece->entryPosition.x << " " << piece->entryPosition.y << " " << piece->entryPosition.z << " "
             << static_cast<int>(piece->entryHeading) << " " << piece->width << " " << piece->exitWidth << " "
             << piece->length << " " << static_cast<int>(piece->curveTurn) << " " << piece->curveRadius << " "
             << piece->elevationDelta << " " << piece->lateralOffset << " "
             << static_cast<int>(piece->material) << " " << piece->curveDegrees << " "
             << piece->bankAngleDegrees << "\n";
    }
    if (!file) {
        error = "Could not finish writing draft: " + path;
        return false;
    }
    return true;
}

bool Load(const std::string& path, Track& track, std::string& error) {
    std::ifstream file(path.c_str());
    if (!file) {
        error = "No custom draft found at " + path;
        return false;
    }

    std::string header;
    int version = 0;
    if (!(file >> header >> version) || header != "NEON_RACER_DRAFT" ||
        (version != 1 && version != 2 && version != 3 && version != 4 && version != 5)) {
        error = "Draft format is not supported.";
        return false;
    }

    if (version >= 2) {
        std::string statusLabel;
        std::string status;
        if (!(file >> statusLabel >> status) || statusLabel != "STATUS" || status != "DRAFT") {
            error = "Draft status data is invalid.";
            return false;
        }
    }

    std::string startLabel;
    std::uint32_t savedStartId = 0;
    int savedDirection = 0;
    if (!(file >> startLabel >> savedStartId >> savedDirection) || startLabel != "START" ||
        (savedDirection != static_cast<int>(RaceDirection::Forward) &&
         savedDirection != static_cast<int>(RaceDirection::Reverse))) {
        error = "Draft start/finish data is invalid.";
        return false;
    }

    std::string piecesLabel;
    std::size_t pieceCount = 0;
    if (!(file >> piecesLabel >> pieceCount) || piecesLabel != "PIECES") {
        error = "Draft piece list is missing.";
        return false;
    }

    Track loaded;
    std::map<std::uint32_t, std::uint32_t> savedToLoadedId;
    for (std::size_t index = 0; index < pieceCount; ++index) {
        std::string pieceLabel;
        std::uint32_t savedId = 0;
        int type = 0;
        GridPosition entry{0, 0, 0};
        int heading = 0;
        int width = 0;
        int exitWidth = 0;
        int length = 0;
        int turn = 0;
        int radius = 0;
        int elevationDelta = 0;
        int lateralOffset = 0;
        int material = static_cast<int>(SurfaceMaterial::Regular);
        int curveDegrees = 90;
        int bankAngleDegrees = 0;
        if (!(file >> pieceLabel >> savedId >> type >> entry.x >> entry.y >> entry.z >> heading >> width) ||
            pieceLabel != "PIECE" || (type != static_cast<int>(TrackPieceType::Straight) &&
            type != static_cast<int>(TrackPieceType::Curve) && type != static_cast<int>(TrackPieceType::Loop) &&
            type != static_cast<int>(TrackPieceType::Twist) && type != static_cast<int>(TrackPieceType::Branch) &&
            type != static_cast<int>(TrackPieceType::Merge)) ||
            heading < static_cast<int>(Heading::North) || heading > static_cast<int>(Heading::West)) {
            error = "Draft contains an invalid piece.";
            return false;
        }

        if (version >= 3) {
            if (!(file >> exitWidth >> length >> turn >> radius >> elevationDelta >> lateralOffset >> material) ||
                material < static_cast<int>(SurfaceMaterial::Regular) ||
                material > static_cast<int>(SurfaceMaterial::HighResistance)) {
                error = "Draft contains invalid advanced piece parameters.";
                return false;
            }
            if (version >= 4 && (!(file >> curveDegrees) ||
                (type == static_cast<int>(TrackPieceType::Curve) &&
                                 (curveDegrees != 90 && curveDegrees != 180 && curveDegrees != 270)))) {
                error = "Draft contains an invalid curve extent.";
                return false;
            }
            if (version == 5 && (!(file >> bankAngleDegrees) || std::abs(bankAngleDegrees) > 45)) {
                error = "Draft contains an invalid curve bank angle.";
                return false;
            }
        } else {
            if (!(file >> length >> turn >> radius)) {
                error = "Draft contains an invalid piece.";
                return false;
            }
            exitWidth = width;
        }
        if (turn != static_cast<int>(CurveTurn::Left) && turn != static_cast<int>(CurveTurn::Right)) {
            error = "Draft contains an invalid curve turn.";
            return false;
        }

        std::uint32_t loadedId = 0;
        if (type == static_cast<int>(TrackPieceType::Straight)) {
            loadedId = loaded.AddStraight(entry, static_cast<Heading>(heading), length, width, exitWidth,
                                         elevationDelta, lateralOffset, static_cast<SurfaceMaterial>(material));
        } else if (type == static_cast<int>(TrackPieceType::Curve)) {
            loadedId = loaded.AddCurve(entry, static_cast<Heading>(heading), static_cast<CurveTurn>(turn), radius,
                                      width, exitWidth, elevationDelta, static_cast<SurfaceMaterial>(material),
                                      curveDegrees, bankAngleDegrees);
        } else if (type == static_cast<int>(TrackPieceType::Loop)) {
            loadedId = loaded.AddLoop(entry, static_cast<Heading>(heading), radius, width, exitWidth,
                                     elevationDelta, static_cast<SurfaceMaterial>(material), lateralOffset);
        } else {
            loadedId = type == static_cast<int>(TrackPieceType::Twist)
                ? loaded.AddTwist(entry, static_cast<Heading>(heading), length, width, exitWidth,
                                 elevationDelta, lateralOffset, static_cast<SurfaceMaterial>(material))
                : type == static_cast<int>(TrackPieceType::Branch)
                    ? loaded.AddBranch(entry, static_cast<Heading>(heading), length, lateralOffset, width,
                                       static_cast<SurfaceMaterial>(material))
                    : loaded.AddMerge(entry, static_cast<Heading>(heading), length, lateralOffset, width,
                                      static_cast<SurfaceMaterial>(material));
        }
        if (loadedId == 0) {
            error = "Draft piece overlaps existing geometry or has invalid dimensions.";
            return false;
        }
        savedToLoadedId[savedId] = loadedId;
    }

    if (savedStartId != 0) {
        const std::map<std::uint32_t, std::uint32_t>::const_iterator start = savedToLoadedId.find(savedStartId);
        if (start == savedToLoadedId.end() || !loaded.SetStartFinish(start->second, static_cast<RaceDirection>(savedDirection))) {
            error = "Draft start/finish piece is invalid.";
            return false;
        }
    }

    track = loaded;
    return true;
}

} // namespace DraftIO
