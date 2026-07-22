#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <SPI.h>
#include <Wire.h>

// PrecisionShot ESP32-S3 prototype firmware. This combines the working BLE
// GATT server with a small, touch-first on-device UI.
constexpr char DEVICE_NAME[] = "PrecisionShot";
constexpr char SERVICE_UUID[] = "8c7a0001-6c3b-4f3d-a8d9-2adbc9f10211";
constexpr char TX_UUID[] = "8c7a0002-6c3b-4f3d-a8d9-2adbc9f10211";
constexpr char RX_UUID[] = "8c7a0003-6c3b-4f3d-a8d9-2adbc9f10211";

constexpr int PIN_SD_CS = 4;
constexpr int PIN_MISO = 9;
constexpr int PIN_BACKLIGHT = 10;
constexpr int PIN_SCLK = 11;
constexpr int PIN_MOSI = 12;
constexpr int PIN_DC = 13;
constexpr int PIN_RESET = 14;
constexpr int PIN_LCD_CS = 3;
constexpr int PIN_TOUCH_INT = 5;
constexpr int PIN_TOUCH_SDA = 6;
constexpr int PIN_TOUCH_RESET = 7;
constexpr int PIN_TOUCH_SCL = 8;

constexpr uint16_t LCD_WIDTH = 480;
constexpr uint16_t LCD_HEIGHT = 320;
constexpr uint32_t SPI_FREQUENCY = 80000000;
constexpr uint8_t TOUCH_ADDRESS = 0x38;

constexpr int SCORE_X = 168;
constexpr int SCORE_Y = 64;
constexpr int SCORE_W = 296;
constexpr int SCORE_H = 178;
constexpr int RESET_X = 16;
constexpr int RESET_Y = 164;
constexpr int RESET_W = 136;
constexpr int RESET_H = 78;
constexpr int BLE_ACTION_X = 330;
constexpr int BLE_ACTION_Y = 91;
constexpr int BLE_ACTION_W = 118;
constexpr int BLE_ACTION_H = 62;
constexpr int THEME_ROW_X = 16;
constexpr int THEME_ROW_Y = 190;
constexpr int THEME_ROW_W = 448;
constexpr int THEME_ROW_H = 68;
constexpr int MENU_X = 270;
constexpr int MENU_CLASSIC_Y = 68;
constexpr int MENU_SETTINGS_Y = 138;
constexpr int MENU_ITEM_H = 56;
constexpr int BUBBLE_X = 438;
constexpr int BUBBLE_Y = 278;
constexpr int BUBBLE_RADIUS = 27;

struct Palette {
  uint16_t background;
  uint16_t surface;
  uint16_t surfaceAlt;
  uint16_t header;
  uint16_t accent;
  uint16_t accentPressed;
  uint16_t text;
  uint16_t muted;
  uint16_t onAccent;
  uint16_t success;
  uint16_t danger;
};

constexpr Palette DARK_PALETTE = {
    0x0840, 0x18C2, 0x2944, 0x0000, 0xEE82, 0xBDA4,
    0xF77A, 0x8410, 0x0840, 0x3666, 0xD145};
constexpr Palette LIGHT_PALETTE = {
    0xEF7D, 0xFFFF, 0xDEFB, 0xFFFF, 0xDDA2, 0xB4C2,
    0x18C2, 0x6B4D, 0x0840, 0x2589, 0xC904};

enum class Page : uint8_t { Classic, Settings };

SPIClass lcdSpi(FSPI);
const SPISettings lcdSettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0);

BLEServer *bleServer = nullptr;
BLECharacteristic *txCharacteristic = nullptr;
BLEAdvertising *bleAdvertising = nullptr;

volatile bool phoneConnected = false;
volatile bool connectionChanged = false;
volatile bool restartAdvertising = false;
volatile bool rxChanged = false;
portMUX_TYPE bleMux = portMUX_INITIALIZER_UNLOCKED;

Page currentPage = Page::Classic;
bool menuOpen = false;
bool darkMode = true;
bool touchWasDown = false;
bool bleAdvertisingActive = false;
bool hasShot = false;
char lastRx[25] = "NONE";
uint32_t shotCount = 0;
uint32_t rxCount = 0;
uint32_t connectionCount = 0;
int lastShotScore = 0;

const Palette &theme() {
  return darkMode ? DARK_PALETTE : LIGHT_PALETTE;
}

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
  const uint8_t bytes[] = {static_cast<uint8_t>(data >> 8),
                           static_cast<uint8_t>(data)};
  startWrite(true);
  lcdSpi.writeBytes(bytes, sizeof(bytes));
  endWrite();
}

