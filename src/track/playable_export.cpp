#include "playable_export.hpp"

namespace PlayableExport {

bool Build(const Track& draft, const std::string& name, PlayableTrack& playable, std::string& error) {
    if (!draft.Validate().raceReady) {
        error = "Only a race-ready layout with a start/finish line can be exported.";
        return false;
    }

    playable.layout = draft;
    playable.metadata.name = name.empty() ? "Untitled track" : name;
    playable.metadata.playableExportVersion = 1;
    playable.sourceLayoutRevision = draft.LayoutRevision();
    return true;
}

} // namespace PlayableExport
