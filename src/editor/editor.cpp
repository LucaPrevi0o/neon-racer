#include "editor.hpp"

#include "editor_camera.hpp"
#include "editor_picking.hpp"
#include "piece_catalog.hpp"

#include "../persistence/draft_io.hpp"
#include "../render/track_renderer.hpp"
#include "../ui/neon.hpp"

#include <algorithm>
#include <cmath>

namespace {

const int kPanelX = 28;
const int kPanelY = 80;

Rectangle EditorPanelBounds() {
    return Rectangle{static_cast<float>(kPanelX), static_cast<float>(kPanelY), 470.0f, 350.0f};
}

int ClampGridValue(long long value) {
    const long long maximum = static_cast<long long>(TrackLimits::kMaximumGridCoordinate);
    if (value < -maximum) return -TrackLimits::kMaximumGridCoordinate;
    if (value > maximum) return TrackLimits::kMaximumGridCoordinate;
    return static_cast<int>(value);
}

int OffsetGridValue(int value, int offset) {
    return ClampGridValue(static_cast<long long>(value) + static_cast<long long>(offset));
}

int OffsetElevationDelta(int value, int offset) {
    const long long maximum = static_cast<long long>(TrackLimits::kMaximumElevationDelta);
    const long long shifted = static_cast<long long>(value) + static_cast<long long>(offset);
    if (shifted < -maximum) return -TrackLimits::kMaximumElevationDelta;
    if (shifted > maximum) return TrackLimits::kMaximumElevationDelta;
    return static_cast<int>(shifted);
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

Rectangle TrackLibraryBounds() {
    return Rectangle{820.0f, 80.0f, 420.0f, 490.0f};
}

Rectangle SaveDraftButtonBounds() {
    return Rectangle{840.0f, 132.0f, 180.0f, 34.0f};
}

Rectangle RefreshDraftButtonBounds() {
    return Rectangle{1035.0f, 132.0f, 180.0f, 34.0f};
}

Vector3 HeadingVector(Heading heading) {
    switch (heading) {
    case Heading::North: return Vector3{0.0f, 0.0f, -1.0f};
    case Heading::East: return Vector3{1.0f, 0.0f, 0.0f};
    case Heading::South: return Vector3{0.0f, 0.0f, 1.0f};
    case Heading::West: return Vector3{-1.0f, 0.0f, 0.0f};
    }
    return Vector3{0.0f, 0.0f, 0.0f};
}

Color PieceColor(const TrackPiece& piece, std::uint32_t selectedId) {
    if (selectedId != 0 && piece.id == selectedId) return Neon::Yellow;
    return piece.type == TrackPieceType::Straight ? Neon::Pink : Neon::Cyan;
}

void DrawPieceCells(const TrackPiece& piece, std::uint32_t selectedId, float alpha) {
    const Color color = Fade(PieceColor(piece, selectedId), alpha);
    DrawTrackPieceSurface(piece, Fade(Neon::Panel, alpha), color);
}

void DrawConnectorGuide(const TrackConnector& connector, Color color) {
    const Vector3 origin = Vector3{static_cast<float>(connector.position.x), static_cast<float>(connector.position.y) + 0.48f,
                                   static_cast<float>(connector.position.z)};
    const Vector3 direction = HeadingVector(connector.heading);
    const Vector3 tip = Vector3{origin.x + direction.x * 0.70f, origin.y, origin.z + direction.z * 0.70f};
    DrawSphere(origin, 0.16f, color);
    DrawLine3D(origin, tip, color);
    DrawSphere(tip, 0.09f, color);
}

void DrawPieceConnectorGuides(const TrackPiece& piece) {
    // The arrows express travel direction: cyan enters the component and pink
    // leaves it. Compatible faces meet on the same cell and point the same way.
    DrawConnectorGuide(piece.EntryConnector(), Neon::Cyan);
    DrawConnectorGuide(piece.ExitConnector(), Neon::Pink);
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

SurfaceMaterial NextMaterial(SurfaceMaterial material) {
    return static_cast<SurfaceMaterial>((static_cast<int>(material) + 1) % 3);
}

int ClampTwistLength(int length, int width, int exitWidth) {
    const int minimum = TrackLimits::MinimumTwistLengthForRoadWidth(width, exitWidth);
    return std::max(minimum, std::min(TrackLimits::kMaximumTwistLength, length));
}

int ClampTwistRadius(int radius, int width, int exitWidth) {
    const int minimum = TrackLimits::MinimumTwistRadiusForRoadWidth(width, exitWidth);
    const int maximum = static_cast<int>(TrackLimits::kMaximumTwistRadius);
    return std::max(minimum, std::min(maximum, radius));
}

int EffectiveTwistRadiusForEditor(const TrackPiece& piece) {
    const int resolved = static_cast<int>(std::round(TrackLimits::ResolveTwistRadius(piece.length, piece.curveRadius)));
    return ClampTwistRadius(resolved, piece.width, piece.exitWidth);
}

const char* ValidationHint(const TrackValidation& validation) {
    if (validation.raceReady) return "Closed loop and start/finish are ready.";
    if (validation.issues.empty()) return "Track needs one more step.";
    switch (validation.issues.front().kind) {
    case TrackIssueKind::NotOneClosedLoop:
        return "Join every open end into one closed loop.";
    case TrackIssueKind::DisconnectedEntry:
    case TrackIssueKind::DisconnectedExit:
        return "Connect the open cyan and pink ends.";
    case TrackIssueKind::OverlappingGeometry:
        return "Move pieces apart; roads cannot overlap.";
    case TrackIssueKind::MissingStartFinish:
        return "Choose a straight for start/finish.";
    case TrackIssueKind::InvalidStartFinish:
        return "Start/finish must be on a straight.";
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
      piecePalette_(),
      namingDraft_(false),
      draftName_("untitled"),
      previewOverlapCacheValid_(false),
      cachedPreview_(preview_),
      cachedPreviewRevision_(0),
      cachedPreviewOverlaps_(false) {
}

void TrackEditor::BeginNewTrack() {
    // Keep the startup defaults in one place: a fresh editor must reset its
    // placement preview, selection, undo/redo history, and transient draft UI
    // just as if the application had been launched again. This affects only
    // the in-memory session; DraftIO-owned files remain untouched.
    *this = TrackEditor();
    SetMessage("New empty track ready. Saved drafts are unchanged.");
}

void TrackEditor::Update(Camera3D& camera) {
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    // While the draft library is open, it owns the frame's input. F5 and
    // Ctrl+O are the explicit close shortcuts; every other key stays inside
    // the library instead of changing the layout behind it.
    if (libraryOpen_) {
        const Vector2 libraryMouse = GetMousePosition();
        if ((control && IsKeyPressed(KEY_O)) || IsKeyPressed(KEY_F5)) {
            libraryOpen_ = false;
            namingDraft_ = false;
            piecePalette_.Update(libraryMouse, GetScreenWidth(), GetScreenHeight(), GetFrameTime(), false);
            return;
        }
        UpdateTrackLibraryInput();
        piecePalette_.Update(libraryMouse, GetScreenWidth(), GetScreenHeight(), GetFrameTime(), false);
        return;
    }

    if (control && IsKeyPressed(KEY_Z)) Undo();
    if (control && IsKeyPressed(KEY_Y)) Redo();
    if (control && IsKeyPressed(KEY_S)) BeginSaveDraft();
    if (control && IsKeyPressed(KEY_O)) {
        libraryOpen_ = true;
        namingDraft_ = false;
        RefreshDraftList();
    }
    if (IsKeyPressed(KEY_F5)) {
        libraryOpen_ = !libraryOpen_;
        namingDraft_ = false;
        if (libraryOpen_) RefreshDraftList();
    }
    const bool libraryConsumed = UpdateTrackLibraryInput();
    const Vector2 mouse = GetMousePosition();
    piecePalette_.Update(mouse, GetScreenWidth(), GetScreenHeight(), GetFrameTime(), !libraryConsumed);
    // The draft library is a modal surface. Do not let its text entry, rows,
    // or empty areas change the placement preview behind it in the same frame.
    if (libraryConsumed) return;

    const bool paletteConsumesPointer = !libraryConsumed && piecePalette_.ConsumesPointer(mouse);
    const bool editorPanelConsumesPointer = helpPanelExpanded_ && CheckCollisionPointRec(mouse, EditorPanelBounds());
    const bool leftClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    TrackPieceType paletteSelection = TrackPieceType::Straight;
    const bool piecePaletteClicked = !libraryConsumed && leftClick &&
        piecePalette_.ConsumeSelection(mouse, paletteSelection);
    const bool helpToggled = !libraryConsumed && !paletteConsumesPointer && leftClick &&
        CheckCollisionPointRec(mouse, HelpToggleBounds(helpPanelExpanded_));
    if (helpToggled) {
        helpPanelExpanded_ = !helpPanelExpanded_;
        return;
    }

    const int pieceShortcutKeys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX};
    for (std::size_t index = 0; index < sizeof(pieceShortcutKeys) / sizeof(pieceShortcutKeys[0]); ++index) {
        if (libraryConsumed || !IsKeyPressed(pieceShortcutKeys[index])) continue;
        const EditorPieceCatalog::Item* item = EditorPieceCatalog::FindShortcut(static_cast<int>(index) + 1);
        if (item != 0) SelectPreviewType(item->type);
    }
    const float wheel = GetMouseWheelMove();
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool zooming = shift;
    if (!paletteConsumesPointer && !editorPanelConsumesPointer) {
        if (control && wheel > 0.0f) ChangeDimension(1);
        if (control && wheel < 0.0f) ChangeDimension(-1);
        if (!control && zooming && wheel != 0.0f) EditorCamera::Zoom(camera, wheel);
        if (!control && !zooming && wheel > 0.0f) RotatePreview();
        if (!control && !zooming && wheel < 0.0f) {
            preview_.entryHeading = static_cast<Heading>((static_cast<int>(preview_.entryHeading) + 3) % 4);
            SetMessage("Preview rotated 90 degrees.");
        }
    }
    if (!paletteConsumesPointer && !editorPanelConsumesPointer && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        EditorCamera::Orbit(camera, GetMouseDelta());
    }
    if (IsKeyPressed(KEY_HOME)) EditorCamera::Reset(camera);
    if (IsKeyPressed(KEY_R)) RotatePreview();
    if (!libraryConsumed && IsKeyPressed(KEY_UP)) CycleProperty(-1);
    if (!libraryConsumed && IsKeyPressed(KEY_DOWN)) CycleProperty(1);
    if (!libraryConsumed && IsKeyPressed(KEY_LEFT)) AdjustSelectedProperty(-1);
    if (!libraryConsumed && IsKeyPressed(KEY_RIGHT)) AdjustSelectedProperty(1);

    if (IsKeyPressed(KEY_F) && selectedPieceId_ != 0) {
        const TrackPiece* selected = track_.GetPiece(selectedPieceId_);
        if (selected != 0) {
            preview_ = *selected;
            preview_.id = 0;
            SetMessage("Selected component copied into the placement preview.");
        }
    }
    GridPosition mousePosition;
    // A selected component stays anchored while its properties are edited.
    // Without this guard, merely moving the mouse before applying a turn flip
    // silently changed its entry position and made its connectors appear wrong.
    if (!paletteConsumesPointer && !editorPanelConsumesPointer && selectedPieceId_ == 0) {
        const Ray mouseRay = GetMouseRay(mouse, camera);
        if (EditorPicking::GridPositionFromRay(mouseRay, mousePosition)) {
            preview_.entryPosition.x = mousePosition.x;
            preview_.entryPosition.z = mousePosition.z;
        }
    }

    const bool selectionClick = leftClick && shift;
    const bool clearTrackClicked = !libraryConsumed && !paletteConsumesPointer && helpPanelExpanded_ && leftClick &&
        CheckCollisionPointRec(mouse, ClearTrackButtonBounds());
    const bool startFinishClicked = !libraryConsumed && !paletteConsumesPointer && helpPanelExpanded_ && leftClick &&
        CheckCollisionPointRec(mouse, StartFinishButtonBounds());
    if (piecePaletteClicked) {
        SelectPreviewType(paletteSelection);
    } else if (clearTrackClicked) {
        ClearTrack();
    } else if (startFinishClicked) {
        SetStartFinish();
    } else if (!libraryConsumed && !paletteConsumesPointer && !editorPanelConsumesPointer && selectionClick) {
        const std::uint32_t pickedPieceId =
            EditorPicking::PickPieceFromRay(GetMouseRay(mouse, camera), track_.Pieces());
        if (pickedPieceId != 0 && pickedPieceId != selectedPieceId_) {
            const TrackPiece* selected = track_.GetPiece(pickedPieceId);
            if (selected != 0) {
                selectedPieceId_ = pickedPieceId;
                preview_ = *selected;
                preview_.id = selectedPieceId_;
                SetMessage("Component selected. Adjust its panel properties, then click or press M to apply.");
            }
        } else if (pickedPieceId == 0) {
            SetMessage("No component under the pointer to select.");
        }
    } else if (!libraryConsumed && (IsKeyPressed(KEY_ENTER) ||
               (!paletteConsumesPointer && !editorPanelConsumesPointer && leftClick))) {
        // Plain clicks never ray-pick. They place a free preview or apply an
        // already selected edit, so nearby road surfaces cannot steal a click.
        if (selectedPieceId_ == 0) PlacePreview();
        else TransformSelected();
    }
    if (IsKeyPressed(KEY_M)) TransformSelected();
    if (IsKeyPressed(KEY_C)) DuplicateSelected();
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_X)) DeleteSelected();
    const float pan = 0.35f;
    if (IsKeyDown(KEY_A)) EditorCamera::Pan(camera, Vector2{pan / 0.035f, 0.0f});
    if (IsKeyDown(KEY_D)) EditorCamera::Pan(camera, Vector2{-pan / 0.035f, 0.0f});
    if (IsKeyDown(KEY_W)) EditorCamera::Pan(camera, Vector2{0.0f, pan / 0.035f});
    if (IsKeyDown(KEY_S)) EditorCamera::Pan(camera, Vector2{0.0f, -pan / 0.035f});
    if (IsKeyDown(KEY_Q)) EditorCamera::Raise(camera, pan);
    if (IsKeyDown(KEY_E)) EditorCamera::Raise(camera, -pan);
}

void TrackEditor::DrawTrack3D() const {
    for (std::vector<TrackPiece>::const_iterator piece = track_.Pieces().begin();
         piece != track_.Pieces().end(); ++piece) {
        if (piece->id == selectedPieceId_) continue;
        DrawPieceCells(*piece, selectedPieceId_, 0.96f);
        DrawPieceConnectorGuides(*piece);
    }

    if (selectedPieceId_ != 0) {
        const TrackPiece* original = track_.GetPiece(selectedPieceId_);
        if (original != 0) {
            const float pulse = 0.16f + 0.14f * (0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 5.0f));
            DrawPieceCells(*original, 0, pulse);
        }
    }

    if (track_.HasStartFinish()) {
        const TrackPiece* start = track_.GetPiece(track_.StartFinishPieceId());
        if (start != 0) {
            const GridPosition position = start->EntryConnector().position;
            DrawCube(Vector3{static_cast<float>(position.x), static_cast<float>(position.y) + 0.34f,
                             static_cast<float>(position.z)},
                     0.92f, 0.08f, 0.92f, Neon::Green);
        }
    }

    const TrackValidation validation = track_.Validate();
    for (std::vector<TrackIssue>::const_iterator issue = validation.issues.begin();
         issue != validation.issues.end(); ++issue) {
        for (std::vector<std::uint32_t>::const_iterator id = issue->affectedPieceIds.begin();
             id != issue->affectedPieceIds.end(); ++id) {
            const TrackPiece* affected = track_.GetPiece(*id);
            if (affected == 0) continue;
            const GridPosition position = affected->EntryConnector().position;
            DrawSphere(Vector3{static_cast<float>(position.x), static_cast<float>(position.y) + 0.72f,
                               static_cast<float>(position.z)},
                       0.25f, Neon::Orange);
        }
    }

    const TrackPiece candidate = BuildPreview();
    const bool overlaps = PreviewOverlaps(candidate);
    DrawPieceCells(candidate, selectedPieceId_, overlaps ? 0.28f : 0.52f);
    DrawPieceConnectorGuides(candidate);

    const TrackConnector entry = candidate.EntryConnector();
    const TrackConnector exit = candidate.ExitConnector();
    const Color connectorColor = overlaps ? Neon::Orange : Neon::Green;
    DrawSphere(Vector3{static_cast<float>(entry.position.x), static_cast<float>(entry.position.y) + 0.42f,
                       static_cast<float>(entry.position.z)}, 0.22f, connectorColor);
    DrawSphere(Vector3{static_cast<float>(exit.position.x), static_cast<float>(exit.position.y) + 0.42f,
                       static_cast<float>(exit.position.z)}, 0.22f, connectorColor);
}

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
    const bool lengthBasedPiece = preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist ||
        preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge;
    if (preview_.type == TrackPieceType::Twist) {
        const bool legacyFlatTwist = TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius);
        DrawText(legacyFlatTwist ?
                     TextFormat("GRID (%i, %i, %i)  |  LEGACY FLAT ROLL", preview_.entryPosition.x,
                                preview_.entryPosition.y, preview_.entryPosition.z) :
                     TextFormat("GRID (%i, %i, %i)  |  RUN %i  |  R %.1f", preview_.entryPosition.x,
                                preview_.entryPosition.y, preview_.entryPosition.z, preview_.length,
                                TrackLimits::ResolveTwistRadius(preview_.length, preview_.curveRadius)),
                 kPanelX + 18, kPanelY + 89, 14, Fade(RAYWHITE, 0.86f));
    } else {
        DrawText(TextFormat("GRID (%i, %i, %i)  |  %s %i", preview_.entryPosition.x, preview_.entryPosition.y,
                            preview_.entryPosition.z, lengthBasedPiece ? "LENGTH" : "RADIUS",
                            lengthBasedPiece ? preview_.length : preview_.curveRadius),
                 kPanelX + 18, kPanelY + 89, 14, Fade(RAYWHITE, 0.86f));
    }
    DrawText(TextFormat("ROAD %i > %i  |  RAMP %+i  |  %s", preview_.width, preview_.exitWidth,
                        preview_.elevationDelta, MaterialName(preview_.material)),
             kPanelX + 18, kPanelY + 111, 14, Neon::Pink);
    if (isCurve) {
        DrawText(TextFormat("CURVE %s  |  %i DEG  |  BANK %+i", preview_.curveTurn == CurveTurn::Right ? "RIGHT" : "LEFT",
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
    DrawText(validation.raceReady ? "RACE READY" : "DRAFT", kPanelX + 18, statusY + 22, 16, validationColor);
    DrawText(ValidationHint(validation), kPanelX + 130, statusY + 24, 13, Fade(validationColor, 0.86f));
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
    const char* startFinishLabel = libraryOpen_ ? "CLOSE LIBRARY TO EDIT" : !layoutComplete ? "CLOSE LOOP FIRST" :
        canSetStartFinish ? "SET START / FINISH" : "SELECT A STRAIGHT";
    const int startFinishSize = canSetStartFinish ? 14 : 13;
    DrawText(startFinishLabel,
             static_cast<int>(startFinishButton.x + (startFinishButton.width - MeasureText(startFinishLabel, startFinishSize)) * 0.5f),
             static_cast<int>(startFinishButton.y) + 9, startFinishSize,
             canSetStartFinish ? BLACK : Fade(RAYWHITE, 0.60f));

    const bool canClear = !libraryOpen_ && !track_.Pieces().empty();
    const Rectangle clearButton = ClearTrackButtonBounds();
    const Neon::ButtonState clearButtonState = Neon::GetButtonState(clearButton, canClear);
    Neon::DrawButton(clearButton, Neon::Orange, clearButtonState, canClear);
    DrawText("CLEAR TRACK", static_cast<int>(clearButton.x) + 28, static_cast<int>(clearButton.y) + 9, 14,
             canClear ? BLACK : Fade(RAYWHITE, 0.62f));
    DrawText(libraryOpen_ ? "Close library to edit this layout." : "Clear is undoable with Ctrl+Z.",
             320, 397, 12, Fade(RAYWHITE, 0.62f));
    DrawPropertyPanel();
    DrawTrackLibrary();
    piecePalette_.Draw(preview_.type);
}

const Track& TrackEditor::GetTrack() const { return track_; }

TrackPiece TrackEditor::BuildPreview() const {
    TrackPiece candidate = preview_;
    candidate.id = selectedPieceId_;
    return candidate;
}

void TrackEditor::SelectPreviewType(TrackPieceType type) {
    const EditorPieceCatalog::Item* item = EditorPieceCatalog::Find(type);
    if (item == 0) return;
    const bool editingSelectedPiece = selectedPieceId_ != 0;
    const TrackPieceType previousType = preview_.type;
    preview_.type = type;
    if (type == TrackPieceType::Twist) {
        const bool convertingLegacyFlatTwist = previousType == TrackPieceType::Twist &&
            TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius);
        const int preferredLength = previousType == TrackPieceType::Twist && !convertingLegacyFlatTwist
            ? preview_.length : TrackLimits::kDefaultTwistLength;
        preview_.length = ClampTwistLength(preferredLength, preview_.width, preview_.exitWidth);
        if (previousType != TrackPieceType::Twist || convertingLegacyFlatTwist) {
            preview_.curveRadius = TrackLimits::DefaultTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
        } else if (preview_.curveRadius != 0) {
            preview_.curveRadius = ClampTwistRadius(preview_.curveRadius, preview_.width, preview_.exitWidth);
        }
    } else if (previousType == TrackPieceType::Twist) {
        // Returning from a long twist to a regular length-based component
        // should leave a placeable preview rather than an out-of-range long
        // straight or branch.
        preview_.length = std::max(3, std::min(20, preview_.length));
    }
    if (type == TrackPieceType::Branch || type == TrackPieceType::Merge) {
        preview_.lateralOffset = std::max(1, std::abs(preview_.lateralOffset));
    }
    selectedPropertyIndex_ = 0;

    // Type selection has always been usable while a component is selected.
    // Preserve that explicit transform workflow for both the 1–6 shortcuts
    // and the visual palette instead of silently discarding the edit target.
    if (editingSelectedPiece) {
        SetMessage(std::string(item->name) + " selected for the active edit. Click or press M to apply.");
        return;
    }

    if (type == TrackPieceType::Straight) SetMessage("Straight selected for placement.");
    else if (type == TrackPieceType::Curve) SetMessage("Curve selected for placement. Edit its properties in the panel.");
    else if (type == TrackPieceType::Loop) SetMessage("Vertical loop selected for placement.");
    else if (type == TrackPieceType::Twist) {
        SetMessage(TextFormat("Twist selected: %i-cell run / %.1f-cell radius. Tune both in the panel.", preview_.length,
                               TrackLimits::ResolveTwistRadius(preview_.length, preview_.curveRadius)));
    }
    else if (type == TrackPieceType::Branch) SetMessage("Two-arm branch selected for placement.");
    else SetMessage("Two-arm merge selected for placement.");
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

void TrackEditor::MovePreview(int x, int z) {
    preview_.entryPosition.x = OffsetGridValue(preview_.entryPosition.x, x);
    preview_.entryPosition.z = OffsetGridValue(preview_.entryPosition.z, z);
}

void TrackEditor::RotatePreview() {
    preview_.entryHeading = static_cast<Heading>((static_cast<int>(preview_.entryHeading) + 1) % 4);
    SetMessage("Preview rotated 90 degrees.");
}

void TrackEditor::ChangeDimension(int amount) {
    if (preview_.type == TrackPieceType::Twist) {
        if (TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius)) {
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = TrackLimits::DefaultTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
            SetMessage("Legacy flat Twist converted to an adjustable corkscrew.");
        }
        preview_.length = ClampTwistLength(preview_.length + amount, preview_.width, preview_.exitWidth);
        SetMessage(TextFormat("Twist length set to %i.", preview_.length));
        return;
    }
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        preview_.length = std::max(3, std::min(20, preview_.length + amount));
        SetMessage(TextFormat("%s length set to %i.", PieceName(preview_.type), preview_.length));
        return;
    }
    int& dimension = (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist || preview_.type == TrackPieceType::Branch) ? preview_.length : preview_.curveRadius;
    const int maximum = preview_.type == TrackPieceType::Loop ? 10 : 20;
    dimension = std::max(3, std::min(maximum, dimension + amount));
}

