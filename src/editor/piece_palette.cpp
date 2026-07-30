#include "piece_palette.hpp"

#include "../ui/neon.hpp"

#include <algorithm>
#include <cmath>

namespace {

const float kClosedHeight = 32.0f;
const float kExpandedHeight = 164.0f;
const float kPanelMargin = 28.0f;
const float kMaximumPanelWidth = 650.0f;
const float kCardGap = 6.0f;
const float kCardMargin = 12.0f;
const float kPi = 3.14159265358979323846f;
const std::size_t kPaletteCapacity = 6;

bool Contains(const Rectangle& bounds, Vector2 point) {
    return CheckCollisionPointRec(point, bounds);
}

float ClampUnit(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

Color WithAlpha(Color color, float alpha) {
    return Fade(color, ClampUnit(alpha));
}

void DrawRoadSegment(Vector2 from, Vector2 to, Color color, float alpha) {
    DrawLineEx(from, to, 5.0f, WithAlpha(color, alpha));
    DrawLineEx(from, to, 1.5f, WithAlpha(RAYWHITE, alpha * 0.48f));
}

void DrawEndpoint(Vector2 position, Color color, float alpha) {
    DrawCircleV(position, 3.2f, WithAlpha(color, alpha));
    DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), 4.8f,
                    WithAlpha(RAYWHITE, alpha * 0.65f));
}

void DrawCurvePath(const Rectangle& bounds, Color color, float alpha) {
    const float left = bounds.x + 8.0f;
    const float right = bounds.x + bounds.width - 8.0f;
    const float bottom = bounds.y + bounds.height - 7.0f;
    const float top = bounds.y + 7.0f;
    Vector2 previous{left, bottom};
    for (int segment = 1; segment <= 12; ++segment) {
        const float progress = static_cast<float>(segment) / 12.0f;
        const Vector2 current{left + (right - left) * progress,
                              bottom - (bottom - top) * std::sin(progress * kPi * 0.5f)};
        DrawRoadSegment(previous, current, color, alpha);
        previous = current;
    }
    DrawEndpoint(Vector2{left, bottom}, Neon::Cyan, alpha);
    DrawEndpoint(previous, Neon::Pink, alpha);
}

void DrawLoopPath(const Rectangle& bounds, Color color, float alpha) {
    const float centerX = bounds.x + bounds.width * 0.5f;
    const float centerY = bounds.y + bounds.height * 0.52f;
    const float radiusX = std::max(8.0f, bounds.width * 0.23f);
    const float radiusY = std::max(8.0f, bounds.height * 0.36f);
    Vector2 previous{centerX, centerY + radiusY};
    for (int segment = 1; segment <= 18; ++segment) {
        const float angle = kPi * 0.5f + static_cast<float>(segment) * (kPi * 2.0f / 18.0f);
        const Vector2 current{centerX + std::cos(angle) * radiusX, centerY + std::sin(angle) * radiusY};
        DrawRoadSegment(previous, current, color, alpha);
        previous = current;
    }
    DrawEndpoint(Vector2{centerX, centerY + radiusY}, Neon::Cyan, alpha);
    DrawEndpoint(previous, Neon::Pink, alpha);
}

void DrawTwistPath(const Rectangle& bounds, Color color, float alpha) {
    const float left = bounds.x + 8.0f;
    const float right = bounds.x + bounds.width - 8.0f;
    const float middle = bounds.y + bounds.height * 0.5f;
    const float amplitude = std::max(5.0f, bounds.height * 0.27f);
    Vector2 previous{left, middle};
    for (int segment = 1; segment <= 16; ++segment) {
        const float progress = static_cast<float>(segment) / 16.0f;
        const Vector2 current{left + (right - left) * progress,
                              middle + std::sin(progress * kPi * 2.0f) * amplitude};
        DrawRoadSegment(previous, current, color, alpha);
        previous = current;
    }
    DrawEndpoint(Vector2{left, middle}, Neon::Cyan, alpha);
    DrawEndpoint(previous, Neon::Pink, alpha);
}

void DrawBranchPath(const Rectangle& bounds, bool merge, Color color, float alpha) {
    const float left = bounds.x + 8.0f;
    const float right = bounds.x + bounds.width - 8.0f;
    const float middle = bounds.y + bounds.height * 0.5f;
    const float split = bounds.x + bounds.width * 0.48f;
    const float offset = std::max(8.0f, bounds.height * 0.30f);
    if (!merge) {
        DrawRoadSegment(Vector2{left, middle}, Vector2{split, middle}, color, alpha);
        DrawRoadSegment(Vector2{split, middle}, Vector2{right, middle - offset}, color, alpha);
        DrawRoadSegment(Vector2{split, middle}, Vector2{right, middle + offset}, color, alpha);
        DrawEndpoint(Vector2{left, middle}, Neon::Cyan, alpha);
        DrawEndpoint(Vector2{right, middle - offset}, Neon::Pink, alpha);
        DrawEndpoint(Vector2{right, middle + offset}, Neon::Pink, alpha);
    } else {
        DrawRoadSegment(Vector2{left, middle - offset}, Vector2{split, middle}, color, alpha);
        DrawRoadSegment(Vector2{left, middle + offset}, Vector2{split, middle}, color, alpha);
        DrawRoadSegment(Vector2{split, middle}, Vector2{right, middle}, color, alpha);
        DrawEndpoint(Vector2{left, middle - offset}, Neon::Cyan, alpha);
        DrawEndpoint(Vector2{left, middle + offset}, Neon::Cyan, alpha);
        DrawEndpoint(Vector2{right, middle}, Neon::Pink, alpha);
    }
}

