#include "neon_racer/editor/editor.hpp"

namespace {

bool SamePieceShape(const TrackPiece& first, const TrackPiece& second) {
    return first.id == second.id && first.type == second.type && first.entryPosition == second.entryPosition &&
        first.entryHeading == second.entryHeading && first.width == second.width && first.exitWidth == second.exitWidth &&
        first.length == second.length && first.curveTurn == second.curveTurn && first.curveRadius == second.curveRadius &&
        first.curveDegrees == second.curveDegrees && first.bankAngleDegrees == second.bankAngleDegrees &&
        first.elevationDelta == second.elevationDelta && first.lateralOffset == second.lateralOffset &&
        first.material == second.material;
}

} // namespace

TrackEditor::TrackEditor()
    : track_(),
      preview_{0, TrackPieceType::Straight, GridPosition{0, 0, -5}, Heading::East, 5, 5, 4,
               CurveTurn::Right, 4, 90, 0, 0, 0, SurfaceMaterial::Regular},
      selectedPieceId_(0),
      selectedPropertyIndex_(0),
      message_("Start with an empty track: place a component or load a draft."),
      libraryOpen_(false),
      helpPanelExpanded_(true),
      trackingGraphVisible_(false),
      piecePalette_(),
      namingDraft_(false),
      draftName_("untitled"),
      currentDraftName_(),
      previewOverlapCacheValid_(false),
      cachedPreview_(preview_),
      cachedPreviewRevision_(0),
      cachedPreviewOverlaps_(false) {
}

void TrackEditor::BeginNewTrack() {
    *this = TrackEditor();
    SetMessage("New empty track ready. Saved drafts are unchanged.");
}

const Track& TrackEditor::GetTrack() const { return track_; }
bool TrackEditor::IsRaceReady() const { return track_.Validate().raceReady; }
bool TrackEditor::HasSavedDraft() const { return !currentDraftName_.empty(); }

TrackPiece TrackEditor::BuildPreview() const {
    TrackPiece candidate = preview_;
    candidate.id = selectedPieceId_;
    return candidate;
}

bool TrackEditor::PreviewOverlaps(const TrackPiece& candidate) const {
    const std::uint32_t revision = track_.LayoutRevision();
    if (!previewOverlapCacheValid_ || cachedPreviewRevision_ != revision || !SamePieceShape(cachedPreview_, candidate)) {
        cachedPreview_ = candidate;
        cachedPreviewRevision_ = revision;
        cachedPreviewOverlaps_ = track_.HasOverlappingGeometry(candidate);
        previewOverlapCacheValid_ = true;
    }
    return cachedPreviewOverlaps_;
}

void TrackEditor::SetMessage(const std::string& message) { message_ = message; }