void initializeDisplay() {
  digitalWrite(PIN_RESET, LOW);
  delay(20);
  digitalWrite(PIN_RESET, HIGH);
  delay(20);

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
  delay(120);
  writeCommand(0x29);
}

void setWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd,
               uint16_t yEnd) {
  writeCommand(0x2A);
  writeData16(xStart); writeData16(xEnd);
  writeCommand(0x2B);
  writeData16(yStart); writeData16(yEnd);
  writeCommand(0x2C);
}

void fillRect(int x, int y, int width, int height, uint16_t color) {
  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > LCD_WIDTH || y + height > LCD_HEIGHT) return;

  setWindow(x, y, x + width - 1, y + height - 1);
  const uint8_t pixel[] = {static_cast<uint8_t>(color >> 8),
                           static_cast<uint8_t>(color)};
  startWrite(true);
  lcdSpi.writePattern(pixel, sizeof(pixel), width * height);
  endWrite();
}

void fillScreen(uint16_t color) {
  fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void fillCircle(int centerX, int centerY, int radius, uint16_t color) {
  for (int y = -radius; y <= radius; ++y) {
    const int halfWidth = static_cast<int>(sqrtf(radius * radius - y * y));
    fillRect(centerX - halfWidth, centerY + y, halfWidth * 2 + 1, 1, color);
  }
}

void fillPill(int x, int y, int width, int height, uint16_t color) {
  const int radius = (height - 1) / 2;
  fillRect(x + radius, y, width - radius * 2, height, color);
  fillCircle(x + radius, y + height / 2, radius, color);
  fillCircle(x + width - radius - 1, y + height / 2, radius, color);
}

const uint8_t *glyphFor(char character) {
  static const uint8_t space[5] = {0, 0, 0, 0, 0};
  static const uint8_t plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static const uint8_t minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
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

  if (character >= 'a' && character <= 'z') character -= 32;
  if (character >= 'A' && character <= 'Z') return letters[character - 'A'];
  if (character >= '0' && character <= '9') return digits[character - '0'];
  if (character == '+') return plus;
  if (character == '-') return minus;
  if (character == ':') return colon;
  return space;
}

int textWidth(const char *text, int scale = 1) {
  const int length = strlen(text);
  return length == 0 ? 0 : length * 6 * scale - scale;
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

void drawCenteredText(int left, int width, int y, const char *text,
                      uint16_t color, int scale = 1) {
  drawText(left + (width - textWidth(text, scale)) / 2, y, text, color, scale);
}

void drawHeader(const char *pageName) {
  fillRect(0, 0, LCD_WIDTH, 48, theme().header);
  drawText(16, 16, "PRECISIONSHOT", theme().accent, 2);
  drawText(LCD_WIDTH - textWidth(pageName) - 16, 20, pageName, theme().muted);
}

void drawConnectionDot(int x, int y) {
  fillCircle(x, y, 5, phoneConnected ? theme().success : theme().accent);
}

void drawMenuBubble() {
  fillCircle(BUBBLE_X + 2, BUBBLE_Y + 3, BUBBLE_RADIUS, theme().background);
  fillCircle(BUBBLE_X, BUBBLE_Y, BUBBLE_RADIUS,
             menuOpen ? theme().accentPressed : theme().accent);

  if (menuOpen) {
    for (int offset = -8; offset <= 8; ++offset) {
      fillRect(BUBBLE_X + offset - 1, BUBBLE_Y + offset - 1, 3, 3,
               theme().onAccent);
      fillRect(BUBBLE_X + offset - 1, BUBBLE_Y - offset - 1, 3, 3,
               theme().onAccent);
    }
  } else {
    fillRect(BUBBLE_X - 10, BUBBLE_Y - 8, 20, 3, theme().onAccent);
    fillRect(BUBBLE_X - 10, BUBBLE_Y - 1, 20, 3, theme().onAccent);
    fillRect(BUBBLE_X - 10, BUBBLE_Y + 6, 20, 3, theme().onAccent);
  }
}

void drawClassicPage() {
  fillScreen(theme().background);
  drawHeader("CLASSIC MODE");

  fillRect(16, 64, 136, 84, theme().surface);
  drawText(28, 78, "SHOT COUNT", theme().muted);
  char countText[12];
  snprintf(countText, sizeof(countText), "%lu",
           static_cast<unsigned long>(shotCount));
  drawCenteredText(16, 136, 103, countText, theme().text, 4);

  fillRect(RESET_X, RESET_Y, RESET_W, RESET_H, theme().surfaceAlt);
  drawCenteredText(RESET_X, RESET_W, RESET_Y + 18, "RESET", theme().danger, 2);
  drawCenteredText(RESET_X, RESET_W, RESET_Y + 49, "SESSION", theme().muted);

  fillRect(SCORE_X, SCORE_Y, SCORE_W, SCORE_H, theme().surface);
  drawText(SCORE_X + 16, SCORE_Y + 14, "LAST SHOT SCORE", theme().muted);
  char scoreText[5];
  if (hasShot) {
    snprintf(scoreText, sizeof(scoreText), "%d", lastShotScore);
  } else {
    strcpy(scoreText, "--");
  }
  drawCenteredText(SCORE_X, SCORE_W, SCORE_Y + 53, scoreText,
                   theme().accent, 8);
  drawCenteredText(SCORE_X, SCORE_W, SCORE_Y + 150,
                   "TAP CARD FOR TEST SHOT", theme().muted);

  fillRect(16, 258, 368, 46, theme().surface);
  drawConnectionDot(34, 281);
  drawText(50, 276, "BLUETOOTH", theme().muted);
  drawText(126, 276, phoneConnected ? "CONNECTED" : "READY TO PAIR",
           theme().text);
  drawMenuBubble();
}

void drawToggle() {
  constexpr int x = 385;
  constexpr int y = 207;
  constexpr int width = 62;
  constexpr int height = 34;
  fillPill(x, y, width, height,
           darkMode ? theme().accent : theme().surfaceAlt);
  fillCircle(darkMode ? x + width - 17 : x + 17, y + height / 2, 12,
             darkMode ? theme().onAccent : theme().muted);
}

void drawSettingsPage() {
  fillScreen(theme().background);
  drawHeader("SETTINGS");

  fillRect(16, 64, 448, 112, theme().surface);
  drawText(28, 78, "BLUETOOTH", theme().muted);
  drawConnectionDot(34, 112);
  drawText(50, 105, phoneConnected ? "CONNECTED" : "READY TO PAIR",
           theme().text, 2);
  drawText(28, 147, "DEVICE  PRECISIONSHOT", theme().muted);

  fillRect(BLE_ACTION_X, BLE_ACTION_Y, BLE_ACTION_W, BLE_ACTION_H,
           phoneConnected ? theme().surfaceAlt : theme().accent);
  drawCenteredText(BLE_ACTION_X, BLE_ACTION_W, BLE_ACTION_Y + 17,
                   phoneConnected ? "LINKED" : "SEARCH",
                   phoneConnected ? theme().success : theme().onAccent, 2);
  if (!phoneConnected) {
    drawCenteredText(BLE_ACTION_X, BLE_ACTION_W, BLE_ACTION_Y + 43,
                     bleAdvertisingActive ? "ADVERTISING" : "RESTART BLE",
                     theme().onAccent);
  }

  fillRect(THEME_ROW_X, THEME_ROW_Y, THEME_ROW_W, THEME_ROW_H,
           theme().surface);
  drawText(28, 204, "DARK MODE", theme().text, 2);
  drawText(28, 232, darkMode ? "DARK THEME ACTIVE" : "LIGHT THEME ACTIVE",
           theme().muted);
  drawToggle();

  char activity[32];
  snprintf(activity, sizeof(activity), "LINKS %lu  MESSAGES %lu",
           static_cast<unsigned long>(connectionCount),
           static_cast<unsigned long>(rxCount));
  drawText(18, 284, activity, theme().muted);
  drawMenuBubble();
}

void drawMenu() {
  fillRect(MENU_X - 6, 0, 6, LCD_HEIGHT, theme().surfaceAlt);
  fillRect(MENU_X, 0, LCD_WIDTH - MENU_X, LCD_HEIGHT, theme().surface);
  drawText(MENU_X + 18, 24, "MENU", theme().text, 2);

  const bool classicActive = currentPage == Page::Classic;
  fillRect(MENU_X + 12, MENU_CLASSIC_Y, 186, MENU_ITEM_H,
           classicActive ? theme().accent : theme().surfaceAlt);
  fillRect(MENU_X + 12, MENU_SETTINGS_Y, 186, MENU_ITEM_H,
           classicActive ? theme().surfaceAlt : theme().accent);
  drawText(MENU_X + 28, MENU_CLASSIC_Y + 24, "CLASSIC MODE",
           classicActive ? theme().onAccent : theme().text);
  drawText(MENU_X + 28, MENU_SETTINGS_Y + 24, "SETTINGS",
           classicActive ? theme().text : theme().onAccent);

  drawConnectionDot(MENU_X + 24, 229);
  drawText(MENU_X + 38, 224, phoneConnected ? "BLE CONNECTED" : "BLE READY",
           theme().muted);
  drawMenuBubble();
}

void renderScreen() {
  if (currentPage == Page::Classic) {
    drawClassicPage();
  } else {
    drawSettingsPage();
  }
  if (menuOpen) drawMenu();
}

uint8_t readTouchRegister(uint8_t reg) {
  Wire.beginTransmission(TOUCH_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom(TOUCH_ADDRESS, static_cast<uint8_t>(1)) != 1) return 0;
  return Wire.read();
}

bool readTouch(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(TOUCH_ADDRESS);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_ADDRESS, static_cast<uint8_t>(5)) != 5) return false;

  const uint8_t touches = Wire.read() & 0x0F;
  const uint8_t xHigh = Wire.read();
  const uint8_t xLow = Wire.read();
  const uint8_t yHigh = Wire.read();
  const uint8_t yLow = Wire.read();
  if (touches == 0) return false;

  const uint16_t portraitX = ((xHigh & 0x0F) << 8) | xLow;
  const uint16_t portraitY = ((yHigh & 0x0F) << 8) | yLow;
  if (portraitX >= 320 || portraitY >= 480) return false;
  x = portraitY;
  y = LCD_HEIGHT - 1 - portraitX;
  return true;
}

