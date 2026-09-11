#pragma once

#include <cstddef>
#include <cstdint>

// Project-owned, allocation-free ST7796S renderer. Colors are palette indices
// (0..15); the palette itself contains the panel's RGB565 color values.
namespace graphics {
constexpr int WIDTH = 480;
constexpr int HEIGHT = 320;
constexpr size_t FRAMEBUFFER_BYTES = WIDTH * HEIGHT / 2;

// Call after hardware::initialize(). Keeps the existing landscape panel setup.
void initialize();
void setPalette(const uint16_t colors[16]);
void clear(uint8_t color);
void fillRect(int x, int y, int width, int height, uint8_t color);
void fillCircle(int centerX, int centerY, int radius, uint8_t color);
void roundedRect(int x, int y, int width, int height, int radius, uint8_t color);
// Transparent 24x24 monochrome bitmap, 3 bytes/row, MSB first.
void bitmap24(int x, int y, const uint8_t (&bits)[72], uint8_t color);
void text(int x, int y, const char *value, uint8_t color, int scale = 1);
int textWidth(const char *value, int scale = 1);
void centeredText(int x, int width, int y, const char *value, uint8_t color,
                  int scale = 1);
// Flush one fully composed frame; there are no intermediate visible UI clears.
void present();

// Read-only access for screenshots and native renderer verification. Pixels
// are packed left-to-right, with the first pixel in each byte's high nibble.
const uint8_t *pixels();
const uint16_t *palette();
}
