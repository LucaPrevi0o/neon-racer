#pragma once

#include <string>
#include <vector>

#include "neon_racer/persistence/playable_track_io.hpp"
#include "../track/track_contracts.hpp"

// Raylib-facing modal UI for frozen playable time-trial packages. The
// application owns state transitions and I/O results; this class only gathers
// player intent and presents the dedicated library/export forms.
class PlayableLibrary {
public:
    PlayableLibrary();

    bool IsOpen() const;
    void Open();
    void Close();
    void BeginExport(const TrackMetadata& defaults);
    void Update();
    void Draw() const;

    bool ConsumeLaunchRequest(std::string& path);
    bool ConsumeExportRequest(TrackMetadata& metadata);
    void ReportMessage(const std::string& message);
    void FinishExport(const std::string& message);

private:
    enum class Mode {
        Closed,
        Library,
        Export,
    };

    bool Refresh();
    void AppendCharacter(int character);
    void RemoveCharacter();
    std::string& ActiveField();
    const std::string& ActiveField() const;

    Mode mode_;
    TrackMetadata exportMetadata_;
    int activeField_;
    std::vector<PlayableTrackIO::PlayableTrackFile> playableFiles_;
    std::size_t firstVisibleIndex_;
    std::string pendingLaunchPath_;
    bool exportRequested_;
    std::string message_;
};