int TrackEditor::PropertyCount() const {
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) return 4;
    if (preview_.type == TrackPieceType::Straight) return 7;
    if (preview_.type == TrackPieceType::Twist) return 8;
    return preview_.type == TrackPieceType::Curve ? 9 : 7;
}

const char* TrackEditor::PropertyName(int index) const {
    static const char* straight[] = {"Length", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* twist[] = {"Run length", "Loop radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* curve[] = {"Radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Surface", "Turn", "Extent", "Bank"};
    static const char* loop[] = {"Radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* branch[] = {"Length", "Width", "Arm spread", "Surface"};
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) return branch[index];
    if (preview_.type == TrackPieceType::Twist) return twist[index];
    if (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Branch) return straight[index];
    return preview_.type == TrackPieceType::Curve ? curve[index] : loop[index];
}

std::string TrackEditor::PropertyValue(int index) const {
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        if (index == 0) return std::to_string(preview_.length);
        if (index == 1) return std::to_string(preview_.width);
        return index == 2 ? std::to_string(std::abs(preview_.lateralOffset)) : MaterialName(preview_.material);
    }
    if (preview_.type == TrackPieceType::Twist) {
        if (index == 0) return std::to_string(preview_.length);
        if (index == 1) {
            if (TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius)) return "legacy flat";
            return preview_.curveRadius == 0 ? "auto" : std::to_string(preview_.curveRadius);
        }
        if (index == 2) return std::to_string(preview_.width);
        if (index == 3) return std::to_string(preview_.exitWidth);
        if (index == 4) return std::to_string(preview_.entryPosition.y);
        if (index == 5) return std::to_string(preview_.elevationDelta);
        return index == 6 ? std::to_string(preview_.lateralOffset) : MaterialName(preview_.material);
    }
    if (index == 0) return std::to_string(preview_.type == TrackPieceType::Straight ? preview_.length : preview_.curveRadius);
    if (index == 1) return std::to_string(preview_.width);
    if (index == 2) return std::to_string(preview_.exitWidth);
    if (index == 3) return std::to_string(preview_.entryPosition.y);
    if (index == 4) return std::to_string(preview_.elevationDelta);
    if (preview_.type == TrackPieceType::Straight) {
        return index == 5 ? std::to_string(preview_.lateralOffset) : MaterialName(preview_.material);
    }
    if (preview_.type == TrackPieceType::Loop) return index == 5 ? std::to_string(preview_.lateralOffset) : MaterialName(preview_.material);
    if (index == 5) return MaterialName(preview_.material);
    if (index == 6) return preview_.curveTurn == CurveTurn::Right ? "right" : "left";
    if (index == 7) return std::to_string(preview_.curveDegrees) + " deg";
    return std::to_string(preview_.bankAngleDegrees) + " deg";
}

