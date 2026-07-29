#pragma once

#include <raylib.h>

// Shared visual language for the neon games.  Game-specific art should stay in
// its own directory; this module only contains the common building blocks.
namespace Neon {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;
constexpr int HEADER_HEIGHT = 52;

extern const Color Background;
extern const Color BackgroundTop;
extern const Color BackgroundBottom;
extern const Color Panel;
extern const Color Cyan;
extern const Color Pink;
extern const Color Yellow;
extern const Color Green;
extern const Color Orange;

void DrawBackground(int width = SCREEN_WIDTH, int height = SCREEN_HEIGHT);
void DrawHeader(const char* title, int width = SCREEN_WIDTH, int height = HEADER_HEIGHT);
void DrawNeonBlock(Rectangle bounds, Color color, float alpha = 1.0f);
void DrawOverlayPanel(Rectangle bounds, float opacity = 0.84f);
void DrawCenteredText(const char* text, int width, int y, int fontSize, Color color);

} // namespace Neon
