#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// LCDWiki MSP4031 Example_01_Simple_test, ported from ESP32-WROOM-32E
// hardware HSPI to the ESP32-S3 hardware FSPI bus and the user's wiring.
constexpr int PIN_SD_CS = 4;
constexpr int PIN_MISO = 9;       // SDO(MISO)
constexpr int PIN_BACKLIGHT = 10; // LED
constexpr int PIN_SCLK = 11;      // SCK
constexpr int PIN_MOSI = 12;      // SDI(MOSI)
constexpr int PIN_DC = 13;        // LCD_RS
constexpr int PIN_RESET = 14;     // LCD_RST
constexpr int PIN_LCD_CS = 3;     // LCD_CS (listed as lcd_cd by the user)
constexpr int PIN_TOUCH_INT = 5;  // CTP_INT
constexpr int PIN_TOUCH_SDA = 6;  // CTP_SDA
constexpr int PIN_TOUCH_RESET = 7; // CTP_RST
constexpr int PIN_TOUCH_SCL = 8;  // CTP_SCL

constexpr uint16_t LCD_WIDTH = 480;
constexpr uint16_t LCD_HEIGHT = 320;
constexpr uint32_t SPI_FREQUENCY = 80000000;

constexpr uint16_t RED = 0xF800;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t BLUE = 0x001F;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t BLACK = 0x0000;
// PrecisionShot brand colors converted from CSS hex to RGB565.
constexpr uint16_t BACKGROUND = 0x0840;   // near black
constexpr uint16_t PANEL = 0x18C2;        // raised dark surface
constexpr uint16_t HEADER = 0x0000;
constexpr uint16_t ACCENT = 0xEE82;       // #ecd316
constexpr uint16_t ACCENT_DARK = 0x7B47;  // #7f683f
constexpr uint16_t MUTED = 0x41A4;
constexpr uint16_t TEXT_DARK = 0x0840;
constexpr uint16_t TEXT_LIGHT = 0xF77A;
constexpr uint16_t TARGET_RED = 0x7B47;
constexpr uint16_t TARGET_GOLD = 0xEE82;

constexpr uint8_t TOUCH_ADDRESS = 0x38;
constexpr int TRAIN_X = 326;
constexpr int TRAIN_Y = 76;
constexpr int TRAIN_W = 62;
constexpr int TRAIN_H = 44;
constexpr int MATCH_X = 396;
constexpr int MATCH_Y = 76;
constexpr int MATCH_W = 62;
constexpr int MATCH_H = 44;
constexpr int MINUS_X = 320;
constexpr int MINUS_Y = 158;
constexpr int MINUS_W = 38;
constexpr int MINUS_H = 44;
constexpr int PLUS_X = 426;
constexpr int PLUS_Y = 158;
constexpr int PLUS_W = 38;
constexpr int PLUS_H = 44;
constexpr int SHOT_X = 16;
constexpr int SHOT_Y = 232;
constexpr int SHOT_W = 448;
constexpr int SHOT_H = 72;

bool matchMode = false;
int distanceYards = 25;
int shotScore = 0;
int impactX = 0;
int impactY = 0;
bool hasShot = false;
bool touchWasDown = false;

SPIClass lcdSpi(FSPI);
const SPISettings lcdSettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0);

void startWrite(bool dataMode) {
  lcdSpi.beginTransaction(lcdSettings);
  digitalWrite(PIN_DC, dataMode ? HIGH : LOW);
  digitalWrite(PIN_LCD_CS, LOW);
}

void endWrite() {
  digitalWrite(PIN_LCD_CS, HIGH);
  lcdSpi.endTransaction();
}

void writeCommand(uint8_t command) {
  startWrite(false);
  lcdSpi.write(command);
  endWrite();
}

void writeData8(uint8_t data) {
  startWrite(true);
  lcdSpi.write(data);
  endWrite();
}

void writeData16(uint16_t data) {
  const uint8_t bytes[] = {
      static_cast<uint8_t>(data >> 8),
      static_cast<uint8_t>(data),
  };

  startWrite(true);
  lcdSpi.writeBytes(bytes, sizeof(bytes));
  endWrite();
}

void resetDisplay() {
  digitalWrite(PIN_RESET, LOW);
  delay(20);
  digitalWrite(PIN_RESET, HIGH);
  delay(20);
}

