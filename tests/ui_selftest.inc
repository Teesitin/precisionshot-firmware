// Included in main.cpp only by the explicitly enabled device diagnostic build.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int runSessionTests();

namespace {
uint8_t testPixel(int x, int y) {
  const size_t index = static_cast<size_t>(y * graphics::WIDTH + x);
  const uint8_t packed = graphics::pixels()[index / 2];
  return index & 1 ? packed & 0x0F : packed >> 4;
}

void testRenderer() {
  graphics::clear(3);
  assert(graphics::pixels()[0] == 0x33);
  assert(graphics::pixels()[graphics::FRAMEBUFFER_BYTES - 1] == 0x33);
  graphics::fillRect(1, 0, 1, 1, 10);
  assert(graphics::pixels()[0] == 0x3A);
  graphics::fillRect(0, 0, 1, 1, 5);
  assert(graphics::pixels()[0] == 0x5A);
  graphics::fillRect(1, 1, 4, 1, 7);
  assert(testPixel(0, 1) == 3 && testPixel(1, 1) == 7);
  assert(testPixel(4, 1) == 7 && testPixel(5, 1) == 3);
  assert(testPixel(0, 2) == 3);
  graphics::fillRect(-2, -2, 3, 3, 9);
  assert(testPixel(0, 0) == 9 && testPixel(1, 0) == 10);
  graphics::fillRect(479, 319, 5, 5, 12);
  assert(testPixel(479, 319) == 12 && testPixel(478, 319) == 3);
  graphics::fillRect(0, 0, 0, 10, 2);
  graphics::fillRect(480, 320, 10, 10, 2);
  assert(testPixel(0, 0) == 9 && testPixel(479, 319) == 12);
  graphics::clear(0x1F);
  assert(graphics::pixels()[0] == 0xFF);
  graphics::clear(0);
  graphics::text(0, 0, "A", 6);
  assert(testPixel(0, 0) == 0 && testPixel(0, 1) == 6);
  assert(testPixel(1, 0) == 6 && testPixel(5, 0) == 0);
  graphics::text(6, 0, "a", 7);
  assert(testPixel(6, 0) == 0 && testPixel(6, 5) == 7);
  assert(graphics::textWidth("100", 20) == 340);
  assert(graphics::textWidth("", 2) == 0);
  // Transparent bitmap ink and offscreen clipping during drawer animation.
  graphics::clear(3);
  graphics::bitmap24(10, 10, icon_menu, 6);
  for (int y = 0; y < 24; ++y) for (int x = 0; x < 24; ++x) {
    const bool ink = icon_menu[y * 3 + x / 8] & (0x80 >> (x % 8));
    assert(testPixel(10 + x, 10 + y) == (ink ? 6 : 3));
  }
  graphics::clear(3);
  graphics::bitmap24(470, 310, icon_settings, 7);
  for (int y = 0; y < 10; ++y) for (int x = 0; x < 10; ++x) {
    const bool ink = icon_settings[y * 3 + x / 8] & (0x80 >> (x % 8));
    assert(testPixel(470 + x, 310 + y) == (ink ? 7 : 3));
  }
  assert(testPixel(0, 0) == 3 && testPixel(469, 310) == 3);
  graphics::setPalette(DARK_PALETTE);
  for (unsigned i = 0; i < 16; ++i) assert(graphics::palette()[i] == DARK_PALETTE[i]);
  puts("[SELFTEST] Renderer nibble, clipping, glyph and palette checks passed");
}

void captureTestFrame(const char *name) {
  // Suppress the transient shot flash so reference captures are deterministic.
  shotPulseUntil = 0;
  const uint64_t renderStarted = hardware::nowMs();
  renderScreen();
  const uint64_t renderMilliseconds = hardware::nowMs() - renderStarted;
  printf("[FRAME_MS:%s]%llu\n", name, static_cast<unsigned long long>(renderMilliseconds));
  assert(renderMilliseconds <= 500);
  printf("[FRAME:%s]\n[SIZE]480 320\n[PALETTE]", name);
  for (unsigned i = 0; i < 16; ++i) printf("%04X", graphics::palette()[i]);
  putchar('\n');
  constexpr char hex[] = "0123456789ABCDEF";
  constexpr size_t bytesPerLine = 256;
  char line[sizeof("[PIXELS]") - 1 + bytesPerLine * 2 + 1];
  std::memcpy(line, "[PIXELS]", 8);
  for (size_t offset = 0; offset < graphics::FRAMEBUFFER_BYTES; offset += bytesPerLine) {
    const size_t count = std::min(bytesPerLine, graphics::FRAMEBUFFER_BYTES - offset);
    for (size_t i = 0; i < count; ++i) {
      const uint8_t byte = graphics::pixels()[offset + i];
      line[8 + i * 2] = hex[byte >> 4];
      line[9 + i * 2] = hex[byte & 0x0F];
    }
    line[8 + count * 2] = '\0';
    puts(line);
    if ((offset / bytesPerLine + 1) % 16 == 0) hardware::sleepMs(1);
  }
  puts("[END_FRAME]");
  fflush(stdout);
}

void finishTestAnimation() {
  hardware::sleepMs(250);
  advanceAnimation();
  assert(!menuAnimating);
  assert(menuProgress == (menuOpen ? 1000 : 0));
}

void resetUiTestState() {
  session = training::Session{};
  settings = training::Settings{};
  currentPage = Page::Training;
  debugReturnPage = Page::Training;
  packetView = PacketView::Payload;
  delivery = Delivery::Example;
  fullscreen = false;
  darkMode = true;
  phoneConnected = false;
  bleAdvertisingActive = false;
  touchWasDown = false;
  dirty = true;
  rxCount = 0;
  connectionCount = 0;
  std::snprintf(lastPacket, sizeof(lastPacket), "{\"hit\":1,\"score\":10}");
  std::snprintf(lastRx, sizeof(lastRx), "NONE");
  packetShot = 0;
  packetTotal = 0;
  packetSensitivity = 1000;
  settings.formatDistance(packetDistance, sizeof(packetDistance));
  packetMode = training::Mode::Classic;
  testSequence = 0;
  shotPulseUntil = 0;
  menuOpen = false;
  menuAnimating = false;
  menuProgress = 0;
  menuFrom = 0;
  menuTo = 0;
  menuStarted = 0;
}
}  // namespace

