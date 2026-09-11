// Project-owned animation playground; all timing is nonblocking.
enum class Celebration { None, Confetti, Orbit, Bounce, Warp };
Celebration celebration = Celebration::None;
uint64_t celebrationStarted = 0;
uint64_t celebrationFrame = 0;
constexpr unsigned CELEBRATION_MS = 5000;

void startCelebration(Celebration effect) {
  celebration = effect;
  celebrationStarted = hardware::nowMs();
  celebrationFrame = 0;
  dirty = true;
}

void advanceCelebration(uint64_t now) {
  if (celebration == Celebration::None) return;
  if (currentPage != Page::DebugZone || now - celebrationStarted >= CELEBRATION_MS) {
    celebration = Celebration::None;
    dirty = true;
  } else if (now - celebrationFrame >= 33) {
    celebrationFrame = now;
    dirty = true;
  }
}

void drawDebugZone() {
  if (celebration == Celebration::None) {
    header("DEBUG ZONE", true);
    graphics::centeredText(0, 480, 66, "HIGHSCORE XXX / ANIMATION PLAYGROUND", Muted);
    const char *names[] = {"CONFETTI CHAOS", "COSMIC ORBIT", "JELLY BOUNCE", "WARP SPEED"};
    const char *details[] = {"COLOR RAIN + POP", "SPINNING SCORE", "BOING! BOING!", "FLY THROUGH STARS"};
    for (int i = 0; i < 4; ++i) {
      const int x = 16 + (i % 2) * 232;
      const int y = 90 + (i / 2) * 94;
      button(x, y, 216, 80, "", SurfaceAlt);
      graphics::centeredText(x, 216, y + 20, names[i], Accent, 2);
      graphics::centeredText(x, 216, y + 55, details[i], Muted);
    }
    graphics::centeredText(0, 480, 288, "TAP TO PLAY / STOPS AFTER 5 SECONDS", Muted);
    return;
  }
  const unsigned elapsed = static_cast<unsigned>(std::min<uint64_t>(
      hardware::nowMs() - celebrationStarted, CELEBRATION_MS));
  const float t = elapsed / 1000.0f;
  const uint8_t colors[] = {Accent, Success, Danger, Highlight, Text};
  int scoreX = 240;
  int scoreY = 138;
  int scale = 12;
  if (celebration == Celebration::Confetti) {
    for (int i = 0; i < 65; ++i) {
      const int x = (i * 97 + static_cast<int>(std::sin(t * 3 + i) * 24) + 480) % 480;
      const int y = (i * 43 + elapsed * (45 + i % 60) / 1000) % 350 - 20;
      graphics::fillRect(x, y, 4 + i % 5, 5 + i % 9, colors[i % 5]);
    }
    scale = 10 + static_cast<int>(2 * (1 + std::sin(t * 5)));
  } else if (celebration == Celebration::Orbit) {
    for (int i = 0; i < 28; ++i) {
      const float a = t * 1.9f + i * 0.2244f;
      graphics::fillCircle(240 + static_cast<int>(205 * std::cos(a)),
          166 + static_cast<int>(100 * std::sin(a)), 3 + i % 5, colors[i % 5]);
    }
    scoreX += static_cast<int>(20 * std::sin(t * 3));
    scoreY += static_cast<int>(14 * std::cos(t * 3));
  } else if (celebration == Celebration::Bounce) {
    const float bounce = std::abs(std::sin(t * 3.5f));
    scoreY = 185 - static_cast<int>(bounce * 85);
    scale = bounce < 0.2f ? 10 : 12;
    graphics::roundedRect(120 + static_cast<int>(bounce * 30), 258,
        240 - static_cast<int>(bounce * 60), 10, 5, SurfaceAlt);
    for (int i = 0; i < 8; ++i)
      graphics::fillCircle(30 + i * 60, 242 - static_cast<int>(20 * std::abs(std::sin(t * 4 + i))),
          5, colors[i % 5]);
  } else {
    for (int i = 0; i < 60; ++i) {
      const float depth = ((elapsed + i * 83) % 1500) / 1500.0f;
      const float a = i * 2.39996f;
      const int x = 240 + static_cast<int>(std::cos(a) * depth * depth * 340);
      const int y = 165 + static_cast<int>(std::sin(a) * depth * depth * 225);
      graphics::fillCircle(x, y, 1 + static_cast<int>(depth * 3), colors[i % 5]);
    }
    scale = std::min(14, 3 + static_cast<int>(elapsed / 90));
  }
  const int scoreWidth = graphics::textWidth("XXX", scale);
  graphics::roundedRect(scoreX - scoreWidth / 2 - 12, scoreY - 12,
      scoreWidth + 24, scale * 7 + 24, 12, Background);
  graphics::text(scoreX - scoreWidth / 2 + 4, scoreY + 4, "XXX", SurfaceAlt, scale);
  graphics::text(scoreX - scoreWidth / 2, scoreY, "XXX", Accent, scale);
  graphics::fillRect(0, 0, 480, 52, Header);
  graphics::text(18, 18, "HIGHSCORE", Accent, 3);
  iconButton(412, 8, 52, 38, icon_close);
  graphics::fillRect(0, 310, 480, 10, SurfaceAlt);
  graphics::fillRect(0, 310, 480 * (CELEBRATION_MS - elapsed) / CELEBRATION_MS, 10, Accent);
}