void DrawPieceIcon(TrackPieceType type, const Rectangle& bounds, Color color, float alpha) {
    if (type == TrackPieceType::Straight) {
        const float middle = bounds.y + bounds.height * 0.5f;
        const Vector2 first{bounds.x + 8.0f, middle};
        const Vector2 second{bounds.x + bounds.width - 8.0f, middle};
        DrawRoadSegment(first, second, color, alpha);
        DrawEndpoint(first, Neon::Cyan, alpha);
        DrawEndpoint(second, Neon::Pink, alpha);
    } else if (type == TrackPieceType::Curve) {
        DrawCurvePath(bounds, color, alpha);
    } else if (type == TrackPieceType::Loop) {
        DrawLoopPath(bounds, color, alpha);
    } else if (type == TrackPieceType::Twist) {
        DrawTwistPath(bounds, color, alpha);
    } else {
        DrawBranchPath(bounds, type == TrackPieceType::Merge, color, alpha);
    }
}

const EditorPieceCatalog::Item* ItemForPointer(const Rectangle* itemBounds, std::size_t itemBoundCount,
                                               Vector2 pointer) {
    std::size_t itemCount = 0;
    const EditorPieceCatalog::Item* items = EditorPieceCatalog::Items(itemCount);
    const std::size_t availableItems = std::min(itemCount, itemBoundCount);
    for (std::size_t index = 0; index < availableItems; ++index) {
        if (Contains(itemBounds[index], pointer)) return &items[index];
    }
    return 0;
}

} // namespace

PiecePalette::PiecePalette()
    : openAmount_(0.0f), screenWidth_(1), screenHeight_(1), enabled_(false) {
}

void PiecePalette::Update(Vector2 pointer, int screenWidth, int screenHeight, float frameTime, bool enabled) {
    screenWidth_ = std::max(1, screenWidth);
    screenHeight_ = std::max(1, screenHeight);
    enabled_ = enabled;

    const Layout layout = CurrentLayout();
    const bool canReachExpandedPanel = openAmount_ > 0.01f && Contains(layout.fullBounds, pointer);
    const bool shouldOpen = enabled_ && (Contains(layout.handleBounds, pointer) || canReachExpandedPanel);
    const float target = shouldOpen ? 1.0f : 0.0f;
    const float speed = target > openAmount_ ? 10.0f : 8.0f;
    const float change = std::max(0.0f, frameTime) * speed;
    if (target > openAmount_) openAmount_ = std::min(target, openAmount_ + change);
    else openAmount_ = std::max(target, openAmount_ - change);
}

bool PiecePalette::ConsumesPointer(Vector2 pointer) const {
    if (!enabled_) return false;
    const Layout layout = CurrentLayout();
    return Contains(layout.handleBounds, pointer) ||
        (openAmount_ > 0.01f && Contains(layout.fullBounds, pointer));
}

bool PiecePalette::ConsumeSelection(Vector2 pointer, TrackPieceType& selectedType) const {
    if (!enabled_ || openAmount_ < 0.82f) return false;
    const Layout layout = CurrentLayout();
    const EditorPieceCatalog::Item* item = ItemForPointer(layout.itemBounds, layout.itemCount, pointer);
    if (item == 0) return false;
    selectedType = item->type;
    return true;
}

