#include "editor.hpp"

#include "../persistence/draft_io.hpp"

void TrackEditor::RefreshDraftList() {
    std::string error;
    if (!DraftIO::ListCustomDrafts(savedDrafts_, error)) SetMessage(error.c_str());
}

void TrackEditor::BeginSaveDraft() {
    libraryOpen_ = true;
    namingDraft_ = true;
    draftName_.clear();
    SetMessage("Enter a custom track name in the library.");
}

void TrackEditor::SaveNamedDraft() {
    std::string error;
    const std::string path = DraftIO::CustomDraftPath(draftName_);
    if (DraftIO::Save(track_, path, error)) {
        namingDraft_ = false;
        RefreshDraftList();
        SetMessage("Custom editable draft saved as " + draftName_ + ".");
    } else {
        SetMessage(error.c_str());
    }
}

void TrackEditor::LoadDraft(const std::string& name) {
    std::string error;
    Track loaded;
    if (!DraftIO::Load(DraftIO::CustomDraftPath(name), loaded, error)) {
        SetMessage(error.c_str());
        return;
    }
    SaveUndoState();
    track_ = loaded;
    selectedPieceId_ = 0;
    preview_.id = 0;
    libraryOpen_ = false;
    SetMessage("Custom editable draft loaded: " + name + ".");
}
