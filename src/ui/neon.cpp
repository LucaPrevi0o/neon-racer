#include "neon.hpp"

namespace Neon {

const Color Background{5, 8, 22, 255};
const Color BackgroundTop{12, 18, 46, 255};
const Color BackgroundBottom{4, 6, 17, 255};
const Color Panel{8, 12, 31, 255};
const Color Cyan{0, 232, 255, 255};
const Color Pink{255, 69, 184, 255};
const Color Yellow{255, 219, 57, 255};
const Color Green{52, 238, 139, 255};
const Color Orange{255, 134, 47, 255};

void DrawBackground(int width, int height) {
    ClearBackground(Background);
    DrawRectangleGradientV(0, 0, width, height, BackgroundTop, BackgroundBottom);
}

void DrawHeader(const char* title, int width, int height) {
    DrawRectangleGradientV(0, 0, width, height, Color{15, 25, 59, 245}, Background);
    DrawLine(0, height - 1, width, height - 1, Fade(SKYBLUE, 0.60f));
    DrawText(title, 22, 13, 28, Cyan);
}

void DrawNeonBlock(Rectangle bounds, Color color, float alpha) {
    DrawRectangleRounded(Rectangle{bounds.x - 4, bounds.y - 4, bounds.width + 8, bounds.height + 8},
                         0.18f, 5, Fade(color, 0.14f * alpha));
    DrawRectangleRounded(bounds, 0.15f, 5, Fade(color, alpha));
    DrawRectangleRounded(Rectangle{bounds.x + 3, bounds.y + 3, bounds.width - 6, bounds.height - 6},
                         0.12f, 4, Fade(Panel, alpha));
    DrawRectangle(bounds.x + 5, bounds.y + 5, bounds.width - 10, 3, Fade(RAYWHITE, 0.35f * alpha));
}

void DrawOverlayPanel(Rectangle bounds, float opacity) {
    DrawRectangleRounded(bounds, 0.08f, 8, Fade(BLACK, opacity));
}

void DrawCenteredText(const char* text, int width, int y, int fontSize, Color color) {
    DrawText(text, (width - MeasureText(text, fontSize)) / 2, y, fontSize, color);
}

} // namespace Neon
