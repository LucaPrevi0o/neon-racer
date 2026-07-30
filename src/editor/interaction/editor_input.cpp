#include "neon_racer/editor/editor.hpp"

#include "neon_racer/editor/editor_camera.hpp"
#include "neon_racer/editor/editor_picking.hpp"
#include "neon_racer/editor/piece_catalog.hpp"

#include <cstddef>

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

} // namespace

void TrackEditor::Update(Camera3D& camera) {
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

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
    if (libraryConsumed) return;

    const bool paletteConsumesPointer = piecePalette_.ConsumesPointer(mouse);
    const bool editorPanelConsumesPointer = helpPanelExpanded_ && CheckCollisionPointRec(mouse, EditorPanelBounds());
    const bool leftClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    TrackPieceType paletteSelection = TrackPieceType::Straight;
    const bool piecePaletteClicked = leftClick && piecePalette_.ConsumeSelection(mouse, paletteSelection);
    const bool helpToggled = !paletteConsumesPointer && leftClick &&
        CheckCollisionPointRec(mouse, HelpToggleBounds(helpPanelExpanded_));
    if (helpToggled) {
        helpPanelExpanded_ = !helpPanelExpanded_;
        return;
    }

    const int pieceShortcutKeys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX};
    for (std::size_t index = 0; index < sizeof(pieceShortcutKeys) / sizeof(pieceShortcutKeys[0]); ++index) {
        if (!IsKeyPressed(pieceShortcutKeys[index])) continue;
        const EditorPieceCatalog::Item* item = EditorPieceCatalog::FindShortcut(static_cast<int>(index) + 1);
        if (item != 0) SelectPreviewType(item->type);
    }

    const float wheel = GetMouseWheelMove();
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (!paletteConsumesPointer && !editorPanelConsumesPointer) {
        if (control && wheel > 0.0f) ChangeDimension(1);
        if (control && wheel < 0.0f) ChangeDimension(-1);
        if (!control && shift && wheel != 0.0f) EditorCamera::Zoom(camera, wheel);
        if (!control && !shift && wheel > 0.0f) RotatePreview();
        if (!control && !shift && wheel < 0.0f) {
            preview_.entryHeading = static_cast<Heading>((static_cast<int>(preview_.entryHeading) + 3) % 4);
            SetMessage("Preview rotated 90 degrees.");
        }
    }
    if (!paletteConsumesPointer && !editorPanelConsumesPointer && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        EditorCamera::Orbit(camera, GetMouseDelta());
    }
    if (IsKeyPressed(KEY_HOME)) EditorCamera::Reset(camera);
    if (IsKeyPressed(KEY_R)) RotatePreview();
    if (IsKeyPressed(KEY_UP)) CycleProperty(-1);
    if (IsKeyPressed(KEY_DOWN)) CycleProperty(1);
    if (IsKeyPressed(KEY_LEFT)) AdjustSelectedProperty(-1);
    if (IsKeyPressed(KEY_RIGHT)) AdjustSelectedProperty(1);

    if (IsKeyPressed(KEY_F) && selectedPieceId_ != 0) {
        const TrackPiece* selected = track_.GetPiece(selectedPieceId_);
        if (selected != 0) {
            preview_ = *selected;
            preview_.id = 0;
            SetMessage("Selected component copied into the placement preview.");
        }
    }

    GridPosition mousePosition;
    if (!paletteConsumesPointer && !editorPanelConsumesPointer && selectedPieceId_ == 0) {
        const Ray mouseRay = GetMouseRay(mouse, camera);
        if (EditorPicking::GridPositionFromRay(mouseRay, mousePosition)) {
            preview_.entryPosition.x = mousePosition.x;
            preview_.entryPosition.z = mousePosition.z;
        }
    }

    const bool selectionClick = leftClick && shift;
    const bool clearTrackClicked = !paletteConsumesPointer && helpPanelExpanded_ && leftClick &&
        CheckCollisionPointRec(mouse, ClearTrackButtonBounds());
    const bool startFinishClicked = !paletteConsumesPointer && helpPanelExpanded_ && leftClick &&
        CheckCollisionPointRec(mouse, StartFinishButtonBounds());
    if (piecePaletteClicked) {
        SelectPreviewType(paletteSelection);
    } else if (clearTrackClicked) {
        ClearTrack();
    } else if (startFinishClicked) {
        SetStartFinish();
    } else if (!paletteConsumesPointer && !editorPanelConsumesPointer && selectionClick) {
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
    } else if (IsKeyPressed(KEY_ENTER) || (!paletteConsumesPointer && !editorPanelConsumesPointer && leftClick)) {
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
