#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "sdkconfig.h"

#include "bluetooth.h"
#include "graphics.h"
#include "icons.h"
#include "hardware.h"
#include "session.h"

// All UI/session state belongs to the main task. Radio callbacks only queue events.
enum Color : uint8_t {
  Background, Surface, SurfaceAlt, Header, Accent, Pressed, Text, Muted,
  OnAccent, Success, Danger, Line, Shade, Highlight, Reserved14, Reserved15
};
constexpr uint16_t DARK_PALETTE[16] = {
    0x0840,0x18C2,0x2944,0x0000,0xEE82,0xBDA4,0xF77A,0x8410,
    0x0840,0x3666,0xD145,0x39C7,0x1081,0xFF6C,0,0};
constexpr uint16_t LIGHT_PALETTE[16] = {
    0xEF7D,0xFFFF,0xDEFB,0xFFFF,0xDDA2,0xB4C2,0x18C2,0x6B4D,
    0x0840,0x2589,0xC904,0xCE59,0xE73C,0xFEE8,0,0};

enum class Page { Training, Settings, Debug, DebugZone };
enum class PacketView { Payload, Hex, Binary };
enum class Delivery { Example, Offline, Unsubscribed, Queued, Failed };
training::Session session;
training::Settings settings;
Page currentPage = Page::Training;
Page debugReturnPage = Page::Training;
PacketView packetView = PacketView::Payload;
Delivery delivery = Delivery::Example;
bool fullscreen = false;
bool darkMode = true;
bool phoneConnected = false;
bool bleAdvertisingActive = false;
bool touchWasDown = false;
bool dirty = true;
uint32_t rxCount = 0;
uint32_t connectionCount = 0;
char lastPacket[21] = "{\"hit\":1,\"score\":10}";
char lastRx[25] = "NONE";
uint32_t packetShot = 0;
uint32_t packetTotal = 0;
unsigned packetSensitivity = 1000;
char packetDistance[20] = "10.0 m";
training::Mode packetMode = training::Mode::Classic;
uint32_t testSequence = 0;
uint64_t shotPulseUntil = 0;
// A 240 ms eased drawer transition, advanced by the main loop without sleeping.
bool menuOpen = false;
bool menuAnimating = false;
int menuProgress = 0;
int menuFrom = 0;
int menuTo = 0;
uint64_t menuStarted = 0;

const char *modeName(training::Mode mode) {
  return mode == training::Mode::Freestyle ? "FREESTYLE" : "CLASSIC";
}