void runUiSelfTests() {
  puts("[SELFTEST:BEGIN]");
  assert(runSessionTests() == 0);
  testRenderer();
  resetUiTestState();

  // Exercise the same hit areas as physical touch, including the full-screen
  // score test control and close action.
  handleTouch(385, 283);
  assert(session.shots == 1 && session.lastScore == 10);
  assert(delivery == Delivery::Offline);
  captureTestFrame("classic_normal");
  handleTouch(390, 82);
  assert(fullscreen && session.shots == 1);
  handleTouch(385, 283);
  assert(fullscreen && session.shots == 2 && session.lastScore == 9);
  captureTestFrame("classic_fullscreen");
  handleTouch(438, 26);
  assert(!fullscreen && session.shots == 2 && session.lastScore == 9);
  handleTouch(263, 283);
  assert(session.shots == 0 && !session.hasShot);
  handleTouch(390, 82);
  assert(fullscreen);
  for (unsigned i = 0; i < 9; ++i) recordTestShot(10);
  assert(fullscreen && session.shots == 9 && session.remaining() == 1);
  assert(!session.complete && session.displayScore() == 10);
  recordTestShot(10);
  assert(fullscreen && session.complete && session.displayScore() == 100);
  assert(session.remaining() == 0 && packetShot == 10 && packetTotal == 100);
  assert(std::strcmp(lastPacket, "{\"hit\":0,\"score\":10}") == 0);
  captureTestFrame("classic_complete");
  recordTestShot(7);
  assert(fullscreen && !session.complete && session.shots == 1);
  assert(session.total == 7 && session.displayScore() == 7 && session.remaining() == 9);
  handleTouch(438, 26);
  assert(!fullscreen && session.lastScore == 7);

  handleTouch(429, 26);
  assert(menuOpen && menuAnimating);
  hardware::sleepMs(60);
  advanceAnimation();
  assert(menuProgress > 0 && menuProgress < 1000 && menuAnimating);
  captureTestFrame("menu_mid");
  finishTestAnimation();
  captureTestFrame("menu_open");
  handleTouch(350, 281);  // Removed fifth menu row has no action.
  assert(menuOpen && !menuAnimating && currentPage == Page::Training);
  assert(session.mode == training::Mode::Classic && session.shots == 1);
  handleTouch(350, 77);
  finishTestAnimation();
  assert(!menuOpen && session.mode == training::Mode::Freestyle);
  assert(!session.hasShot && !fullscreen);
  for (unsigned i = 0; i < 25; ++i) recordTestShot(8);
  assert(session.shots == 25 && session.total == 200 && !session.complete);
  handleTouch(390, 82);
  assert(fullscreen);
  handleTouch(385, 283);
  assert(fullscreen && session.shots == 26 && !session.complete);
  assert(session.lastScore == 10 && session.displayScore() == 10);
  captureTestFrame("freestyle_fullscreen");
  handleTouch(438, 26);
  assert(!fullscreen && session.shots == 26);

  handleTouch(429, 26);
  finishTestAnimation();
  handleTouch(350, 179);
  finishTestAnimation();
  assert(currentPage == Page::Settings && !fullscreen && darkMode);
  handleTouch(297, 99);
  assert(settings.distanceMillimeters == 11000);
  handleTouch(240, 99);
  assert(settings.distanceMillimeters == 10000);
  handleTouch(391, 99);
  assert(settings.unit == training::Unit::Feet && settings.distanceMillimeters == 10000);
  handleTouch(297, 99);
  assert(settings.distanceMillimeters == 10305);
  handleTouch(240, 99);
  assert(settings.distanceMillimeters == 10000);
  handleTouch(426, 190);
  assert(settings.sensitivity == 1100);
  handleTouch(360, 190);
  assert(settings.sensitivity == 1000);
  captureTestFrame("settings_dark");
  handleTouch(110, 279);
  assert(!darkMode);
  captureTestFrame("settings_light");
  handleTouch(110, 279);
  handleTouch(391, 99);
  assert(darkMode && settings.unit == training::Unit::Meters);

  handleTouch(429, 26);
  finishTestAnimation();
  handleTouch(350, 230);
  finishTestAnimation();
  assert(currentPage == Page::Debug && debugReturnPage == Page::Settings);
  handleTouch(385, 283);
  assert(session.shots == 27 && session.lastScore == 9);
  assert(packetShot == 27 && packetTotal == session.total);
  assert(std::strcmp(lastPacket, "{\"hit\":7,\"score\":9}") == 0);
  assert(delivery == Delivery::Offline);
  handleTouch(80, 78);
  assert(packetView == PacketView::Payload);
  captureTestFrame("debug_payload");
  handleTouch(235, 78);
  assert(packetView == PacketView::Hex);
  captureTestFrame("debug_hex");
  handleTouch(385, 78);
  assert(packetView == PacketView::Binary);
  captureTestFrame("debug_binary");
  handleTouch(438, 26);
  assert(currentPage == Page::Settings && session.shots == 27);

  handleTouch(390, 279);
  assert(currentPage == Page::DebugZone && celebration == Celebration::None);
  captureTestFrame("debug_zone");
  const char *effects[] = {"confetti", "orbit", "bounce", "warp"};
  const uint32_t preservedShots = session.shots;
  const uint32_t preservedTotal = session.total;
  for (int i = 0; i < 4; ++i) {
    handleTouch(100 + i % 2 * 232, 120 + i / 2 * 94);
    assert(celebration == static_cast<Celebration>(i + 1));
    advanceCelebration(celebrationStarted + 4999);
    assert(celebration != Celebration::None);
    bool captured = false;
    while (celebration != Celebration::None) {
      advanceCelebration(hardware::nowMs());
      if (!captured && hardware::nowMs() - celebrationStarted >= 2000) {
        captureTestFrame(effects[i]);
        captured = true;
      }
      if (dirty) renderScreen();
      hardware::sleepMs(10);
    }
    assert(captured && hardware::nowMs() - celebrationStarted < 5200);
    assert(currentPage == Page::DebugZone);
    assert(session.shots == preservedShots && session.total == preservedTotal);
  }
  handleTouch(100, 120);
  assert(celebration == Celebration::Confetti);
  handleTouch(438, 26);
  assert(celebration == Celebration::None && currentPage == Page::DebugZone);
  handleTouch(438, 26);
  assert(currentPage == Page::Settings);
  puts("[SELFTEST] Four five-second animations, early close and session preservation passed");

  resetUiTestState();
  assert(session.mode == training::Mode::Classic && session.remaining() == 10);
  assert(currentPage == Page::Training && !fullscreen && darkMode);
  puts("[SELFTEST:PASS]");
  fflush(stdout);
}
