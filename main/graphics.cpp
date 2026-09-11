#include "graphics.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "hardware.h"
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#else
#define DMA_ATTR alignas(4)
#endif

namespace graphics {
namespace {
static_assert(WIDTH == hardware::LCD_WIDTH && HEIGHT == hardware::LCD_HEIGHT,
              "The indexed framebuffer must match the landscape panel.");
alignas(4) uint8_t framebuffer[FRAMEBUFFER_BYTES];
uint16_t currentPalette[16];
uint8_t paletteBytes[16][2];
// Four full rows fit inside hardware's 4096-byte SPI transaction limit.
constexpr size_t ROWS_PER_TRANSFER = 4;
DMA_ATTR uint8_t output[WIDTH * ROWS_PER_TRANSFER * 2];

void writeCommand(uint8_t command) {
  hardware::beginDisplayWrite(false);
  hardware::writeDisplay(&command, 1);
  hardware::endDisplayWrite();
}

void writeData8(uint8_t data) {
  hardware::beginDisplayWrite(true);
  hardware::writeDisplay(&data, 1);
  hardware::endDisplayWrite();
}

void writeData16(uint16_t data) {
  const uint8_t bytes[] = {static_cast<uint8_t>(data >> 8),
                           static_cast<uint8_t>(data)};
  hardware::beginDisplayWrite(true);
  hardware::writeDisplay(bytes, sizeof(bytes));
  hardware::endDisplayWrite();
}

const uint8_t *glyphFor(char character) {
  static const uint8_t space[5] = {0, 0, 0, 0, 0};
  static const uint8_t digits[10][5] = {
      {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
      {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
      {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
      {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
      {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E}};
  static const uint8_t letters[26][5] = {
      {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
      {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
      {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
      {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
      {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
      {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
      {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
      {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}};
  // Lowercase is distinct so the session debugger shows the actual BLE text.
  static const uint8_t lowercase[26][5] = {
      {0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},
      {0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
      {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},
      {0x0C,0x52,0x52,0x52,0x3E},{0x7F,0x08,0x04,0x04,0x78},
      {0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
      {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},
      {0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},
      {0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
      {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},
      {0x48,0x54,0x54,0x54,0x20},{0x04,0x3F,0x44,0x40,0x20},
      {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
      {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},
      {0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44}};
  static const char punctuation[] = "+-:{}\",/.=<>%#[]()!?_\\'|*";
  static const uint8_t punctuationGlyphs[][5] = {
      {0x08,0x08,0x3E,0x08,0x08}, // +
      {0x08,0x08,0x08,0x08,0x08}, // -
      {0x00,0x36,0x36,0x00,0x00}, // :
      {0x00,0x08,0x36,0x41,0x00}, // {
      {0x00,0x41,0x36,0x08,0x00}, // }
      {0x00,0x07,0x00,0x07,0x00}, // double quote
      {0x00,0x40,0x30,0x00,0x00}, // ,
      {0x40,0x20,0x10,0x08,0x04}, // /
      {0x00,0x60,0x60,0x00,0x00}, // .
      {0x14,0x14,0x14,0x14,0x14}, // =
      {0x08,0x14,0x22,0x41,0x00}, // <
      {0x00,0x41,0x22,0x14,0x08}, // >
      {0x63,0x13,0x08,0x64,0x63}, // %
      {0x14,0x7F,0x14,0x7F,0x14}, // #
      {0x00,0x7F,0x41,0x41,0x00}, // [
      {0x00,0x41,0x41,0x7F,0x00}, // ]
      {0x00,0x1C,0x22,0x41,0x00}, // (
      {0x00,0x41,0x22,0x1C,0x00}, // )
      {0x00,0x00,0x5F,0x00,0x00}, // !
      {0x02,0x01,0x51,0x09,0x06}, // ?
      {0x40,0x40,0x40,0x40,0x40}, // _
      {0x01,0x02,0x04,0x08,0x10}, // backslash
      {0x00,0x00,0x07,0x00,0x00}, // single quote
      {0x00,0x00,0x7F,0x00,0x00}, // |
      {0x14,0x08,0x3E,0x08,0x14}}; // *
  static_assert(sizeof(punctuation) - 1 == sizeof(punctuationGlyphs) / 5,
                "Every punctuation character must have a glyph.");

  if (character >= 'a' && character <= 'z') return lowercase[character - 'a'];
  if (character >= 'A' && character <= 'Z') return letters[character - 'A'];
  if (character >= '0' && character <= '9') return digits[character - '0'];
  const char *found = std::strchr(punctuation, character);
  if (found != nullptr && character != '\0') {
    return punctuationGlyphs[found - punctuation];
  }
  return space;
}
}

void initialize() {
  hardware::resetDisplay();

  // Keep the proven project-owned ST7796S initialization byte-for-byte.
  writeCommand(0xF0); writeData8(0xC3);
  writeCommand(0xF0); writeData8(0x96);
  writeCommand(0x36); writeData8(0x28);
  writeCommand(0x3A); writeData8(0x05);
  writeCommand(0xB0); writeData8(0x80);
  writeCommand(0xB6); writeData8(0x00); writeData8(0x02);
  writeCommand(0xB5); writeData8(0x02); writeData8(0x03);
  writeData8(0x00); writeData8(0x04);
  writeCommand(0xB1); writeData8(0x80); writeData8(0x10);
  writeCommand(0xB4); writeData8(0x00);
  writeCommand(0xB7); writeData8(0xC6);
  writeCommand(0xC5); writeData8(0x1C);
  writeCommand(0xE4); writeData8(0x31);
  writeCommand(0xE8);
  writeData8(0x40); writeData8(0x8A); writeData8(0x00); writeData8(0x00);
  writeData8(0x29); writeData8(0x19); writeData8(0xA5); writeData8(0x33);
  writeCommand(0xC2); writeCommand(0xA7);

  writeCommand(0xE0);
  const uint8_t positiveGamma[] = {0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C,
                                   0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21};
  for (uint8_t value : positiveGamma) writeData8(value);
  writeCommand(0xE1);
  const uint8_t negativeGamma[] = {0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B,
                                   0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21};
  for (uint8_t value : negativeGamma) writeData8(value);

  writeCommand(0xF0); writeData8(0x3C);
  writeCommand(0xF0); writeData8(0x69);
  writeCommand(0x13);
  writeCommand(0x11);
  hardware::sleepMs(120);
  writeCommand(0x29);
}

void setPalette(const uint16_t colors[16]) {
  for (unsigned i = 0; i < 16; ++i) {
    currentPalette[i] = colors[i];
    paletteBytes[i][0] = static_cast<uint8_t>(colors[i] >> 8);
    paletteBytes[i][1] = static_cast<uint8_t>(colors[i]);
  }
}

void clear(uint8_t color) {
  color &= 0x0F;
  std::memset(framebuffer, color | (color << 4), sizeof(framebuffer));
}

void fillRect(int x, int y, int width, int height, uint8_t color) {
  if (width <= 0 || height <= 0) return;
  // Wide additions keep clipping well-defined even for extreme offscreen input.
  const int left = static_cast<int>(std::max<int64_t>(0, x));
  const int top = static_cast<int>(std::max<int64_t>(0, y));
  const int right = static_cast<int>(std::min<int64_t>(WIDTH,
      static_cast<int64_t>(x) + width));
  const int bottom = static_cast<int>(std::min<int64_t>(HEIGHT,
      static_cast<int64_t>(y) + height));
  if (left >= right || top >= bottom) return;
  color &= 0x0F;
  const uint8_t pair = color | (color << 4);
  for (int row = top; row < bottom; ++row) {
    uint8_t *destination = framebuffer + (row * WIDTH + left) / 2;
    int remaining = right - left;
    if (left & 1) {
      *destination = (*destination & 0xF0) | color;
      ++destination;
      --remaining;
    }
    const int pairs = remaining / 2;
    std::memset(destination, pair, pairs);
    if (remaining & 1) {
      destination[pairs] = (destination[pairs] & 0x0F) | (color << 4);
    }
  }
}

void fillCircle(int centerX, int centerY, int radius, uint8_t color) {
  if (radius < 0) return;
  // Evaluate only visible rows, also allowing circles to animate offscreen.
  const int top = std::max(-radius, -centerY);
  const int bottom = std::min(radius, HEIGHT - 1 - centerY);
  const double square = static_cast<double>(radius) * radius;
  for (int y = top; y <= bottom; ++y) {
    const int half = static_cast<int>(std::sqrt(square - static_cast<double>(y) * y));
    fillRect(centerX - half, centerY + y, half * 2 + 1, 1, color);
  }
}

void roundedRect(int x, int y, int width, int height, int radius, uint8_t color) {
  if (width <= 0 || height <= 0) return;
  radius = std::max(0, std::min(radius, std::min(width, height) / 2));
  if (radius == 0) {
    fillRect(x, y, width, height, color);
    return;
  }
  fillRect(x, y + radius, width, height - radius * 2, color);
  const double square = static_cast<double>(radius) * radius;
  for (int row = 0; row < radius; ++row) {
    const double distance = radius - row - 0.5;
    const int inset = static_cast<int>(std::ceil(radius - std::sqrt(square - distance * distance) - 0.5));
    fillRect(x + inset, y + row, width - inset * 2, 1, color);
    fillRect(x + inset, y + height - 1 - row, width - inset * 2, 1, color);
  }
}

void bitmap24(int x, int y, const uint8_t (&bits)[72], uint8_t color) {
  for (int row = 0; row < 24; ++row) {
    for (int col = 0; col < 24; ++col) {
      if (bits[row * 3 + col / 8] & (0x80 >> (col % 8))) {
        fillRect(x + col, y + row, 1, 1, color);
      }
    }
  }
}

int textWidth(const char *value, int scale) {
  if (value == nullptr || scale <= 0 || *value == '\0') return 0;
  return static_cast<int>(std::strlen(value)) * 6 * scale - scale;
}

void text(int x, int y, const char *value, uint8_t color, int scale) {
  if (value == nullptr || scale <= 0) return;
  while (*value) {
    const uint8_t *glyph = glyphFor(*value++);
    for (int column = 0; column < 5; ++column) {
      // Draw each contiguous vertical run in a single rectangle.
      int row = 0;
      while (row < 7) {
        if ((glyph[column] & (1 << row)) == 0) {
          ++row;
          continue;
        }
        const int start = row++;
        while (row < 7 && (glyph[column] & (1 << row))) ++row;
        fillRect(x + column * scale, y + start * scale, scale,
                 (row - start) * scale, color);
      }
    }
    x += 6 * scale;
  }
}

void centeredText(int x, int width, int y, const char *value, uint8_t color,
                  int scale) {
  text(x + (width - textWidth(value, scale)) / 2, y, value, color, scale);
}

void present() {
  writeCommand(0x2A);
  writeData16(0); writeData16(WIDTH - 1);
  writeCommand(0x2B);
  writeData16(0); writeData16(HEIGHT - 1);
  writeCommand(0x2C);
  hardware::beginDisplayWrite(true);
  constexpr size_t INPUT_CHUNK = sizeof(output) / 4;
  for (size_t offset = 0; offset < sizeof(framebuffer); offset += INPUT_CHUNK) {
    const size_t packedCount = std::min(INPUT_CHUNK, sizeof(framebuffer) - offset);
    for (size_t i = 0; i < packedCount; ++i) {
      const uint8_t pair = framebuffer[offset + i];
      const uint8_t *first = paletteBytes[pair >> 4];
      const uint8_t *second = paletteBytes[pair & 0x0F];
      output[4 * i] = first[0];
      output[4 * i + 1] = first[1];
      output[4 * i + 2] = second[0];
      output[4 * i + 3] = second[1];
    }
    hardware::writeDisplay(output, packedCount * 4);
  }
  hardware::endDisplayWrite();
}

const uint8_t *pixels() { return framebuffer; }
const uint16_t *palette() { return currentPalette; }
}