void PiecePalette::Draw(TrackPieceType selectedType) const {
    if (!enabled_) return;

    const Layout layout = CurrentLayout();
    const float closedCaptionAlpha = 1.0f - ClampUnit(openAmount_ * 1.8f);
    const float contentAlpha = ClampUnit((openAmount_ - 0.12f) / 0.88f);

    Neon::DrawOverlayPanel(layout.visibleBounds, 0.90f);
    DrawRectangleLinesEx(layout.visibleBounds, 1.5f, WithAlpha(Neon::Cyan, 0.44f + openAmount_ * 0.30f));
    DrawLine(static_cast<int>(layout.visibleBounds.x) + 12, static_cast<int>(layout.visibleBounds.y) + 2,
             static_cast<int>(layout.visibleBounds.x + layout.visibleBounds.width) - 12,
             static_cast<int>(layout.visibleBounds.y) + 2, WithAlpha(Neon::Cyan, 0.42f));

    if (closedCaptionAlpha > 0.01f) {
        const char* caption = "PIECE LIBRARY   /   HOVER TO BROWSE";
        const int fontSize = 13;
        const int x = static_cast<int>(layout.handleBounds.x +
                                       (layout.handleBounds.width - MeasureText(caption, fontSize)) * 0.5f);
        DrawText(caption, x, static_cast<int>(layout.handleBounds.y) + 9, fontSize,
                 WithAlpha(Neon::Cyan, closedCaptionAlpha));
    }

    if (contentAlpha <= 0.01f) return;
    BeginScissorMode(static_cast<int>(layout.visibleBounds.x), static_cast<int>(layout.visibleBounds.y),
                     static_cast<int>(layout.visibleBounds.width), static_cast<int>(layout.visibleBounds.height));
    DrawText("PIECE LIBRARY", static_cast<int>(layout.fullBounds.x) + 12,
             static_cast<int>(layout.fullBounds.y) + 12, 16, WithAlpha(Neon::Cyan, contentAlpha));
    DrawText("Click a component to select it  /  1-6 remain shortcuts",
             static_cast<int>(layout.fullBounds.x) + 132, static_cast<int>(layout.fullBounds.y) + 14, 12,
             WithAlpha(RAYWHITE, contentAlpha * 0.72f));

    const Vector2 pointer = GetMousePosition();
    std::size_t itemCount = 0;
    const EditorPieceCatalog::Item* items = EditorPieceCatalog::Items(itemCount);
    const std::size_t availableItems = std::min(itemCount, layout.itemCount);
    for (std::size_t index = 0; index < availableItems; ++index) {
        const EditorPieceCatalog::Item& item = items[index];
        const Rectangle card = layout.itemBounds[index];
        const bool hovered = Contains(card, pointer);
        const bool active = item.type == selectedType;
        const Color outline = active ? Neon::Yellow : (hovered ? Neon::Pink : Neon::Cyan);
        const Color road = active ? Neon::Yellow : (hovered ? Neon::Pink : Neon::Cyan);
        DrawRectangleRounded(card, 0.12f, 5, WithAlpha(Neon::Panel, contentAlpha * (hovered ? 0.98f : 0.80f)));
        DrawRectangleLinesEx(card, active ? 2.0f : 1.0f,
                             WithAlpha(outline, contentAlpha * (active || hovered ? 0.94f : 0.50f)));
        DrawPieceIcon(item.type, Rectangle{card.x + 6.0f, card.y + 7.0f, card.width - 12.0f, 46.0f}, road, contentAlpha);

        const int labelSize = card.width < 92.0f ? 10 : 12;
        const int labelX = static_cast<int>(card.x + (card.width - MeasureText(item.name, labelSize)) * 0.5f);
        DrawText(item.name, labelX, static_cast<int>(card.y) + 67, labelSize,
                 WithAlpha(active ? Neon::Yellow : RAYWHITE, contentAlpha));

        const Rectangle shortcut{card.x + card.width - 21.0f, card.y + 6.0f, 15.0f, 15.0f};
        DrawRectangleRec(shortcut, WithAlpha(outline, contentAlpha * 0.72f));
        DrawText(TextFormat("%i", item.shortcut), static_cast<int>(shortcut.x) + 4,
                 static_cast<int>(shortcut.y) + 2, 11, WithAlpha(BLACK, contentAlpha));
    }
    EndScissorMode();
}

PiecePalette::Layout PiecePalette::CurrentLayout() const {
    std::size_t itemCount = 0;
    EditorPieceCatalog::Items(itemCount);
    const std::size_t visibleItemCount = std::min(itemCount, kPaletteCapacity);
    const float availableWidth = std::max(1.0f, static_cast<float>(screenWidth_) - kPanelMargin * 2.0f);
    const float panelWidth = std::min(kMaximumPanelWidth, availableWidth);
    const float panelX = screenWidth_ >= 1260
        ? static_cast<float>(screenWidth_) - kPanelMargin - panelWidth
        : (static_cast<float>(screenWidth_) - panelWidth) * 0.5f;
    const float visibleHeight = kClosedHeight + (kExpandedHeight - kClosedHeight) * openAmount_;
    const float bottom = static_cast<float>(screenHeight_);
    Layout layout{};
    layout.visibleBounds = Rectangle{panelX, bottom - visibleHeight, panelWidth, visibleHeight};
    layout.fullBounds = Rectangle{panelX, bottom - kExpandedHeight, panelWidth, kExpandedHeight};
    layout.handleBounds = Rectangle{panelX, bottom - kClosedHeight, panelWidth, kClosedHeight};
    layout.itemCount = visibleItemCount;

    const float totalGap = kCardGap * static_cast<float>(visibleItemCount > 0 ? visibleItemCount - 1 : 0);
    const float cardWidth = (panelWidth - kCardMargin * 2.0f - totalGap) /
                            static_cast<float>(visibleItemCount == 0 ? 1 : visibleItemCount);
    for (std::size_t index = 0; index < visibleItemCount; ++index) {
        layout.itemBounds[index] = Rectangle{panelX + kCardMargin + static_cast<float>(index) * (cardWidth + kCardGap),
                                             bottom - 118.0f, cardWidth, 108.0f};
    }
    return layout;
}
