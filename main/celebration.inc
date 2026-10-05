// Debug animations and tunes. The main loop keeps running during them.
enum class Celebration { None, Confetti, Orbit, Bounce, Warp };
Celebration celebration = Celebration::None;
uint64_t celebrationStarted = 0;
uint64_t celebrationFrame = 0;
constexpr unsigned CELEBRATION_MS = 5000;
unsigned celebrationDuration = CELEBRATION_MS;

void startCelebration(Celebration effect) {
    // Replace the test beep and give longer tunes time to finish.
    hardware::stopSpeakerSequence();
    celebrationDuration = CELEBRATION_MS;
    switch (effect) {
        case Celebration::Confetti:
            music::playEntertainer();
            break;
        case Celebration::Orbit:
            music::playGreensleeves();
            celebrationDuration = 6000;
            break;
        case Celebration::Bounce:
            music::playFurElise();
            break;
        case Celebration::Warp:
            music::playMountainKing();
            celebrationDuration = 6957;
            break;
        case Celebration::None:
            break;
    }
    celebration = effect;
    celebrationStarted = hardware::nowMs();
    celebrationFrame = 0;
    dirty = true;
}

void advanceCelebration(uint64_t now) {
    // Stop sound too when the animation ends or we leave this page.
    if (celebration == Celebration::None)
        return;
    if (currentPage != Page::DebugZone || now - celebrationStarted >= celebrationDuration) {
        hardware::stopSpeakerSequence();
        celebration = Celebration::None;
        dirty = true;
    } else if (now - celebrationFrame >= 33) {
        celebrationFrame = now;
        dirty = true;
    }
}

void drawDebugZone() {
    // Show the buttons when no animation is playing.
    if (celebration == Celebration::None) {
        header("DEBUG ZONE", true);
        const int temperatures[] = {sensors::temperatureTenths(sensors::regulator33),
            sensors::temperatureTenths(sensors::regulator5)};
        const char *labels[] = {"3.3V REGULATOR", "5V REGULATOR"};
        char value[48];
        for (int i = 0; i < 2; ++i) {
            const int x = 16 + i * 232;
            graphics::roundedRect(x, 58, 216, 48, 8, Surface);
            graphics::text(x + 12, 66, labels[i], Muted);
            if (temperatures[i] == -9999)
                std::snprintf(value, sizeof(value), "-- C");
            else
                std::snprintf(value, sizeof(value), "%.1f C", temperatures[i] / 10.0);
            graphics::text(x + 12, 83, value, Text, 2);
        }
        graphics::roundedRect(16, 112, 448, 60, 8, Surface);
        graphics::text(28, 120, "SENSOR INPUT", Muted);
        if (sensors::input.millivolts < 0)
            std::snprintf(value, sizeof(value), "-- V");
        else
            std::snprintf(value, sizeof(value), "%.3f V", sensors::input.millivolts / 1000.0);
        graphics::text(28, 137, value, Text, 2);
        const bool detected = sensors::input.raw >= static_cast<int>(settings.sensitivity);
        graphics::text(290, 120,
            sensors::input.raw < 0 ? "UNAVAILABLE"
                : detected         ? "DETECTED"
                                   : "BELOW THRESHOLD",
            detected ? Success : Muted);
        std::snprintf(
            value, sizeof(value), "ADC %d / LIMIT %u", sensors::input.raw, settings.sensitivity);
        graphics::text(180, 147, value, Muted);
        const char *names[] = {"CONFETTI / ENTERTAINER", "ORBIT / GREENSLEEVES",
            "BOUNCE / FUR ELISE", "WARP / MOUNTAIN KING"};
        for (int i = 0; i < 4; ++i)
            button(16 + (i % 2) * 232, 180 + (i / 2) * 42, 216, 34, names[i]);
        button(16, 264, 216, 42, "SESSION DEBUG");
        button(248, 264, 216, 42, "STOP SOUND", SurfaceAlt, Text);
        return;
    }
    const unsigned elapsed = static_cast<unsigned>(
        std::min<uint64_t>(hardware::nowMs() - celebrationStarted, celebrationDuration));
    const float t = elapsed / 1000.0f;
    const uint8_t colors[] = {Accent, Success, Danger, Highlight, Text};
    int scoreX = 240;
    int scoreY = 138;
    int scale = 12;
    if (celebration == Celebration::Confetti) {
        // Falling colored pieces.
        for (int i = 0; i < 65; ++i) {
            const int x = (i * 97 + static_cast<int>(std::sin(t * 3 + i) * 24) + 480) % 480;
            const int y = (i * 43 + elapsed * (45 + i % 60) / 1000) % 350 - 20;
            graphics::fillRect(x, y, 4 + i % 5, 5 + i % 9, colors[i % 5]);
        }
        scale = 10 + static_cast<int>(2 * (1 + std::sin(t * 5)));
    } else if (celebration == Celebration::Orbit) {
        // Dots move around the score.
        for (int i = 0; i < 28; ++i) {
            const float a = t * 1.9f + i * 0.2244f;
            graphics::fillCircle(240 + static_cast<int>(205 * std::cos(a)),
                166 + static_cast<int>(100 * std::sin(a)), 3 + i % 5, colors[i % 5]);
        }
        scoreX += static_cast<int>(20 * std::sin(t * 3));
        scoreY += static_cast<int>(14 * std::cos(t * 3));
    } else if (celebration == Celebration::Bounce) {
        // Bounce the score and squash it near the bottom.
        const float bounce = std::abs(std::sin(t * 3.5f));
        scoreY = 185 - static_cast<int>(bounce * 85);
        scale = bounce < 0.2f ? 10 : 12;
        graphics::roundedRect(120 + static_cast<int>(bounce * 30), 258,
            240 - static_cast<int>(bounce * 60), 10, 5, SurfaceAlt);
        for (int i = 0; i < 8; ++i)
            graphics::fillCircle(30 + i * 60,
                242 - static_cast<int>(20 * std::abs(std::sin(t * 4 + i))), 5, colors[i % 5]);
    } else {
        // Move stars away from the center.
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
    graphics::roundedRect(
        scoreX - scoreWidth / 2 - 12, scoreY - 12, scoreWidth + 24, scale * 7 + 24, 12, Background);
    graphics::text(scoreX - scoreWidth / 2 + 4, scoreY + 4, "XXX", SurfaceAlt, scale);
    graphics::text(scoreX - scoreWidth / 2, scoreY, "XXX", Accent, scale);
    graphics::fillRect(0, 0, 480, 52, Header);
    graphics::text(18, 18, "HIGHSCORE", Accent, 3);
    iconButton(412, 8, 52, 38, icon_close);
    graphics::fillRect(0, 310, 480, 10, SurfaceAlt);
    graphics::fillRect(
        0, 310, 480 * (celebrationDuration - elapsed) / celebrationDuration, 10, Accent);
}