bool inside(uint16_t x, uint16_t y, int left, int top, int width, int height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

void button(int x, int y, int width, int height, const char *label,
            uint8_t background = SurfaceAlt, uint8_t foreground = Text,
            int scale = 1) {
  graphics::roundedRect(x, y, width, height, 8, background);
  graphics::centeredText(x, width, y + (height - 7 * scale) / 2,
                         label, foreground, scale);
}

void iconButton(int x, int y, int width, int height,
                const uint8_t (&icon)[72], const char *label = "",
                uint8_t background = SurfaceAlt, uint8_t foreground = Text) {
  graphics::roundedRect(x, y, width, height, 8, background);
  const int labelWidth = graphics::textWidth(label);
  const int contentWidth = 24 + (labelWidth ? 8 + labelWidth : 0);
  const int left = x + (width - contentWidth) / 2;
  graphics::bitmap24(left, y + (height - 24) / 2, icon, foreground);
  if (labelWidth) graphics::text(left + 32, y + (height - 7) / 2, label, foreground);
}

void header(const char *title, bool close = false) {
  graphics::fillRect(0, 0, 480, 52, Header);
  graphics::text(18, 18, title, Accent, 2);
  if (close) iconButton(412, 8, 52, 38, icon_close);
  else iconButton(396, 8, 68, 38, icon_menu);
}

void debugShotButton() {
  iconButton(338, 264, 126, 42, icon_debug_hit, "DEBUG +HIT", SurfaceAlt, Accent);
}

void scoreText(char *buffer, size_t length) {
  if (!session.hasShot) std::snprintf(buffer, length, "--");
  else std::snprintf(buffer, length, "%u", session.displayScore());
}

void drawFullscreen() {
  header(modeName(session.mode), true);
  char score[12];
  scoreText(score, sizeof(score));
  const int scale = std::strlen(score) > 2 ? 20 : 24;
  if (session.complete) {
    graphics::centeredText(0, 480, 60, "SESSION TOTAL / 100", Muted);
  }
  graphics::centeredText(0, 480, 80, score, Accent, scale);
  if (session.mode == training::Mode::Classic && !session.complete) {
    char left[4];
    std::snprintf(left, sizeof(left), "%u", session.remaining());
    graphics::text(18, 264, left, Text, 5);
    const int labelX = 18 + graphics::textWidth(left, 5) + 12;
    graphics::text(labelX, 275, "SHOTS", Muted);
    graphics::text(labelX, 288, "LEFT", Muted);
  } else if (session.complete) {
    graphics::text(18, 277, "NEXT SHOT STARTS", Muted);
    graphics::text(18, 292, "A NEW SESSION", Muted);
  }
  debugShotButton();
}

void drawTraining() {
  if (fullscreen) {
    drawFullscreen();
    return;
  }
  header(modeName(session.mode));
  graphics::roundedRect(16, 64, 138, 172, 10, Surface);
  graphics::text(28, 80, session.mode == training::Mode::Classic ?
                 "SHOTS LEFT" : "SHOT COUNT", Muted);
  char count[12];
  const unsigned shown = session.mode == training::Mode::Classic ?
      session.remaining() : session.shots;
  std::snprintf(count, sizeof(count), "%u", shown);
  const int countScale = std::min(6, 120 / std::max(1, graphics::textWidth(count)));
  graphics::centeredText(16, 138, 111, count, Text, std::max(1, countScale));
  char distance[16];
  settings.formatDistance(distance, sizeof(distance));
  graphics::text(28, 194, "DISTANCE", Muted);
  graphics::text(28, 212, distance, Text, 2);

  iconButton(326, 64, 138, 38, icon_fullscreen, "FULLSCREEN", Accent, OnAccent);
  graphics::roundedRect(170, 112, 294, 124, 10, Surface);
  graphics::text(186, 124, session.complete ? "SESSION TOTAL / 100" :
                 "LAST HIT SCORE", Muted);
  char score[12];
  scoreText(score, sizeof(score));
  graphics::centeredText(170, 294, 149, score, Accent, 10);
  if (session.complete) {
    graphics::text(180, 243, "NEXT SHOT STARTS A NEW SESSION", Muted);
  }
  iconButton(16, 264, 174, 42, icon_session_debug, "SESSION DEBUG");
  iconButton(200, 264, 126, 42, icon_reset, "RESET", SurfaceAlt, Danger);
  debugShotButton();
  graphics::fillCircle(184, 82, 4, phoneConnected ? Success : Muted);
  graphics::text(196, 79, phoneConnected ? "BLE LINKED" : "STANDALONE", Muted);
}

void drawSettings() {
  header("SETTINGS");
  graphics::roundedRect(16, 62, 448, 72, 10, Surface);
  graphics::bitmap24(28, 65, icon_distance, Muted);
  graphics::text(60, 73, "DISTANCE", Muted);
  char distance[16];
  settings.formatDistance(distance, sizeof(distance));
  graphics::text(28, 98, distance, Text, 2);
  iconButton(218, 78, 46, 44, icon_minus);
  iconButton(274, 78, 46, 44, icon_plus);
  iconButton(334, 78, 118, 44, icon_units,
         settings.unit == training::Unit::Meters ? "METERS" : "FEET", Accent, OnAccent);

  graphics::roundedRect(16, 144, 448, 80, 10, Surface);
  graphics::bitmap24(28, 147, icon_calibration, Muted);
  graphics::text(60, 155, "CALIBRATION SENSITIVITY", Muted);
  char sensitivity[8];
  std::snprintf(sensitivity, sizeof(sensitivity), "%u", settings.sensitivity);
  graphics::text(28, 177, sensitivity, Text, 3);
  iconButton(334, 168, 52, 44, icon_minus);
  iconButton(400, 168, 52, 44, icon_plus);
  graphics::text(28, 209, "HIGHER = LESS SENSITIVE", Muted);
  graphics::text(20, 233, "SENSOR BASELINE CALIBRATION NOT CONNECTED", Muted);

  iconButton(16, 254, 128, 52, darkMode ? icon_moon : icon_sun,
             darkMode ? "DARK" : "LIGHT");
  iconButton(156, 254, 166, 52, icon_bluetooth, phoneConnected ? "BLE LINKED" :
         (bleAdvertisingActive ? "BLE RESTART" : "START BLE"),
         SurfaceAlt, phoneConnected ? Success : Accent);
  iconButton(334, 254, 130, 52, icon_session_debug, "DEBUG ZONE", Accent, OnAccent);
}

#include "celebration.inc"

const char *deliveryLabel() {
  switch (delivery) {
    case Delivery::Example: return "EXAMPLE ONLY - NO SHOT YET";
    case Delivery::Offline: return "WOULD SEND - PHONE OFFLINE";
    case Delivery::Unsubscribed: return "NOT SENT - NO SUBSCRIPTION";
    case Delivery::Queued: return "QUEUED FOR BLE - NOT A PHONE ACK";
    case Delivery::Failed: return "BLE QUEUE FAILED - PREVIEW ONLY";
  }
  return "";
}

void drawDebug() {
  header("SESSION DEBUG", true);
  const char *tabNames[] = {"PAYLOAD", "HEX", "BINARY"};
  for (int i = 0; i < 3; ++i) {
    const bool active = static_cast<int>(packetView) == i;
    button(16 + i * 152, 62, 144, 34, tabNames[i],
           active ? Accent : SurfaceAlt, active ? OnAccent : Text);
  }
  graphics::roundedRect(16, 106, 448, 104, 10, Surface);
  const size_t size = std::strlen(lastPacket);
  if (packetView == PacketView::Payload) {
    char title[40];
    std::snprintf(title, sizeof(title), "FINAL BLE PAYLOAD / %u BYTES", static_cast<unsigned>(size));
    graphics::text(28, 119, title, Muted);
    graphics::text(28, 146, lastPacket, Text, 2);
    graphics::text(28, 182, "JSON TEXT ENCODED AS UTF-8 BYTES", Muted);
  } else if (packetView == PacketView::Hex) {
    graphics::text(28, 117, "SAME PAYLOAD / HEXADECIMAL BYTES", Muted);
    for (size_t row = 0; row < 2; ++row) {
      char line[31] = {};
      for (size_t i = 0; i < 10 && row * 10 + i < size; ++i) {
        std::snprintf(line + i * 3, sizeof(line) - i * 3, "%02X ",
                      static_cast<unsigned char>(lastPacket[row * 10 + i]));
      }
      graphics::text(28, 142 + static_cast<int>(row) * 26, line, Text, 2);
    }
  } else {
    for (size_t row = 0; row < 5; ++row) {
      char line[37] = {};
      size_t offset = 0;
      for (size_t i = 0; i < 4 && row * 4 + i < size; ++i) {
        const uint8_t byte = static_cast<uint8_t>(lastPacket[row * 4 + i]);
        for (int bit = 7; bit >= 0; --bit) line[offset++] = (byte & (1 << bit)) ? '1' : '0';
        line[offset++] = ' ';
      }
      graphics::text(30, 116 + static_cast<int>(row) * 18, line, Text);
    }
  }
  graphics::text(18, 220, deliveryLabel(), delivery == Delivery::Queued ? Success : Accent);
  char metadata[76];
  std::snprintf(metadata, sizeof(metadata), "%s SHOT %lu TOTAL %lu / %s / CAL %u",
      modeName(packetMode), static_cast<unsigned long>(packetShot),
      static_cast<unsigned long>(packetTotal), packetDistance, packetSensitivity);
  graphics::text(18, 239, metadata, Muted);
  char received[38];
  std::snprintf(received, sizeof(received), "RX: %s", lastRx);
  graphics::text(18, 274, received, Muted);
  graphics::text(18, 294, "PREVIEW IS AVAILABLE OFFLINE", Muted);
  debugShotButton();
}

void drawMenu() {
  const int x = 480 - 240 * menuProgress / 1000;
  graphics::fillRect(x - 5, 0, 5, 320, Shade);
  graphics::fillRect(x, 0, 240, 320, Surface);
  graphics::text(x + 18, 19, "TRAINING", Text, 2);
  iconButton(x + 182, 8, 42, 38, icon_close);
  const uint8_t (*icons[])[72] = {&icon_freestyle, &icon_classic,
      &icon_settings, &icon_session_debug};
  const char *labels[] = {"FREESTYLE", "CLASSIC", "SETTINGS", "SESSION DEBUG"};
  for (int i = 0; i < 4; ++i) {
    const bool active = (i == 0 && currentPage == Page::Training && session.mode == training::Mode::Freestyle) ||
        (i == 1 && currentPage == Page::Training && session.mode == training::Mode::Classic) ||
        (i == 2 && currentPage == Page::Settings) || (i == 3 && currentPage == Page::Debug);
    const int y = 56 + i * 51;
    const uint8_t foreground = active ? OnAccent : Text;
    graphics::roundedRect(x + 12, y, 216, 43, 8, active ? Accent : SurfaceAlt);
    graphics::bitmap24(x + 24, y + 9, *icons[i], foreground);
    graphics::text(x + 60, y + 18, labels[i], foreground);
  }
}

void renderScreen() {
  graphics::setPalette(darkMode ? DARK_PALETTE : LIGHT_PALETTE);
  graphics::clear(Background);
  if (currentPage == Page::Training) drawTraining();
  else if (currentPage == Page::Settings) drawSettings();
  else if (currentPage == Page::DebugZone) drawDebugZone();
  else drawDebug();
  if (currentPage == Page::Training && hardware::nowMs() < shotPulseUntil) {
    graphics::fillRect(0, 50, 480, 3, Highlight);
  }
  if (menuProgress > 0) drawMenu();
  graphics::present();
  dirty = false;
}

void animateMenu(bool open) {
  menuOpen = open;
  menuFrom = menuProgress;
  menuTo = open ? 1000 : 0;
  menuStarted = hardware::nowMs();
  menuAnimating = menuFrom != menuTo;
  dirty = true;
}

void advanceAnimation() {
  if (!menuAnimating) return;
  const uint64_t elapsed = hardware::nowMs() - menuStarted;
  const int64_t t = std::min<uint64_t>(elapsed, 240) * 1000 / 240;
  const int64_t remaining = 1000 - t;
  const int eased = 1000 - static_cast<int>(remaining * remaining * remaining / 1000000);
  menuProgress = menuFrom + (menuTo - menuFrom) * eased / 1000;
  if (elapsed >= 240) {
    menuProgress = menuTo;
    menuAnimating = false;
  }
  dirty = true;
}

void sendState() {
  char packet[21];
  std::snprintf(packet, sizeof(packet), "MODE:%s", modeName(session.mode));
  bluetooth::notify(packet);
  std::snprintf(packet, sizeof(packet), "SHOTS:%lu LEFT:%u",
      static_cast<unsigned long>(std::min<uint32_t>(session.shots, 999999)), session.remaining());
  bluetooth::notify(packet);
  std::snprintf(packet, sizeof(packet), "TOTAL:%lu LAST:%d",
      static_cast<unsigned long>(std::min<uint32_t>(session.total, 999999)), session.lastScore);
  bluetooth::notify(packet);
  char distance[16];
  settings.formatDistance(distance, sizeof(distance));
  std::snprintf(packet, sizeof(packet), "DIST:%s", distance);
  bluetooth::notify(packet);
  std::snprintf(packet, sizeof(packet), "CAL:%u", settings.sensitivity);
  bluetooth::notify(packet);
}

void setMode(training::Mode mode) {
  session.setMode(mode);
  currentPage = Page::Training;
  fullscreen = false;
  dirty = true;
  sendState();
}

void resetSession() {
  session.reset();
  testSequence = 0;
  dirty = true;
  puts("[SESSION] Session reset");
}

void recordTestShot(unsigned score) {
  session.recordShot(score);
  packetShot = session.shots;
  packetTotal = session.total;
  packetMode = session.mode;
  packetSensitivity = settings.sensitivity;
  settings.formatDistance(packetDistance, sizeof(packetDistance));
  // Preserve the app's existing <=20-byte shot packet and modulo-10 hit digit.
  std::snprintf(lastPacket, sizeof(lastPacket), "{\"hit\":%lu,\"score\":%d}",
      static_cast<unsigned long>(session.shots % 10), session.lastScore);
  const bool subscribed = bluetooth::notificationsEnabled();
  const bool queued = phoneConnected && bluetooth::notify(lastPacket);
  delivery = !phoneConnected ? Delivery::Offline : !subscribed ? Delivery::Unsubscribed :
             queued ? Delivery::Queued : Delivery::Failed;
  printf("[TEST SHOT] mode=%s shot=%lu last=%d total=%lu left=%u complete=%d\n",
      modeName(session.mode), static_cast<unsigned long>(session.shots), session.lastScore,
      static_cast<unsigned long>(session.total), session.remaining(), session.complete);
  printf("[BLE PREVIEW] %s / %s\n", lastPacket, deliveryLabel());
  shotPulseUntil = hardware::nowMs() + 180;
  dirty = true;
}

void simulateShot() {
  recordTestShot(10 - (testSequence++ % 4));
}

void openDebug() {
  debugReturnPage = currentPage;
  currentPage = Page::Debug;
  dirty = true;
}

void handleTouch(uint16_t x, uint16_t y) {
  printf("[TOUCH] %u, %u\n", x, y);
  if (menuAnimating) return;
  if (menuOpen) {
    if (x < 240 || inside(x, y, 422, 8, 42, 38)) animateMenu(false);
    else {
      for (int i = 0; i < 4; ++i) {
        if (!inside(x, y, 252, 56 + i * 51, 216, 43)) continue;
        if (i < 2) setMode(i == 0 ? training::Mode::Freestyle : training::Mode::Classic);
        else if (i == 2) { currentPage = Page::Settings; fullscreen = false; }
        else openDebug();
        animateMenu(false);
        break;
      }
    }
    return;
  }
  if (currentPage == Page::DebugZone) {
    if (inside(x, y, 412, 8, 52, 38)) {
      if (celebration != Celebration::None) celebration = Celebration::None;
      else currentPage = Page::Settings;
    } else if (celebration == Celebration::None) {
      for (int i = 0; i < 4; ++i) {
        if (inside(x, y, 16 + i % 2 * 232, 90 + i / 2 * 94, 216, 80))
          startCelebration(static_cast<Celebration>(i + 1));
      }
    }
    dirty = true;
    return;
  }
  if (currentPage == Page::Debug) {
    if (inside(x, y, 412, 8, 52, 38)) currentPage = debugReturnPage;
    else if (inside(x, y, 338, 264, 126, 42)) simulateShot();
    else for (int i = 0; i < 3; ++i) {
      if (inside(x, y, 16 + i * 152, 62, 144, 34)) packetView = static_cast<PacketView>(i);
    }
    dirty = true;
    return;
  }
  if (currentPage == Page::Training && fullscreen) {
    if (inside(x, y, 412, 8, 52, 38)) fullscreen = false;
    else if (inside(x, y, 338, 264, 126, 42)) simulateShot();
    dirty = true;
    return;
  }
  if (inside(x, y, 396, 8, 68, 38)) { animateMenu(true); return; }
  if (currentPage == Page::Training) {
    if (inside(x, y, 326, 64, 138, 38)) fullscreen = true;
    else if (inside(x, y, 16, 264, 174, 42)) openDebug();
    else if (inside(x, y, 200, 264, 126, 42)) resetSession();
    else if (inside(x, y, 338, 264, 126, 42)) simulateShot();
    dirty = true;
    return;
  }
  bool settingChanged = true;
  if (inside(x, y, 218, 78, 46, 44)) settings.adjustDistance(-1);
  else if (inside(x, y, 274, 78, 46, 44)) settings.adjustDistance(1);
  else if (inside(x, y, 334, 78, 118, 44)) settings.toggleUnit();
  else if (inside(x, y, 334, 168, 52, 44)) settings.adjustSensitivity(-1);
  else if (inside(x, y, 400, 168, 52, 44)) settings.adjustSensitivity(1);
  else if (inside(x, y, 16, 254, 128, 52)) darkMode = !darkMode;
  else if (inside(x, y, 156, 254, 166, 52) && !phoneConnected) bluetooth::restartAdvertising();
  else if (inside(x, y, 334, 254, 130, 52)) {
    currentPage = Page::DebugZone;
    celebration = Celebration::None;
    dirty = true;
    return;
  }
  else settingChanged = false;
  if (settingChanged) { sendState(); dirty = true; }
}

bool parseUnsigned(const char *text, unsigned maximum, unsigned &value) {
  if (*text < '0' || *text > '9') return false;
  char *end = nullptr;
  const unsigned long parsed = std::strtoul(text, &end, 10);
  if (*end != '\0' || parsed > maximum) return false;
  value = static_cast<unsigned>(parsed);
  return true;
}

void handleCommand(const char *command) {
  unsigned value = 0;
  if (std::strcmp(command, "PING") == 0) bluetooth::notify("{\"pong\":1}");
  else if (std::strcmp(command, "RESET") == 0) { resetSession(); sendState(); }
  else if (std::strcmp(command, "STATE") == 0) sendState();
  else if (std::strcmp(command, "MODE:FREESTYLE") == 0) setMode(training::Mode::Freestyle);
  else if (std::strcmp(command, "MODE:CLASSIC") == 0) setMode(training::Mode::Classic);
  else if (std::strcmp(command, "TEST") == 0) simulateShot();
  else if (std::strncmp(command, "TEST:", 5) == 0 && parseUnsigned(command + 5, 10, value)) recordTestShot(value);
  else if (std::strncmp(command, "CAL:", 4) == 0 && parseUnsigned(command + 4, 4095, value)) {
    settings.sensitivity = value;
    sendState();
  } else if (std::strncmp(command, "DIST:", 5) == 0) {
    char *end = nullptr;
    const double distance = std::strtod(command + 5, &end);
    const bool feet = std::strcmp(end, "FT") == 0;
    const bool meters = std::strcmp(end, "M") == 0;
    const double millimeters = distance * (feet ? 304.8 : 1000.0);
    if ((feet || meters) && millimeters >= 1000.0 && millimeters <= 100000.0) {
      settings.distanceMillimeters = static_cast<int>(millimeters + 0.5);
      settings.unit = feet ? training::Unit::Feet : training::Unit::Meters;
      sendState();
    } else bluetooth::notify("ERR:DIST RANGE");
  } else bluetooth::notify("ERR:COMMAND");
  dirty = true;
}

void processBleEvents() {
  bluetooth::Event event;
  for (unsigned i = 0; i < 8 && bluetooth::poll(event); ++i) {
    switch (event.type) {
      case bluetooth::EventType::Connected:
        phoneConnected = true;
        bleAdvertisingActive = false;
        ++connectionCount;
        break;
      case bluetooth::EventType::Disconnected:
        phoneConnected = false;
        bleAdvertisingActive = false;
        break;
      case bluetooth::EventType::Advertising:
        bleAdvertisingActive = true;
        break;
      case bluetooth::EventType::Received:
        ++rxCount;
        std::memcpy(lastRx, event.text, sizeof(lastRx));
        handleCommand(event.text);
        break;
    }
    dirty = true;
  }
}

bool readTouch(uint16_t &x, uint16_t &y) {
  uint8_t data[5] = {};
  if (!hardware::readTouchRegisters(0x02, data, sizeof(data)) || (data[0] & 0x0F) == 0) return false;
  const uint16_t portraitX = ((data[1] & 0x0F) << 8) | data[2];
  const uint16_t portraitY = ((data[3] & 0x0F) << 8) | data[4];
  if (portraitX >= 320 || portraitY >= 480) return false;
  x = portraitY;
  y = 319 - portraitX;
  return true;
}

#ifdef CONFIG_PRECISIONSHOT_SELF_TEST
#include "../tests/ui_selftest.inc"
#endif

extern "C" void app_main() {
  hardware::sleepMs(250);
  puts("\n=== PrecisionShot native ESP-IDF training UI boot ===");
  hardware::initialize();
  graphics::initialize();
  uint8_t chipId = 0;
  hardware::resetTouch();
  hardware::readTouchRegisters(0xA3, &chipId, 1);
  printf("[TOUCH] FT6336 chip ID: 0x%02X\n", chipId);
  bluetooth::initialize();
#ifdef CONFIG_PRECISIONSHOT_SELF_TEST
  runUiSelfTests();
#endif
  renderScreen();
  puts("[READY] Freestyle + Classic / Debug Zone animations");
  bool pulseVisible = false;
  for (;;) {
    processBleEvents();
    uint16_t x = 0, y = 0;
    const bool down = readTouch(x, y);
    if (down && !touchWasDown) handleTouch(x, y);
    touchWasDown = down;
    advanceAnimation();
    advanceCelebration(hardware::nowMs());
    const bool pulse = hardware::nowMs() < shotPulseUntil;
    if (pulse != pulseVisible) { pulseVisible = pulse; dirty = true; }
    if (dirty) renderScreen();
    hardware::sleepMs(10);
  }
}