bool inside(uint16_t x, uint16_t y, int left, int top, int width, int height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

class ServerCallbacks final : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    phoneConnected = true;
    bleAdvertisingActive = false;
    connectionChanged = true;
    Serial.println("[BLE] Phone connected");
  }

  void onDisconnect(BLEServer *) override {
    phoneConnected = false;
    connectionChanged = true;
    restartAdvertising = true;
    Serial.println("[BLE] Phone disconnected; advertising will restart");
  }
};

class RxCallbacks final : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    const std::string value = characteristic->getValue();
    portENTER_CRITICAL(&bleMux);
    const size_t count = min(value.size(), sizeof(lastRx) - 1);
    memcpy(lastRx, value.data(), count);
    lastRx[count] = '\0';
    rxChanged = true;
    portEXIT_CRITICAL(&bleMux);
    Serial.printf("[BLE RX] %s\n", lastRx);
  }
};

void initializeBle() {
  Serial.println("[BLE] Initializing GATT server");
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(185);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  BLEService *service = bleServer->createService(SERVICE_UUID);

  txCharacteristic = service->createCharacteristic(
      TX_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  txCharacteristic->addDescriptor(new BLE2902());
  txCharacteristic->setValue("READY");

  BLECharacteristic *rxCharacteristic = service->createCharacteristic(
      RX_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
                   BLECharacteristic::PROPERTY_WRITE_NR);
  rxCharacteristic->setCallbacks(new RxCallbacks());
  rxCharacteristic->setValue("WRITE PING");

  service->start();
  bleAdvertising = BLEDevice::getAdvertising();
  bleAdvertising->addServiceUUID(SERVICE_UUID);
  bleAdvertising->setScanResponse(true);
  bleAdvertising->start();
  bleAdvertisingActive = true;

  Serial.printf("[BLE] Advertising as %s\n", DEVICE_NAME);
  Serial.printf("[BLE] Service: %s\n", SERVICE_UUID);
  Serial.printf("[BLE] TX notify: %s\n", TX_UUID);
  Serial.printf("[BLE] RX write: %s\n", RX_UUID);
}

void startAdvertising() {
  if (phoneConnected || bleAdvertising == nullptr) return;
  bleAdvertising->stop();
  bleAdvertising->start();
  bleAdvertisingActive = true;
  Serial.println("[BLE] Advertising restarted");
}

void resetSession() {
  shotCount = 0;
  lastShotScore = 0;
  hasShot = false;
  Serial.println("[SESSION] Classic session reset");
  renderScreen();
}

void sendTestHit() {
  ++shotCount;
  lastShotScore = 10 - ((shotCount - 1) % 4);
  hasShot = true;

  if (phoneConnected) {
    char packet[21];
    // Keep the diagnostic notification within the default 20-byte payload.
    snprintf(packet, sizeof(packet), "{\"hit\":%lu,\"score\":%d}",
             static_cast<unsigned long>(shotCount % 10), lastShotScore);
    txCharacteristic->setValue(reinterpret_cast<uint8_t *>(packet), strlen(packet));
    txCharacteristic->notify();
    Serial.printf("[BLE TX] %s (%u bytes)\n", packet,
                  static_cast<unsigned>(strlen(packet)));
  } else {
    restartAdvertising = true;
    Serial.printf("[SHOT] Test shot %lu scored %d; no phone connected\n",
                  static_cast<unsigned long>(shotCount), lastShotScore);
  }
  renderScreen();
}

void handleMenuTouch(uint16_t x, uint16_t y) {
  if (x < MENU_X) {
    menuOpen = false;
  } else if (inside(x, y, MENU_X + 12, MENU_CLASSIC_Y, 186, MENU_ITEM_H)) {
    currentPage = Page::Classic;
    menuOpen = false;
  } else if (inside(x, y, MENU_X + 12, MENU_SETTINGS_Y, 186, MENU_ITEM_H)) {
    currentPage = Page::Settings;
    menuOpen = false;
  }
  renderScreen();
}

void handleTouch(uint16_t x, uint16_t y) {
  Serial.printf("[TOUCH] %u, %u\n", x, y);

  if (inside(x, y, BUBBLE_X - BUBBLE_RADIUS - 4,
             BUBBLE_Y - BUBBLE_RADIUS - 4,
             BUBBLE_RADIUS * 2 + 8, BUBBLE_RADIUS * 2 + 8)) {
    menuOpen = !menuOpen;
    renderScreen();
    return;
  }

  if (menuOpen) {
    handleMenuTouch(x, y);
    return;
  }

  if (currentPage == Page::Classic) {
    if (inside(x, y, RESET_X, RESET_Y, RESET_W, RESET_H)) {
      resetSession();
    } else if (inside(x, y, SCORE_X, SCORE_Y, SCORE_W, SCORE_H)) {
      sendTestHit();
    }
    return;
  }

  if (inside(x, y, BLE_ACTION_X, BLE_ACTION_Y, BLE_ACTION_W, BLE_ACTION_H)) {
    if (!phoneConnected) startAdvertising();
    renderScreen();
  } else if (inside(x, y, THEME_ROW_X, THEME_ROW_Y,
                    THEME_ROW_W, THEME_ROW_H)) {
    darkMode = !darkMode;
    renderScreen();
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("=== PrecisionShot UI + BLE boot ===");

  pinMode(PIN_SD_CS, OUTPUT);
  pinMode(PIN_LCD_CS, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_BACKLIGHT, OUTPUT);
  pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
  pinMode(PIN_TOUCH_RESET, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_RESET, HIGH);
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_BACKLIGHT, HIGH);

  lcdSpi.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, -1);
  initializeDisplay();

  digitalWrite(PIN_TOUCH_RESET, LOW);
  delay(20);
  digitalWrite(PIN_TOUCH_RESET, HIGH);
  delay(300);
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);
  Serial.printf("[TOUCH] FT6336 chip ID: 0x%02X\n", readTouchRegister(0xA3));

  initializeBle();
  renderScreen();
  Serial.println("[READY] PrecisionShot Classic Mode started");
}

void loop() {
  if (restartAdvertising) {
    restartAdvertising = false;
    delay(100);
    startAdvertising();
    renderScreen();
  }

  if (connectionChanged) {
    connectionChanged = false;
    if (phoneConnected) ++connectionCount;
    renderScreen();
  }

  if (rxChanged) {
    char received[sizeof(lastRx)];
    portENTER_CRITICAL(&bleMux);
    strcpy(received, lastRx);
    rxChanged = false;
    portEXIT_CRITICAL(&bleMux);
    ++rxCount;

    if (phoneConnected && strcmp(received, "PING") == 0) {
      txCharacteristic->setValue("{\"pong\":1}");
      txCharacteristic->notify();
      Serial.println("[BLE TX] {\"pong\":1}");
    } else if (strcmp(received, "RESET") == 0) {
      resetSession();
    } else if (currentPage == Page::Settings) {
      renderScreen();
    }
  }

  uint16_t x = 0;
  uint16_t y = 0;
  const bool touchIsDown = readTouch(x, y);
  if (touchIsDown && !touchWasDown) handleTouch(x, y);
  touchWasDown = touchIsDown;
  delay(15);
}
