#pragma once

#include <string>
#include <vector>

#include "../track/track.hpp"

namespace DraftIO {

// Editable drafts intentionally have no verification replay or playable-export
// status. They belong exclusively to the custom-track storage space.
const char* DefaultCustomDraftPath();
std::string CustomDraftPath(const std::string& name);
bool ListCustomDrafts(std::vector<std::string>& names, std::string& error);
bool Save(const Track& track, const std::string& path, std::string& error);
bool Load(const std::string& path, Track& track, std::string& error);

} // namespace DraftIO