float TrackEditor::PropertyFraction(int index) const {
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        if (index == 0) return static_cast<float>(preview_.length - 3) / 17.0f;
        if (index == 1) return static_cast<float>(preview_.width - 5) / 6.0f;
        return index == 2 ? static_cast<float>(std::abs(preview_.lateralOffset) - 1) / 9.0f :
            (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    }
    if (index == 0 && preview_.type == TrackPieceType::Twist) {
        const int minimum = TrackLimits::MinimumTwistLengthForRoadWidth(preview_.width, preview_.exitWidth);
        return static_cast<float>(preview_.length - minimum) /
            static_cast<float>(TrackLimits::kMaximumTwistLength - minimum);
    }
    if (preview_.type == TrackPieceType::Twist) {
        const int minimum = TrackLimits::MinimumTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
        const int effectiveRadius = EffectiveTwistRadiusForEditor(preview_);
        if (index == 1) return static_cast<float>(effectiveRadius - minimum) /
            static_cast<float>(static_cast<int>(TrackLimits::kMaximumTwistRadius) - minimum);
        if (index == 2) return static_cast<float>(preview_.width - 5) / 6.0f;
        if (index == 3) return static_cast<float>(preview_.exitWidth - 5) / 6.0f;
        if (index == 4 || index == 5) return 0.5f;
        return index == 6 ? (preview_.lateralOffset + 3.0f) / 6.0f :
            (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    }
    if (index == 0) return static_cast<float>((preview_.type == TrackPieceType::Straight ? preview_.length : preview_.curveRadius) - 3) / 17.0f;
    if (index == 1) return static_cast<float>(preview_.width - 5) / 6.0f;
    if (index == 2) return static_cast<float>(preview_.exitWidth - 5) / 6.0f;
    if (index == 3 || index == 4) return 0.5f;
    if (preview_.type == TrackPieceType::Straight) return index == 5 ? (preview_.lateralOffset + 3.0f) / 6.0f : (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    if (preview_.type == TrackPieceType::Loop) return index == 5 ? (preview_.lateralOffset + 10.0f) / 20.0f :
        (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    if (index == 5) return (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
    if (index == 6) return preview_.curveTurn == CurveTurn::Right ? 1.0f : 0.0f;
    if (index == 7) return static_cast<float>(preview_.curveDegrees - 90) / 180.0f;
    return static_cast<float>(preview_.bankAngleDegrees + 45) / 90.0f;
}

void TrackEditor::CycleProperty(int direction) {
    const int count = PropertyCount();
    selectedPropertyIndex_ = (selectedPropertyIndex_ + direction + count) % count;
}

void TrackEditor::AdjustSelectedProperty(int direction) {
    const int index = selectedPropertyIndex_;
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        if (index == 0) ChangeDimension(direction);
        if (index == 1) preview_.width = preview_.exitWidth = std::max(5, std::min(11, preview_.width + direction));
        if (index == 2) preview_.lateralOffset = std::max(1, std::min(10, std::abs(preview_.lateralOffset) + direction));
        if (index == 3) preview_.material = direction > 0 ? NextMaterial(preview_.material) :
            static_cast<SurfaceMaterial>((static_cast<int>(preview_.material) + 2) % 3);
        return;
    }
    if (preview_.type == TrackPieceType::Twist) {
        if (TrackLimits::IsLegacyFlatTwistRadius(preview_.curveRadius)) {
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = TrackLimits::DefaultTwistRadiusForRoadWidth(preview_.width, preview_.exitWidth);
            SetMessage("Legacy flat Twist converted to an adjustable corkscrew.");
        }
        if (index == 0) { ChangeDimension(direction); return; }
        if (index == 1) {
            preview_.curveRadius = ClampTwistRadius(EffectiveTwistRadiusForEditor(preview_) + direction,
                                                     preview_.width, preview_.exitWidth);
            SetMessage(TextFormat("Twist radius set to %i.", preview_.curveRadius));
            return;
        }
        if (index == 2) {
            preview_.width = std::max(5, std::min(11, preview_.width + direction));
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = ClampTwistRadius(EffectiveTwistRadiusForEditor(preview_), preview_.width,
                                                     preview_.exitWidth);
            return;
        }
        if (index == 3) {
            preview_.exitWidth = std::max(5, std::min(11, preview_.exitWidth + direction));
            preview_.length = ClampTwistLength(preview_.length, preview_.width, preview_.exitWidth);
            preview_.curveRadius = ClampTwistRadius(EffectiveTwistRadiusForEditor(preview_), preview_.width,
                                                     preview_.exitWidth);
            return;
        }
        if (index == 4) { preview_.entryPosition.y = OffsetGridValue(preview_.entryPosition.y, direction); return; }
        if (index == 5) { preview_.elevationDelta = OffsetElevationDelta(preview_.elevationDelta, direction); return; }
        if (index == 6) {
            preview_.lateralOffset = std::max(-3, std::min(3, preview_.lateralOffset + direction));
            return;
        }
        preview_.material = direction > 0 ? NextMaterial(preview_.material) :
            static_cast<SurfaceMaterial>((static_cast<int>(preview_.material) + 2) % 3);
        return;
    }
    if (index == 0) { ChangeDimension(direction); return; }
    if (index == 1) {
        preview_.width = std::max(5, std::min(11, preview_.width + direction));
        return;
    }
    if (index == 2) {
        preview_.exitWidth = std::max(5, std::min(11, preview_.exitWidth + direction));
        return;
    }
    if (index == 3) { preview_.entryPosition.y = OffsetGridValue(preview_.entryPosition.y, direction); return; }
    if (index == 4) { preview_.elevationDelta = OffsetElevationDelta(preview_.elevationDelta, direction); return; }
    if (preview_.type == TrackPieceType::Straight) {
        if (index == 5) {
            const int maximum = preview_.length < 4 ? 2 : 3;
            preview_.lateralOffset = std::max(-maximum, std::min(maximum, preview_.lateralOffset + direction));
        } else {
            preview_.material = direction > 0 ? NextMaterial(preview_.material) :
                static_cast<SurfaceMaterial>((static_cast<int>(preview_.material) + 2) % 3);
        }
        return;
    }
    if (preview_.type == TrackPieceType::Loop) {
        if (index == 5) preview_.lateralOffset = std::max(-10, std::min(10, preview_.lateralOffset + direction));
        if (index == 6) preview_.material = direction > 0 ? NextMaterial(preview_.material) :
            static_cast<SurfaceMaterial>((static_cast<int>(preview_.material) + 2) % 3);
        return;
    }
    if (index == 5) preview_.material = direction > 0 ? NextMaterial(preview_.material) : static_cast<SurfaceMaterial>((static_cast<int>(preview_.material) + 2) % 3);
    if (index == 6) preview_.curveTurn = preview_.curveTurn == CurveTurn::Right ? CurveTurn::Left : CurveTurn::Right;
    if (index == 7) preview_.curveDegrees = direction > 0 ? (preview_.curveDegrees == 270 ? 90 : preview_.curveDegrees + 90) : (preview_.curveDegrees == 90 ? 270 : preview_.curveDegrees - 90);
    if (index == 8) preview_.bankAngleDegrees = std::max(-45, std::min(45, preview_.bankAngleDegrees + direction * 5));
}

void TrackEditor::DrawPropertyPanel() const {
    const Rectangle bounds = Rectangle{820.0f, 80.0f, 390.0f, 330.0f};
    Neon::DrawOverlayPanel(bounds);
    DrawText("PIECE PROPERTIES", 840, 100, 21, Neon::Cyan);
    DrawText("Up/Down: select     Left/Right: adjust", 840, 130, 14, Neon::Yellow);
    for (int index = 0; index < PropertyCount(); ++index) {
        const float y = 163.0f + static_cast<float>(index) * 25.0f;
        const bool active = index == selectedPropertyIndex_;
        const Rectangle row = Rectangle{838.0f, y - 3.0f, 354.0f, 22.0f};
        if (active) DrawRectangleRec(row, Fade(Neon::Cyan, 0.20f));
        DrawText(PropertyName(index), 846, static_cast<int>(y), 15, active ? Neon::Cyan : RAYWHITE);
        DrawText(PropertyValue(index).c_str(), 1080, static_cast<int>(y), 15, active ? Neon::Yellow : Fade(RAYWHITE, 0.82f));
        const float fraction = std::max(0.0f, std::min(1.0f, PropertyFraction(index)));
        DrawRectangle(965, static_cast<int>(y) + 16, 95, 3, Fade(RAYWHITE, 0.24f));
        DrawRectangle(965, static_cast<int>(y) + 16, static_cast<int>(95.0f * fraction), 3, active ? Neon::Pink : Neon::Cyan);
    }
}

bool TrackEditor::UpdateTrackLibraryInput() {
    if (!libraryOpen_) return false;

    if (namingDraft_) {
        int character = GetCharPressed();
        while (character > 0) {
            if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') || character == ' ' || character == '_' || character == '-') {
                if (draftName_.size() < 32) draftName_ += static_cast<char>(character);
            }
            character = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !draftName_.empty()) draftName_.erase(draftName_.size() - 1);
        if (IsKeyPressed(KEY_ENTER)) SaveNamedDraft();
        return true;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return true;
    const Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, SaveDraftButtonBounds())) {
        BeginSaveDraft();
        return true;
    }
    if (CheckCollisionPointRec(mouse, RefreshDraftButtonBounds())) {
        RefreshDraftList();
        return true;
    }
    for (std::size_t index = 0; index < savedDrafts_.size() && index < 12; ++index) {
        const Rectangle row = Rectangle{840.0f, 192.0f + static_cast<float>(index) * 27.0f, 375.0f, 23.0f};
        if (CheckCollisionPointRec(mouse, row)) {
            LoadDraft(savedDrafts_[index]);
            return true;
        }
    }
    return true;
}

void TrackEditor::DrawTrackLibrary() const {
    if (!libraryOpen_) return;
    const Rectangle bounds = TrackLibraryBounds();
    Neon::DrawOverlayPanel(bounds, 0.92f);
    DrawText("CUSTOM TRACK LIBRARY", static_cast<int>(bounds.x) + 20, static_cast<int>(bounds.y) + 18, 22, Neon::Cyan);

    const Rectangle save = SaveDraftButtonBounds();
    const Rectangle refresh = RefreshDraftButtonBounds();
    const bool actionsEnabled = !namingDraft_;
    const Neon::ButtonState saveState = Neon::GetButtonState(save, actionsEnabled);
    const Neon::ButtonState refreshState = Neon::GetButtonState(refresh, actionsEnabled);
    Neon::DrawButton(save, Neon::Pink, saveState, actionsEnabled);
    DrawText("SAVE AS", static_cast<int>(save.x) + 54, static_cast<int>(save.y) + 9, 15,
             actionsEnabled ? BLACK : Fade(RAYWHITE, 0.36f));
    Neon::DrawButton(refresh, Neon::Cyan, refreshState);
    DrawText("REFRESH", static_cast<int>(refresh.x) + 54, static_cast<int>(refresh.y) + 9, 15,
             actionsEnabled ? (Neon::IsButtonHighlighted(refreshState) ? RAYWHITE : Neon::Cyan) : Fade(RAYWHITE, 0.36f));

    if (namingDraft_) {
        DrawText("Name your draft, then press Enter:", 840, 188, 16, RAYWHITE);
        DrawRectangleRec(Rectangle{840.0f, 214.0f, 375.0f, 34.0f}, Fade(BLACK, 0.65f));
        DrawRectangleLinesEx(Rectangle{840.0f, 214.0f, 375.0f, 34.0f}, 2.0f, Neon::Yellow);
        DrawText(draftName_.c_str(), 850, 223, 18, Neon::Yellow);
        DrawText("Saved in your Neon Racer data folder.", 840, 265, 14, Fade(RAYWHITE, 0.68f));
        return;
    }

    DrawText("CUSTOM DRAFTS  |  click one to load", 840, 175, 16, RAYWHITE);
    if (savedDrafts_.empty()) {
        DrawText("No saved custom tracks yet.", 840, 205, 16, Fade(RAYWHITE, 0.65f));
    }
    for (std::size_t index = 0; index < savedDrafts_.size() && index < 12; ++index) {
        const Rectangle row = Rectangle{840.0f, 192.0f + static_cast<float>(index) * 27.0f, 375.0f, 23.0f};
        const Neon::ButtonState rowState = Neon::GetButtonState(row);
        Neon::DrawButton(row, Neon::Cyan, rowState);
        DrawText(savedDrafts_[index].c_str(), static_cast<int>(row.x) + 10, static_cast<int>(row.y) + 4, 15,
                 Neon::IsButtonHighlighted(rowState) ? RAYWHITE : Neon::Cyan);
    }
    DrawText("Ctrl+S save  |  Ctrl+O/F5 library", 840, 540, 14, Neon::Yellow);
}

void TrackEditor::SetMessage(const std::string& message) { message_ = message; }
