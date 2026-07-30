#include "neon_racer/editor/editor.hpp"

#include "neon_racer/editor/piece_catalog.hpp"
#include "ui/neon.hpp"

#include <algorithm>

namespace {

const int kPanelX = 28;
const int kPanelY = 80;

Rectangle EditorPanelBounds() {
    return Rectangle{static_cast<float>(kPanelX), static_cast<float>(kPanelY), 470.0f, 350.0f};
}

Rectangle StartFinishButtonBounds() {
    return Rectangle{static_cast<float>(kPanelX + 18), 350.0f, 260.0f, 34.0f};
}

Rectangle ClearTrackButtonBounds() {
    return Rectangle{320.0f, 350.0f, 160.0f, 34.0f};
}

Rectangle HelpToggleBounds(bool expanded) {
    return expanded ? Rectangle{412.0f, 88.0f, 68.0f, 26.0f} :
        Rectangle{28.0f, 80.0f, 206.0f, 38.0f};
}

const char* PieceName(TrackPieceType type) {
    const EditorPieceCatalog::Item* item = EditorPieceCatalog::Find(type);
    return item != 0 ? item->name : "Unknown";
}

const char* MaterialName(SurfaceMaterial material) {
    switch (material) {
    case SurfaceMaterial::Regular: return "regular";
    case SurfaceMaterial::Slippery: return "slippery";
    case SurfaceMaterial::HighResistance: return "high-resistance";
    }
    return "unknown";
}

const char* ValidationHint(const TrackValidation& validation) {
    if (validation.raceReady) return "Closed loop and start/finish are ready.";
    if (validation.issues.empty()) return "Track needs one more step.";
    switch (validation.issues.front().kind) {
    case TrackIssueKind::NotOneClosedLoop: return "Join every open end into one closed loop.";
    case TrackIssueKind::DisconnectedEntry:
    case TrackIssueKind::DisconnectedExit: return "Connect the open cyan and pink ends.";
    case TrackIssueKind::OverlappingGeometry: return "Move pieces apart; roads cannot overlap.";
    case TrackIssueKind::MissingStartFinish: return "Choose a straight for start/finish.";
    case TrackIssueKind::InvalidStartFinish: return "Start/finish must be on a straight.";
    }
    return "Track needs attention.";
}

std::string TruncatePanelText(const std::string& text, int fontSize, int maximumWidth) {
    if (MeasureText(text.c_str(), fontSize) <= maximumWidth) return text;
    std::string clipped = text;
    while (!clipped.empty() && MeasureText((clipped + "...").c_str(), fontSize) > maximumWidth) {
        clipped.erase(clipped.size() - 1);
    }
    return clipped + "...";
}

} // namespace

