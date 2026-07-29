#include "editor.hpp"

// History snapshots intentionally store the complete value-type Track. This
// keeps every editor action atomic and avoids UI code owning mutation details.
void TrackEditor::Undo() {
    if (undoStates_.empty()) { SetMessage("Nothing to undo."); return; }
    redoStates_.push_back(track_);
    track_ = undoStates_.back();
    undoStates_.pop_back();
    if (track_.GetPiece(selectedPieceId_) == 0) selectedPieceId_ = 0;
    SetMessage("Edit undone.");
}

void TrackEditor::Redo() {
    if (redoStates_.empty()) { SetMessage("Nothing to redo."); return; }
    undoStates_.push_back(track_);
    track_ = redoStates_.back();
    redoStates_.pop_back();
    if (track_.GetPiece(selectedPieceId_) == 0) selectedPieceId_ = 0;
    SetMessage("Edit restored.");
}

void TrackEditor::SaveUndoState() {
    undoStates_.push_back(track_);
    redoStates_.clear();
}
