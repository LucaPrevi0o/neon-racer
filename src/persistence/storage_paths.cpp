#include "storage_paths.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace {

std::string ParentDirectory(const std::string& path) {
    const std::string::size_type separator = path.find_last_of('/');
    return separator == std::string::npos ? "." : (separator == 0 ? "/" : path.substr(0, separator));
}

} // namespace

namespace StoragePaths {

std::string DataDirectory() {
    const char* overrideDirectory = std::getenv("NEON_RACER_DATA_DIR");
    if (overrideDirectory != 0 && *overrideDirectory != '\0') return overrideDirectory;
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData != 0 && *xdgData != '\0') return std::string(xdgData) + "/neon-racer";
    const char* home = std::getenv("HOME");
    if (home != 0 && *home != '\0') return std::string(home) + "/.local/share/neon-racer";
    return "/tmp/neon-racer";
}

std::string SafeFileStem(const std::string& name) {
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

bool EnsureDirectory(const std::string& path, std::string& error) {
    std::string current;
    for (std::size_t index = 0; index < path.size(); ++index) {
        current += path[index];
        if (path[index] != '/' && index + 1 != path.size()) continue;
        if (current.empty() || current == "/") continue;
        if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            error = std::string("Could not create data directory: ") + std::strerror(errno);
            return false;
        }
    }
    return true;
}

bool EnsureParentDirectory(const std::string& path, std::string& error) {
    return EnsureDirectory(ParentDirectory(path), error);
}

bool InspectRegularFile(const std::string& path, std::size_t maximumBytes, bool& exists, std::string& error) {
    exists = false;
    struct stat status;
    if (lstat(path.c_str(), &status) != 0) {
        if (errno == ENOENT) return true;
        error = std::string("Could not inspect persisted file: ") + std::strerror(errno);
        return false;
    }
    if (!S_ISREG(status.st_mode)) {
        error = "Persisted path is not a regular file.";
        return false;
    }
    if (status.st_size < 0 || static_cast<unsigned long long>(status.st_size) >
                               static_cast<unsigned long long>(maximumBytes)) {
        error = "Persisted file exceeds the supported size limit.";
        return false;
    }
    exists = true;
    return true;
}

} // namespace StoragePaths
