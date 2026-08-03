#include "editor.hpp"

#include "../persistence/draft_io.hpp"

void TrackEditor::RefreshDraftList() {
    std::string error;
    if (!DraftIO::ListCustomDrafts(savedDrafts_, error)) SetMessage(error.c_str());
}

void TrackEditor::SaveDraft() {
    if (currentDraftName_.empty()) {
        BeginSaveDraft();
        return;
    }
    draftName_ = currentDraftName_;
    SaveNamedDraft();
}

void TrackEditor::BeginSaveDraft() {
    libraryOpen_ = true;
    if (!currentDraftName_.empty()) {
        SaveDraft();
        return;
    }
    namingDraft_ = true;
    draftName_.clear();
    SetMessage("Enter a custom track name in the library.");
}

void TrackEditor::SaveNamedDraft() {
    if (draftName_.empty()) {
        SetMessage("Enter a draft name before saving.");
        return;
    }

    std::string error;
    const bool updatingCurrentDraft = !currentDraftName_.empty() && currentDraftName_ == draftName_;
    const std::string path = DraftIO::CustomDraftPath(draftName_);
    if (DraftIO::Save(track_, path, error)) {
        currentDraftName_ = draftName_;
        namingDraft_ = false;
        RefreshDraftList();
        SetMessage(updatingCurrentDraft
                       ? "Updated editable draft " + currentDraftName_ + " in the current format."
                       : "Custom editable draft saved as " + currentDraftName_ + ".");
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
    currentDraftName_ = name;
    draftName_ = name;
    selectedPieceId_ = 0;
    preview_.id = 0;
    libraryOpen_ = false;
    SetMessage("Custom editable draft loaded: " + name + ". Ctrl+S updates this file.");
}
