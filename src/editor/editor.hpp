#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../track/track.hpp"

class TrackEditor {
public:
    TrackEditor();

    void Update(Camera3D& camera);
    void DrawTrack3D() const;
    void DrawInterface() const;

    const Track& GetTrack() const;

private:
    TrackPiece BuildPreview() const;
    bool MouseGridPosition(const Camera3D& camera, GridPosition& position) const;
    std::uint32_t PickPieceAtMouse(const Camera3D& camera) const;
    void MovePreview(int x, int z);
    void RotatePreview();
    void ChangeDimension(int amount);
    void CycleProperty(int direction);
    void AdjustSelectedProperty(int direction);
    int PropertyCount() const;
    const char* PropertyName(int index) const;
    std::string PropertyValue(int index) const;
    float PropertyFraction(int index) const;
    void DrawPropertyPanel() const;
    bool PreviewOverlaps(const TrackPiece& candidate) const;
    void CycleSelection();
    void PlacePreview();
    void TransformSelected();
    void DuplicateSelected();
    void DeleteSelected();
    void SetStartFinish();
    bool UpdateTrackLibraryInput();
    void DrawTrackLibrary() const;
    void RefreshDraftList();
    void BeginSaveDraft();
    void SaveNamedDraft();
    void LoadDraft(const std::string& name);
    void Undo();
    void Redo();
    void SaveUndoState();
    void SetMessage(const std::string& message);

    Track track_;
    TrackPiece preview_;
    std::uint32_t selectedPieceId_;
    int selectedPropertyIndex_;
    std::vector<Track> undoStates_;
    std::vector<Track> redoStates_;
    std::string message_;
    bool libraryOpen_;
    bool helpPanelExpanded_;
    bool namingDraft_;
    std::string draftName_;
    std::vector<std::string> savedDrafts_;
    mutable bool previewOverlapCacheValid_;
    mutable TrackPiece cachedPreview_;
    mutable std::uint32_t cachedPreviewRevision_;
    mutable bool cachedPreviewOverlaps_;
};
