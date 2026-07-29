#include "editor.hpp"

#include "editor_camera.hpp"
#include "editor_picking.hpp"

#include "../persistence/draft_io.hpp"
#include "../render/track_renderer.hpp"
#include "../ui/neon.hpp"

#include <algorithm>
#include <cmath>

namespace {

const int kPanelX = 28;
const int kPanelY = 80;

Rectangle StartFinishButtonBounds() {
    return Rectangle{static_cast<float>(kPanelX + 18), 600.0f, 260.0f, 34.0f};
}

Rectangle ClearTrackButtonBounds() {
    return Rectangle{320.0f, 600.0f, 160.0f, 34.0f};
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
    if (type == TrackPieceType::Straight) return "Straight";
    if (type == TrackPieceType::Curve) return "Curve";
    if (type == TrackPieceType::Loop) return "Loop";
    if (type == TrackPieceType::Twist) return "Twist";
    return type == TrackPieceType::Branch ? "Branch" : "Merge";
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
      namingDraft_(false),
      draftName_("untitled"),
      previewOverlapCacheValid_(false),
      cachedPreview_(preview_),
      cachedPreviewRevision_(0),
      cachedPreviewOverlaps_(false) {
}

void TrackEditor::Update(Camera3D& camera) {
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
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
    const bool helpToggled = !libraryConsumed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(GetMousePosition(), HelpToggleBounds(helpPanelExpanded_));
    if (helpToggled) {
        helpPanelExpanded_ = !helpPanelExpanded_;
        return;
    }

    if (IsKeyPressed(KEY_ONE)) {
        preview_.type = TrackPieceType::Straight;
        selectedPropertyIndex_ = 0;
        SetMessage("Straight selected for placement.");
    }
    if (IsKeyPressed(KEY_TWO)) {
        preview_.type = TrackPieceType::Curve;
        selectedPropertyIndex_ = 0;
        SetMessage("Curve selected for placement. Edit its properties in the panel.");
    }
    if (IsKeyPressed(KEY_THREE)) {
        preview_.type = TrackPieceType::Loop;
        selectedPropertyIndex_ = 0;
        SetMessage("Vertical loop selected for placement.");
    }
    if (IsKeyPressed(KEY_FOUR)) {
        preview_.type = TrackPieceType::Twist;
        selectedPropertyIndex_ = 0;
        SetMessage("Twist selected for placement.");
    }
    if (IsKeyPressed(KEY_FIVE)) {
        preview_.type = TrackPieceType::Branch;
        preview_.lateralOffset = std::max(1, std::abs(preview_.lateralOffset));
        selectedPropertyIndex_ = 0;
        SetMessage("Two-arm branch selected for placement.");
    }
    if (IsKeyPressed(KEY_SIX)) {
        preview_.type = TrackPieceType::Merge;
        preview_.lateralOffset = std::max(1, std::abs(preview_.lateralOffset));
        selectedPropertyIndex_ = 0;
        SetMessage("Two-arm merge selected for placement.");
    }
    const float wheel = GetMouseWheelMove();
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool zooming = shift;
    if (control && wheel > 0.0f) ChangeDimension(1);
    if (control && wheel < 0.0f) ChangeDimension(-1);
    if (!control && zooming && wheel != 0.0f) EditorCamera::Zoom(camera, wheel);
    if (!control && !zooming && wheel > 0.0f) RotatePreview();
    if (!control && !zooming && wheel < 0.0f) {
        preview_.entryHeading = static_cast<Heading>((static_cast<int>(preview_.entryHeading) + 3) % 4);
        SetMessage("Preview rotated 90 degrees.");
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) EditorCamera::Orbit(camera, GetMouseDelta());
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
    if (selectedPieceId_ == 0) {
        const Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
        if (EditorPicking::GridPositionFromRay(mouseRay, mousePosition)) {
            preview_.entryPosition.x = mousePosition.x;
            preview_.entryPosition.z = mousePosition.z;
        }
    }

    const bool leftClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool selectionClick = leftClick && shift;
    const bool clearTrackClicked = !libraryConsumed && helpPanelExpanded_ && leftClick &&
        CheckCollisionPointRec(GetMousePosition(), ClearTrackButtonBounds());
    const bool startFinishClicked = !libraryConsumed && helpPanelExpanded_ && leftClick &&
        CheckCollisionPointRec(GetMousePosition(), StartFinishButtonBounds());
    if (clearTrackClicked) {
        ClearTrack();
    } else if (startFinishClicked) {
        SetStartFinish();
    } else if (!libraryConsumed && selectionClick) {
        const std::uint32_t pickedPieceId =
            EditorPicking::PickPieceFromRay(GetMouseRay(GetMousePosition(), camera), track_.Pieces());
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
    } else if (!libraryConsumed && (leftClick || IsKeyPressed(KEY_ENTER))) {
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
        Neon::DrawOverlayPanel(button, 0.82f);
        DrawText("SHOW EDITOR HELP", 42, 92, 15, Neon::Cyan);
        DrawPropertyPanel();
        DrawTrackLibrary();
        return;
    }
    const TrackValidation validation = track_.Validate();
    const int detailOffset = 48 + (preview_.type == TrackPieceType::Curve ? 24 : 0);
    Neon::DrawOverlayPanel(Rectangle{static_cast<float>(kPanelX), static_cast<float>(kPanelY), 470.0f,
                                     static_cast<float>(550 + detailOffset)});
    DrawText("TRACK EDITOR", kPanelX + 18, kPanelY + 18, 22, Neon::Cyan);
    const Rectangle helpButton = HelpToggleBounds(true);
    DrawRectangleRec(helpButton, Fade(Neon::Panel, 0.85f));
    DrawRectangleLinesEx(helpButton, 1.0f, Fade(Neon::Cyan, 0.72f));
    DrawText("HIDE", 425, 94, 13, Neon::Cyan);
    DrawText(TextFormat("Preview: %s  |  %s", PieceName(preview_.type), HeadingName(preview_.entryHeading)),
             kPanelX + 18, kPanelY + 52, 17, RAYWHITE);
    DrawText(TextFormat("Grid: (%i, %i, %i)  |  %s: %i", preview_.entryPosition.x, preview_.entryPosition.y,
                        preview_.entryPosition.z, (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist ||
                        preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) ? "length" : "radius",
                        (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist ||
                        preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) ? preview_.length : preview_.curveRadius),
             kPanelX + 18, kPanelY + 78, 16, RAYWHITE);
    DrawText(TextFormat("Road: width %i -> %i  |  ramp delta: %i", preview_.width, preview_.exitWidth,
                        preview_.elevationDelta), kPanelX + 18, kPanelY + 104, 16, Neon::Pink);
    DrawText(TextFormat("Offset: %i  |  Surface: %s", preview_.lateralOffset, MaterialName(preview_.material)),
             kPanelX + 18, kPanelY + 128, 16, Neon::Yellow);
    if (preview_.type == TrackPieceType::Curve) {
        DrawText(TextFormat("Turn: %s | %i deg | bank %i deg  (T/V, B/N)",
                            preview_.curveTurn == CurveTurn::Right ? "RIGHT" : "LEFT", preview_.curveDegrees,
                            preview_.bankAngleDegrees),
                 kPanelX + 18, kPanelY + 152, 16, Neon::Pink);
    }
    const int selectedY = kPanelY + 104 + detailOffset;
    const int validationY = selectedY + 28;
    const int issueY = validationY + 24;
    DrawText(TextFormat("Selected: %s", selectedPieceId_ == 0 ? "none" : TextFormat("piece %i", selectedPieceId_)),
             kPanelX + 18, selectedY, 16, selectedPieceId_ == 0 ? Fade(RAYWHITE, 0.65f) : Neon::Yellow);
    DrawText(validation.raceReady ? "RACE-READY: closed circuit" : "DRAFT: validation issues", kPanelX + 18,
             validationY, 17, validation.raceReady ? Neon::Green : Neon::Orange);
    DrawText(validation.raceReady ? "Start/finish line configured" : validation.issues.front().message.c_str(),
             kPanelX + 18, issueY, 14, validation.raceReady ? Fade(RAYWHITE, 0.75f) : Neon::Orange);
    DrawText(message_.c_str(), kPanelX + 18, kPanelY + 187 + detailOffset, 14, Neon::Yellow);
    DrawText("CONNECTORS  cyan = entry  |  pink = exit", kPanelX + 18, kPanelY + 215 + detailOffset, 14, Neon::Cyan);
    DrawText("A valid join puts an exit and entry on the same grid cell.", kPanelX + 18, kPanelY + 237 + detailOffset,
             13, Fade(RAYWHITE, 0.82f));
    DrawText("Their arrows must point in the same travel direction and have equal width.", kPanelX + 18,
             kPanelY + 256 + detailOffset, 13, Fade(RAYWHITE, 0.82f));
    DrawText("Mouse: left place/apply, Shift+left select, wheel rotate", kPanelX + 18, kPanelY + 280 + detailOffset,
             13, Fade(RAYWHITE, 0.72f));
    DrawText("Camera: WASD move, Q/E vertical, right-drag look, Shift+wheel zoom", kPanelX + 18,
             kPanelY + 297 + detailOffset,
             13, Fade(RAYWHITE, 0.72f));
    DrawText("Properties: Up/Down select a panel row; Left/Right adjust it", kPanelX + 18, kPanelY + 314 + detailOffset,
             13, Fade(RAYWHITE, 0.72f));
    DrawText("1/2 component type, R rotate, Home reset camera", kPanelX + 18,
             kPanelY + 331 + detailOffset, 13, Fade(RAYWHITE, 0.72f));
    DrawText("CUSTOM DRAFT: Ctrl+S save  Ctrl+O load", kPanelX + 18, kPanelY + 348 + detailOffset,
             13, Neon::Yellow);
    if (track_.IsLayoutValid()) {
        const TrackPiece* selected = track_.GetPiece(selectedPieceId_);
        const bool eligible = selected != 0 && selected->type == TrackPieceType::Straight;
        const Rectangle button = StartFinishButtonBounds();
        DrawRectangleRec(button, eligible ? Fade(Neon::Green, 0.86f) : Fade(Neon::Panel, 0.90f));
        DrawRectangleLinesEx(button, 2.0f, eligible ? Neon::Green : Fade(RAYWHITE, 0.36f));
        DrawText(eligible ? "SET START / FINISH" : "SELECT A STRAIGHT FOR START / FINISH",
                 static_cast<int>(button.x) + 12, static_cast<int>(button.y) + 9, 14,
                 eligible ? BLACK : Fade(RAYWHITE, 0.62f));
    } else {
        DrawText("Close the loop to unlock start/finish.", kPanelX + 18, 610,
                 14, Fade(RAYWHITE, 0.56f));
    }
    const bool canClear = !track_.Pieces().empty();
    const Rectangle clearButton = ClearTrackButtonBounds();
    DrawRectangleRec(clearButton, canClear ? Fade(Neon::Orange, 0.86f) : Fade(Neon::Panel, 0.90f));
    DrawRectangleLinesEx(clearButton, 2.0f, canClear ? Neon::Orange : Fade(RAYWHITE, 0.36f));
    DrawText("CLEAR TRACK", static_cast<int>(clearButton.x) + 28, static_cast<int>(clearButton.y) + 9, 14,
             canClear ? BLACK : Fade(RAYWHITE, 0.62f));
    DrawPropertyPanel();
    DrawTrackLibrary();
}

const Track& TrackEditor::GetTrack() const { return track_; }

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

void TrackEditor::MovePreview(int x, int z) {
    preview_.entryPosition.x += x;
    preview_.entryPosition.z += z;
}

void TrackEditor::RotatePreview() {
    preview_.entryHeading = static_cast<Heading>((static_cast<int>(preview_.entryHeading) + 1) % 4);
    SetMessage("Preview rotated 90 degrees.");
}

void TrackEditor::ChangeDimension(int amount) {
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
    if (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist) return 7;
    return preview_.type == TrackPieceType::Curve ? 9 : 7;
}

const char* TrackEditor::PropertyName(int index) const {
    static const char* straight[] = {"Length", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* curve[] = {"Radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Surface", "Turn", "Extent", "Bank"};
    static const char* loop[] = {"Radius", "Incoming width", "Outgoing width", "Height", "Ramp delta", "Offset", "Surface"};
    static const char* branch[] = {"Length", "Width", "Arm spread", "Surface"};
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) return branch[index];
    if (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist || preview_.type == TrackPieceType::Branch) return straight[index];
    return preview_.type == TrackPieceType::Curve ? curve[index] : loop[index];
}

std::string TrackEditor::PropertyValue(int index) const {
    if (preview_.type == TrackPieceType::Branch || preview_.type == TrackPieceType::Merge) {
        if (index == 0) return std::to_string(preview_.length);
        if (index == 1) return std::to_string(preview_.width);
        return index == 2 ? std::to_string(std::abs(preview_.lateralOffset)) : MaterialName(preview_.material);
    }
    if (index == 0) return std::to_string((preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist) ? preview_.length : preview_.curveRadius);
    if (index == 1) return std::to_string(preview_.width);
    if (index == 2) return std::to_string(preview_.exitWidth);
    if (index == 3) return std::to_string(preview_.entryPosition.y);
    if (index == 4) return std::to_string(preview_.elevationDelta);
    if (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist) {
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
    if (index == 0) return static_cast<float>(((preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist) ? preview_.length : preview_.curveRadius) - 3) / 17.0f;
    if (index == 1) return static_cast<float>(preview_.width - 5) / 6.0f;
    if (index == 2) return static_cast<float>(preview_.exitWidth - 5) / 6.0f;
    if (index == 3 || index == 4) return 0.5f;
    if (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist) return index == 5 ? (preview_.lateralOffset + 3.0f) / 6.0f : (static_cast<int>(preview_.material) + 1.0f) / 3.0f;
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
    if (index == 0) { ChangeDimension(direction); return; }
    if (index == 1) { preview_.width = std::max(5, std::min(11, preview_.width + direction)); return; }
    if (index == 2) { preview_.exitWidth = std::max(5, std::min(11, preview_.exitWidth + direction)); return; }
    if (index == 3) { preview_.entryPosition.y += direction; return; }
    if (index == 4) { preview_.elevationDelta += direction; return; }
    if (preview_.type == TrackPieceType::Straight || preview_.type == TrackPieceType::Twist) {
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
    DrawRectangleRec(save, Fade(Neon::Pink, 0.82f));
    DrawRectangleLinesEx(save, 2.0f, Neon::Pink);
    DrawText("SAVE AS", static_cast<int>(save.x) + 54, static_cast<int>(save.y) + 9, 15, BLACK);
    DrawRectangleRec(refresh, Fade(Neon::Panel, 0.95f));
    DrawRectangleLinesEx(refresh, 2.0f, Neon::Cyan);
    DrawText("REFRESH", static_cast<int>(refresh.x) + 54, static_cast<int>(refresh.y) + 9, 15, Neon::Cyan);

    if (namingDraft_) {
        DrawText("Type a track name, then press Enter:", 840, 188, 16, RAYWHITE);
        DrawRectangleRec(Rectangle{840.0f, 214.0f, 375.0f, 34.0f}, Fade(BLACK, 0.65f));
        DrawRectangleLinesEx(Rectangle{840.0f, 214.0f, 375.0f, 34.0f}, 2.0f, Neon::Yellow);
        DrawText(draftName_.c_str(), 850, 223, 18, Neon::Yellow);
        DrawText("Names save in your Neon Racer data folder.", 840, 265, 14, Fade(RAYWHITE, 0.68f));
        return;
    }

    DrawText("Saved editable drafts (click one to load):", 840, 175, 16, RAYWHITE);
    if (savedDrafts_.empty()) {
        DrawText("No saved custom tracks yet.", 840, 205, 16, Fade(RAYWHITE, 0.65f));
    }
    for (std::size_t index = 0; index < savedDrafts_.size() && index < 12; ++index) {
        const Rectangle row = Rectangle{840.0f, 192.0f + static_cast<float>(index) * 27.0f, 375.0f, 23.0f};
        DrawRectangleRec(row, Fade(Neon::Panel, 0.65f));
        DrawRectangleLinesEx(row, 1.0f, Fade(Neon::Cyan, 0.35f));
        DrawText(savedDrafts_[index].c_str(), static_cast<int>(row.x) + 10, static_cast<int>(row.y) + 4, 15, Neon::Cyan);
    }
    DrawText("Ctrl+S: save as   Ctrl+O/F5: open or close", 840, 540, 14, Neon::Yellow);
}

void TrackEditor::SetMessage(const std::string& message) { message_ = message; }
