#include "editor.hpp"

#include <algorithm>

void TrackEditor::CycleSelection() {
    const std::vector<TrackPiece>& pieces = track_.Pieces();
    if (pieces.empty()) { selectedPieceId_ = 0; return; }
    std::vector<TrackPiece>::const_iterator found = std::find_if(pieces.begin(), pieces.end(),
        [this](const TrackPiece& piece) { return piece.id == selectedPieceId_; });
    selectedPieceId_ = (found == pieces.end() || ++found == pieces.end()) ? pieces.front().id : found->id;
    SetMessage("Component selected. Press F to copy it into the preview.");
}

void TrackEditor::PlacePreview() {
    const TrackPiece candidate = BuildPreview();
    if (track_.HasOverlappingGeometry(candidate)) { SetMessage("Placement blocked: track geometry overlaps."); return; }
    SaveUndoState();
    if (track_.Add(candidate) == 0) { undoStates_.pop_back(); SetMessage("Placement blocked: invalid component parameters."); return; }
    selectedPieceId_ = 0; preview_.id = 0;
    SetMessage("Component placed. Connect all entry and exit markers for a valid circuit.");
}

void TrackEditor::TransformSelected() {
    if (selectedPieceId_ == 0) { SetMessage("Select a component first."); return; }
    TrackPiece replacement = BuildPreview(); replacement.id = selectedPieceId_;
    if (track_.HasOverlappingGeometry(replacement)) { SetMessage("Transform blocked: track geometry overlaps."); return; }
    SaveUndoState();
    if (!track_.ReplacePiece(replacement)) { undoStates_.pop_back(); SetMessage("Transform blocked: invalid component."); return; }
    selectedPieceId_ = 0; preview_.id = 0; SetMessage("Component transformed and deselected.");
}

void TrackEditor::DuplicateSelected() {
    if (selectedPieceId_ == 0) { SetMessage("Select a component first."); return; }
    const TrackPiece* selected = track_.GetPiece(selectedPieceId_); if (selected == 0) return;
    preview_ = *selected; preview_.id = 0; MovePreview(1, 1); PlacePreview();
}

void TrackEditor::DeleteSelected() {
    if (selectedPieceId_ == 0) { SetMessage("Select a component first."); return; }
    SaveUndoState(); if (!track_.RemovePiece(selectedPieceId_)) { undoStates_.pop_back(); return; }
    selectedPieceId_ = 0; SetMessage("Component deleted.");
}

void TrackEditor::ClearTrack() {
    if (track_.Pieces().empty()) { SetMessage("Track is already empty."); return; }
    SaveUndoState();
    track_.Clear();
    selectedPieceId_ = 0;
    preview_.id = 0;
    SetMessage("Track cleared. Ctrl+Z restores it.");
}

void TrackEditor::SetStartFinish() {
    if (!track_.IsLayoutValid()) { SetMessage("Finish placement unlocks after all connectors form one closed loop."); return; }
    const TrackPiece* selected = track_.GetPiece(selectedPieceId_);
    if (selected == 0 || selected->type != TrackPieceType::Straight) { SetMessage("Start/finish must be placed on a selected straight."); return; }
    SaveUndoState(); track_.SetStartFinish(selectedPieceId_, RaceDirection::Forward);
    SetMessage("Start/finish line set to the selected straight.");
}
