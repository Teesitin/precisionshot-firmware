#pragma once

#include <cstddef>
#include <cstdint>

namespace hardware {
constexpr uint16_t LCD_WIDTH = 480;
constexpr uint16_t LCD_HEIGHT = 320;
void initialize();
void sleepMs(uint32_t milliseconds);
uint64_t nowMs();
void resetDisplay();
void resetTouch();
void beginDisplayWrite(bool dataMode);
void endDisplayWrite();
void writeDisplay(const uint8_t *bytes, size_t length);
void writePixels(uint16_t color, size_t count);
bool readTouchRegisters(uint8_t reg, uint8_t *bytes, size_t length);
}