void TrackEditor::DrawInterface() const {
    if (!helpPanelExpanded_) {
        const Rectangle button = HelpToggleBounds(false);
        const Neon::ButtonState buttonState = Neon::GetButtonState(button, !libraryOpen_);
        Neon::DrawButton(button, Neon::Cyan, buttonState);
        DrawText("SHOW EDITOR PANEL", 42, 92, 15,
                 libraryOpen_ ? Fade(RAYWHITE, 0.36f) :
                 (Neon::IsButtonHighlighted(buttonState) ? RAYWHITE : Neon::Cyan));
        DrawPropertyPanel();
        DrawTrackLibrary();
        piecePalette_.Draw(preview_.type);
        return;
    }

    const TrackValidation validation = track_.Validate();
    const bool isCurve = preview_.type == TrackPieceType::Curve;
    const int detailOffset = isCurve ? 23 : 0;
    Neon::DrawOverlayPanel(EditorPanelBounds());
    DrawText("TRACK EDITOR", kPanelX + 18, kPanelY + 18, 22, Neon::Cyan);

    const Rectangle helpButton = HelpToggleBounds(true);
    const Neon::ButtonState helpButtonState = Neon::GetButtonState(helpButton, !libraryOpen_);
    Neon::DrawButton(helpButton, Neon::Cyan, helpButtonState);
    DrawText("HIDE", 425, 94, 13, libraryOpen_ ? Fade(RAYWHITE, 0.36f) :
             (Neon::IsButtonHighlighted(helpButtonState) ? RAYWHITE : Neon::Cyan));

    DrawText("PREVIEW", kPanelX + 18, kPanelY + 48, 12, Neon::Cyan);
    DrawText(TextFormat("%s  |  %s", PieceName(preview_.type), HeadingName(preview_.entryHeading)),
             kPanelX + 18, kPanelY + 63, 17, RAYWHITE);

    const bool lengthBasedPiece = preview_.type == TrackPieceType::Straight ||
        preview_.type == TrackPieceType::Twist || preview_.type == TrackPieceType::Branch ||
        preview_.type == TrackPieceType::Merge;
    if (preview_.type == TrackPieceType::Twist) {
        const bool legacyFlatTwist = TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius);
        DrawText(legacyFlatTwist
                     ? TextFormat("GRID (%i, %i, %i)  |  LEGACY FLAT ROLL", preview_.entryPosition.x,
                                  preview_.entryPosition.y, preview_.entryPosition.z)
                     : TextFormat("GRID (%i, %i, %i)  |  RUN %i  |  R %.1f", preview_.entryPosition.x,
                                  preview_.entryPosition.y, preview_.entryPosition.z, preview_.length,
                                  TrackLimits::ResolveTwistRadius(preview_.length, preview_.curveRadius)),
                 kPanelX + 18, kPanelY + 89, 14, Fade(RAYWHITE, 0.86f));
    } else {
        DrawText(TextFormat("GRID (%i, %i, %i)  |  %s %i", preview_.entryPosition.x,
                            preview_.entryPosition.y, preview_.entryPosition.z,
                            lengthBasedPiece ? "LENGTH" : "RADIUS",
                            lengthBasedPiece ? preview_.length : preview_.curveRadius),
                 kPanelX + 18, kPanelY + 89, 14, Fade(RAYWHITE, 0.86f));
    }

    DrawText(TextFormat("ROAD %i > %i  |  RAMP %+i  |  %s", preview_.width, preview_.exitWidth,
                        preview_.elevationDelta, MaterialName(preview_.material)),
             kPanelX + 18, kPanelY + 111, 14, Neon::Pink);
    if (isCurve) {
        DrawText(TextFormat("CURVE %s  |  %i DEG  |  BANK %+i",
                            preview_.curveTurn == CurveTurn::Right ? "RIGHT" : "LEFT",
                            preview_.curveDegrees, preview_.bankAngleDegrees),
                 kPanelX + 18, kPanelY + 133, 14, Neon::Yellow);
    }

    const int statusY = kPanelY + 139 + detailOffset;
    const Color selectionColor = selectedPieceId_ == 0 ? Fade(RAYWHITE, 0.62f) : Neon::Yellow;
    const std::string selectedLabel = selectedPieceId_ == 0 ? "none" :
        "piece " + std::to_string(selectedPieceId_);
    DrawText(TextFormat("SELECTED: %s", selectedLabel.c_str()),
             kPanelX + 18, statusY, 14, selectionColor);

    const Color validationColor = validation.raceReady ? Neon::Green : Neon::Orange;
    DrawText(validation.raceReady ? "RACE READY" : "DRAFT", kPanelX + 18, statusY + 22, 16,
             validationColor);
    DrawText(ValidationHint(validation), kPanelX + 130, statusY + 24, 13,
             Fade(validationColor, 0.86f));
    const std::string panelMessage = TruncatePanelText(message_, 13, 434);
    DrawText(panelMessage.c_str(), kPanelX + 18, statusY + 47, 13, Neon::Yellow);
    DrawText("CONNECTORS: CYAN IN  |  PINK OUT", kPanelX + 18, statusY + 68, 13, Neon::Cyan);

    const bool layoutComplete = track_.IsLayoutValid();
    const TrackPiece* selected = track_.GetPiece(selectedPieceId_);
    const bool canSetStartFinish = !libraryOpen_ && layoutComplete && selected != 0 &&
        selected->type == TrackPieceType::Straight;
    const Rectangle startFinishButton = StartFinishButtonBounds();
    const Neon::ButtonState startFinishState = Neon::GetButtonState(startFinishButton, canSetStartFinish);
    Neon::DrawButton(startFinishButton, Neon::Green, startFinishState, canSetStartFinish);
    const char* startFinishLabel = libraryOpen_ ? "CLOSE LIBRARY TO EDIT" :
        !layoutComplete ? "CLOSE LOOP FIRST" :
        canSetStartFinish ? "SET START / FINISH" : "SELECT A STRAIGHT";
    const int startFinishSize = canSetStartFinish ? 14 : 13;
    DrawText(startFinishLabel,
             static_cast<int>(startFinishButton.x +
                              (startFinishButton.width - MeasureText(startFinishLabel, startFinishSize)) * 0.5f),
             static_cast<int>(startFinishButton.y) + 9, startFinishSize,
             canSetStartFinish ? BLACK : Fade(RAYWHITE, 0.60f));

    const bool canClear = !libraryOpen_ && !track_.Pieces().empty();
    const Rectangle clearButton = ClearTrackButtonBounds();
    const Neon::ButtonState clearButtonState = Neon::GetButtonState(clearButton, canClear);
    Neon::DrawButton(clearButton, Neon::Orange, clearButtonState, canClear);
    DrawText("CLEAR TRACK", static_cast<int>(clearButton.x) + 28,
             static_cast<int>(clearButton.y) + 9, 14,
             canClear ? BLACK : Fade(RAYWHITE, 0.62f));
    DrawText(libraryOpen_ ? "Close library to edit this layout." : "Clear is undoable with Ctrl+Z.",
             320, 397, 12, Fade(RAYWHITE, 0.62f));

    DrawPropertyPanel();
    DrawTrackLibrary();
    piecePalette_.Draw(preview_.type);
}

void TrackEditor::DrawPropertyPanel() const {
    const Rectangle bounds{820.0f, 80.0f, 390.0f, 330.0f};
    Neon::DrawOverlayPanel(bounds);
    DrawText("PIECE PROPERTIES", 840, 100, 21, Neon::Cyan);
    DrawText("Up/Down: select     Left/Right: adjust", 840, 130, 14, Neon::Yellow);
    for (int index = 0; index < PropertyCount(); ++index) {
        const float y = 163.0f + static_cast<float>(index) * 25.0f;
        const bool active = index == selectedPropertyIndex_;
        const Rectangle row{838.0f, y - 3.0f, 354.0f, 22.0f};
        if (active) DrawRectangleRec(row, Fade(Neon::Cyan, 0.20f));
        DrawText(PropertyName(index), 846, static_cast<int>(y), 15,
                 active ? Neon::Cyan : RAYWHITE);
        DrawText(PropertyValue(index).c_str(), 1080, static_cast<int>(y), 15,
                 active ? Neon::Yellow : Fade(RAYWHITE, 0.82f));
        const float fraction = std::max(0.0f, std::min(1.0f, PropertyFraction(index)));
        DrawRectangle(965, static_cast<int>(y) + 16, 95, 3, Fade(RAYWHITE, 0.24f));
        DrawRectangle(965, static_cast<int>(y) + 16, static_cast<int>(95.0f * fraction), 3,
                      active ? Neon::Pink : Neon::Cyan);
    }
}