void initializeDisplay() {
  resetDisplay();

  // Exact LCDWiki initialization for the 4.0-inch ST7796S TN module.
  writeCommand(0xF0);
  writeData8(0xC3);
  writeCommand(0xF0);
  writeData8(0x96);
  writeCommand(0x36);
  writeData8(0x28); // LCDWiki landscape orientation: 480x320.
  writeCommand(0x3A);
  writeData8(0x05);
  writeCommand(0xB0);
  writeData8(0x80);
  writeCommand(0xB6);
  writeData8(0x00);
  writeData8(0x02);
  writeCommand(0xB5);
  writeData8(0x02);
  writeData8(0x03);
  writeData8(0x00);
  writeData8(0x04);
  writeCommand(0xB1);
  writeData8(0x80);
  writeData8(0x10);
  writeCommand(0xB4);
  writeData8(0x00);
  writeCommand(0xB7);
  writeData8(0xC6);
  writeCommand(0xC5);
  writeData8(0x1C);
  writeCommand(0xE4);
  writeData8(0x31);
  writeCommand(0xE8);
  writeData8(0x40);
  writeData8(0x8A);
  writeData8(0x00);
  writeData8(0x00);
  writeData8(0x29);
  writeData8(0x19);
  writeData8(0xA5);
  writeData8(0x33);
  writeCommand(0xC2);
  writeCommand(0xA7);

  writeCommand(0xE0);
  writeData8(0xF0);
  writeData8(0x09);
  writeData8(0x13);
  writeData8(0x12);
  writeData8(0x12);
  writeData8(0x2B);
  writeData8(0x3C);
  writeData8(0x44);
  writeData8(0x4B);
  writeData8(0x1B);
  writeData8(0x18);
  writeData8(0x17);
  writeData8(0x1D);
  writeData8(0x21);

  writeCommand(0xE1);
  writeData8(0xF0);
  writeData8(0x09);
  writeData8(0x13);
  writeData8(0x0C);
  writeData8(0x0D);
  writeData8(0x27);
  writeData8(0x3B);
  writeData8(0x44);
  writeData8(0x4D);
  writeData8(0x0B);
  writeData8(0x17);
  writeData8(0x17);
  writeData8(0x1D);
  writeData8(0x21);

  writeCommand(0xF0);
  writeData8(0x3C);
  writeCommand(0xF0);
  writeData8(0x69);
  writeCommand(0x13);
  writeCommand(0x11);
  writeCommand(0x29);
}

void setWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd,
               uint16_t yEnd) {
  writeCommand(0x2A);
  writeData16(xStart);
  writeData16(xEnd);
  writeCommand(0x2B);
  writeData16(yStart);
  writeData16(yEnd);
  writeCommand(0x2C);
}

void fillScreen(uint16_t color) {
  setWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

  const uint8_t pixel[] = {
      static_cast<uint8_t>(color >> 8),
      static_cast<uint8_t>(color),
  };

  startWrite(true);
  lcdSpi.writePattern(pixel, sizeof(pixel), LCD_WIDTH * LCD_HEIGHT);
  endWrite();
}

void fillRect(int x, int y, int width, int height, uint16_t color) {
  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > LCD_WIDTH || y + height > LCD_HEIGHT) {
    return;
  }

  setWindow(x, y, x + width - 1, y + height - 1);
  const uint8_t pixel[] = {
      static_cast<uint8_t>(color >> 8),
      static_cast<uint8_t>(color),
  };

  startWrite(true);
  lcdSpi.writePattern(pixel, sizeof(pixel), width * height);
  endWrite();
}

