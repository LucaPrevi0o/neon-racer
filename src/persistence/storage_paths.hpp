#pragma once

#include <cstddef>
#include <string>

// Shared user-data paths and safe filename helpers for persistent custom
// content. Format-specific modules choose their own subdirectory and suffix.
namespace StoragePaths {

std::string DataDirectory();
std::string SafeFileStem(const std::string& name);
bool EnsureDirectory(const std::string& path, std::string& error);
bool EnsureParentDirectory(const std::string& path, std::string& error);

// Inspects a persistence target without opening special files. `exists` is
// false only when the path does not exist; symlinks, FIFOs, directories, and
// files larger than `maximumBytes` fail explicitly before callers open them.
bool InspectRegularFile(const std::string& path, std::size_t maximumBytes, bool& exists, std::string& error);

} // namespace StoragePaths
