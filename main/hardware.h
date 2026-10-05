#pragma once

#include <cstddef>
#include <cstdint>

namespace hardware {
// Screen size and board setup.
constexpr uint16_t LCD_WIDTH = 480;
constexpr uint16_t LCD_HEIGHT = 320;
void initialize();
bool startSpeakerTest();
// Update the beep or tune from the main loop.
void advanceSpeakerTest(uint64_t now);
bool speakerTestActive();
struct SpeakerNote {
    uint16_t frequencyHz; // Zero is a rest.
    uint16_t durationMs;
};
// Keep the note table in memory until playback ends. Tunes use static arrays.
bool startSpeakerSequence(const SpeakerNote *notes, size_t count);
void stopSpeakerSequence();
// Timing and display/touch communication.
void sleepMs(uint32_t milliseconds);
uint64_t nowMs();
void resetDisplay();
void resetTouch();
void beginDisplayWrite(bool dataMode);
void endDisplayWrite();
void writeDisplay(const uint8_t *bytes, size_t length);
void writePixels(uint16_t color, size_t count);
bool readTouchRegisters(uint8_t reg, uint8_t *bytes, size_t length);
} // namespace hardware