const uint8_t *glyphFor(char character) {
  static const uint8_t space[5] = {0, 0, 0, 0, 0};
  static const uint8_t plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static const uint8_t minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t digits[10][5] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
      {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
      {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
      {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
  };
  static const uint8_t letters[26][5] = {
      {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
      {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
      {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
      {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
      {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
      {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
      {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
      {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
      {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
      {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
      {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
      {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
      {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
  };

  if (character >= 'a' && character <= 'z') {
    character -= 32;
  }
  if (character >= 'A' && character <= 'Z') {
    return letters[character - 'A'];
  }
  if (character >= '0' && character <= '9') {
    return digits[character - '0'];
  }
  if (character == '+') {
    return plus;
  }
  if (character == '-') {
    return minus;
  }
  return space;
}

void drawText(int x, int y, const char *text, uint16_t color, int scale = 1) {
  while (*text) {
    const uint8_t *glyph = glyphFor(*text++);
    for (int column = 0; column < 5; ++column) {
      for (int row = 0; row < 7; ++row) {
        if (glyph[column] & (1 << row)) {
          fillRect(x + column * scale, y + row * scale, scale, scale, color);
        }
      }
    }
    x += 6 * scale;
  }
}

void drawSevenSegmentDigit(int x, int y, uint8_t digit) {
  constexpr int width = 30;
  constexpr int height = 54;
  constexpr int thickness = 6;
  constexpr uint8_t masks[] = {
      0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
      0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111,
  };
  const uint8_t mask = masks[digit];

  auto segment = [&](uint8_t bit, int sx, int sy, int sw, int sh) {
    fillRect(sx, sy, sw, sh, (mask & (1 << bit)) ? ACCENT : MUTED);
  };

  segment(0, x + thickness, y, width - 2 * thickness, thickness);
  segment(1, x + width - thickness, y + thickness, thickness,
          height / 2 - thickness);
  segment(2, x + width - thickness, y + height / 2, thickness,
          height / 2 - thickness);
  segment(3, x + thickness, y + height - thickness,
          width - 2 * thickness, thickness);
  segment(4, x, y + height / 2, thickness, height / 2 - thickness);
  segment(5, x, y + thickness, thickness, height / 2 - thickness);
  segment(6, x + thickness, y + height / 2 - thickness / 2,
          width - 2 * thickness, thickness);
}

void drawTarget() {
  constexpr int x = 150;
  constexpr int y = 92;
  fillRect(x, y, 72, 72, TEXT_DARK);
  fillRect(x + 5, y + 5, 62, 62, WHITE);
  fillRect(x + 13, y + 13, 46, 46, TARGET_RED);
  fillRect(x + 22, y + 22, 28, 28, TARGET_GOLD);
  fillRect(x + 32, y + 5, 2, 62, TEXT_DARK);
  fillRect(x + 5, y + 32, 62, 2, TEXT_DARK);

  if (hasShot) {
    const int dotX = x + 34 + impactX * 29 / 100;
    const int dotY = y + 34 + impactY * 29 / 100;
    fillRect(dotX - 4, dotY - 4, 8, 8, BLACK);
  }
}

void drawScoreCard() {
  fillRect(16, 64, 220, 152, PANEL);
  drawText(28, 78, "SCORE", ACCENT, 1);

  char value[4];
  snprintf(value, sizeof(value), "%d", shotScore);
  const int digitCount = strlen(value);
  constexpr int digitWidth = 30;
  constexpr int spacing = 5;
  const int totalWidth = digitCount * digitWidth + (digitCount - 1) * spacing;
  int x = 28 + (106 - totalWidth) / 2;

  for (int i = 0; i < digitCount; ++i) {
    drawSevenSegmentDigit(x, 108, value[i] - '0');
    x += digitWidth + spacing;
  }
  drawText(28, 190, hasShot ? "LAST SHOT" : "READY", TEXT_LIGHT, 1);
  drawTarget();
}

void drawModeControl() {
  fillRect(250, 64, 214, 70, PANEL);
  drawText(262, 80, "MODE", ACCENT, 1);

  fillRect(TRAIN_X, TRAIN_Y, TRAIN_W, TRAIN_H,
           matchMode ? MUTED : ACCENT);
  fillRect(MATCH_X, MATCH_Y, MATCH_W, MATCH_H,
           matchMode ? ACCENT : MUTED);
  drawText(TRAIN_X + 16, TRAIN_Y + 18, "TRAIN",
           matchMode ? TEXT_LIGHT : TEXT_DARK, 1);
  drawText(MATCH_X + 16, MATCH_Y + 18, "MATCH",
           matchMode ? TEXT_DARK : TEXT_LIGHT, 1);
}

void drawDistanceControl() {
  fillRect(250, 146, 214, 70, PANEL);
  drawText(262, 162, "DIST", ACCENT, 1);

  fillRect(MINUS_X, MINUS_Y, MINUS_W, MINUS_H, MUTED);
  fillRect(PLUS_X, PLUS_Y, PLUS_W, PLUS_H, ACCENT);
  drawText(MINUS_X + 13, MINUS_Y + 15, "-", TEXT_LIGHT, 2);
  drawText(PLUS_X + 13, PLUS_Y + 15, "+", TEXT_DARK, 2);

  fillRect(364, 158, 56, 44, HEADER);
  char distance[4];
  snprintf(distance, sizeof(distance), "%d", distanceYards);
  const int width = strlen(distance) * 12;
  drawText(392 - width / 2, 168, distance, ACCENT, 2);
  drawText(386, 190, "YD", TEXT_LIGHT, 1);
}

void drawShotButton(bool pressed) {
  fillRect(SHOT_X, SHOT_Y, SHOT_W, SHOT_H,
           pressed ? ACCENT_DARK : ACCENT);
  drawText(162, 261, "SIMULATE SHOT", TEXT_DARK, 2);
}

void drawUi() {
  fillScreen(BACKGROUND);
  fillRect(0, 0, LCD_WIDTH, 48, HEADER);
  drawText(18, 17, "PRECISIONSHOT", ACCENT, 2);
  drawText(389, 20, "SYSTEM READY", TEXT_LIGHT, 1);
  fillRect(370, 18, 10, 10, ACCENT);
  drawScoreCard();
  drawModeControl();
  drawDistanceControl();
  drawShotButton(false);
}

bool inside(uint16_t x, uint16_t y, int left, int top, int width, int height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

void simulateShot() {
  const int spread = map(distanceYards, 5, 100, 18, matchMode ? 96 : 76);
  impactX = random(-spread, spread + 1);
  impactY = random(-spread, spread + 1);
  const float radius = sqrtf(impactX * impactX + impactY * impactY);
  shotScore = max(0, 100 - static_cast<int>(radius * 0.78f));
  hasShot = true;

  drawShotButton(true);
  drawScoreCard();
  Serial.printf("Shot: score=%d impact=(%d,%d) mode=%s distance=%dyd\n",
                shotScore, impactX, impactY,
                matchMode ? "MATCH" : "TRAIN", distanceYards);
}

uint8_t readTouchRegister(uint8_t reg) {
  Wire.beginTransmission(TOUCH_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return 0;
  }
  if (Wire.requestFrom(TOUCH_ADDRESS, static_cast<uint8_t>(1)) != 1) {
    return 0;
  }
  return Wire.read();
}

bool readTouch(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(TOUCH_ADDRESS);
  Wire.write(0x02); // TD_STATUS, followed by the first touch point.
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(TOUCH_ADDRESS, static_cast<uint8_t>(5)) != 5) {
    return false;
  }

  const uint8_t touches = Wire.read() & 0x0F;
  const uint8_t xHigh = Wire.read();
  const uint8_t xLow = Wire.read();
  const uint8_t yHigh = Wire.read();
  const uint8_t yLow = Wire.read();
  if (touches == 0) {
    return false;
  }

  const uint16_t portraitX = ((xHigh & 0x0F) << 8) | xLow;
  const uint16_t portraitY = ((yHigh & 0x0F) << 8) | yLow;
  if (portraitX >= 320 || portraitY >= 480) {
    return false;
  }

  // Rotate the FT6336 portrait coordinates clockwise into 480x320 landscape.
  x = portraitY;
  y = LCD_HEIGHT - 1 - portraitX;
  return true;
}

void initializeTouch() {
  pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
  pinMode(PIN_TOUCH_RESET, OUTPUT);
  digitalWrite(PIN_TOUCH_RESET, LOW);
  delay(20);
  digitalWrite(PIN_TOUCH_RESET, HIGH);
  delay(300);

  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);
  Serial.printf("FT6336 chip ID: 0x%02X\n", readTouchRegister(0xA3));
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_SD_CS, OUTPUT);
  pinMode(PIN_LCD_CS, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_BACKLIGHT, OUTPUT);

  // Deselect both SPI devices before starting the shared bus.
  digitalWrite(PIN_SD_CS, HIGH);
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_RESET, HIGH);
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_BACKLIGHT, HIGH);

  lcdSpi.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, -1);
  initializeDisplay();
  initializeTouch();
  drawUi();

  randomSeed(micros());
  Serial.println("PrecisionShot control UI started");
}

void loop() {
  uint16_t x = 0;
  uint16_t y = 0;
  const bool touchIsDown = readTouch(x, y);

  if (touchIsDown && !touchWasDown) {
    Serial.printf("Touch: %u, %u\n", x, y);
    if (inside(x, y, TRAIN_X, TRAIN_Y, TRAIN_W, TRAIN_H)) {
      matchMode = false;
      drawModeControl();
    } else if (inside(x, y, MATCH_X, MATCH_Y, MATCH_W, MATCH_H)) {
      matchMode = true;
      drawModeControl();
    } else if (inside(x, y, MINUS_X, MINUS_Y, MINUS_W, MINUS_H)) {
      distanceYards = max(5, distanceYards - 5);
      drawDistanceControl();
    } else if (inside(x, y, PLUS_X, PLUS_Y, PLUS_W, PLUS_H)) {
      distanceYards = min(100, distanceYards + 5);
      drawDistanceControl();
    } else if (inside(x, y, SHOT_X, SHOT_Y, SHOT_W, SHOT_H)) {
      simulateShot();
    }
  } else if (!touchIsDown && touchWasDown) {
    drawShotButton(false);
  }

  touchWasDown = touchIsDown;
  delay(15);
}
