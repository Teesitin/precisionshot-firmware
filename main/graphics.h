#pragma once

#include <cstddef>
#include <cstdint>

// Simple screen drawing. Each pixel stores a color number from 0 to 15.
namespace graphics {
constexpr int WIDTH = 480;
constexpr int HEIGHT = 320;
constexpr size_t FRAMEBUFFER_BYTES = WIDTH * HEIGHT / 2;

// Start this after the hardware is ready. The screen uses landscape mode.
void initialize();
void setPalette(const uint16_t colors[16]);
void clear(uint8_t color);
void fillRect(int x, int y, int width, int height, uint8_t color);
void fillCircle(int centerX, int centerY, int radius, uint8_t color);
void roundedRect(int x, int y, int width, int height, int radius, uint8_t color);
// Draw a 24x24 icon. Zero bits leave the background alone.
void bitmap24(int x, int y, const uint8_t (&bits)[72], uint8_t color);
void text(int x, int y, const char *value, uint8_t color, int scale = 1);
int textWidth(const char *value, int scale = 1);
void centeredText(int x, int width, int y, const char *value, uint8_t color, int scale = 1);
// Send the finished frame to the screen.
void present();

// Used by screenshot checks. Each byte holds two pixels, left pixel first.
const uint8_t *pixels();
const uint16_t *palette();
} // namespace graphics
