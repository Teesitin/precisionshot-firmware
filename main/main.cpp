#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <SPI.h>
#include <Wire.h>

// PrecisionShot ESP32-S3 Bluetooth Low Energy diagnostic firmware.
// This intentionally excludes the normal product UI and sensor pipeline.

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

constexpr uint16_t BACKGROUND = 0x0840;
constexpr uint16_t PANEL = 0x18C2;
constexpr uint16_t HEADER = 0x0000;
constexpr uint16_t ACCENT = 0xEE82;
constexpr uint16_t ACCENT_DARK = 0x7B47;
constexpr uint16_t MUTED = 0x41A4;
constexpr uint16_t TEXT_DARK = 0x0840;
constexpr uint16_t TEXT_LIGHT = 0xF77A;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t RED = 0xF800;

constexpr int HIT_X = 16;
constexpr int HIT_Y = 244;
constexpr int HIT_W = 448;
constexpr int HIT_H = 60;

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

char lastRx[25] = "NONE";
uint32_t hitCount = 0;
uint32_t rxCount = 0;
uint32_t connectionCount = 0;
bool touchWasDown = false;
bool buttonPressed = false;

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

const uint8_t *glyphFor(char character) {
  static const uint8_t space[5] = {0, 0, 0, 0, 0};
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
  if (character == '-') return minus;
  if (character == ':') return colon;
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

void drawStatus() {
  fillRect(16, 56, 448, 54, PANEL);
  drawText(28, 68, "BLE STATUS", ACCENT, 1);
  fillRect(28, 88, 10, 10, phoneConnected ? GREEN : ACCENT);
  drawText(48, 88, phoneConnected ? "CONNECTED" : "ADVERTISING", TEXT_LIGHT, 1);

  char count[24];
  snprintf(count, sizeof(count), "LINKS %lu", static_cast<unsigned long>(connectionCount));
  fillRect(328, 68, 124, 30, PANEL);
  drawText(340, 88, count, MUTED, 1);
}

void drawActivity() {
  fillRect(16, 122, 448, 108, PANEL);
  drawText(28, 134, "DEVICE", ACCENT, 1);
  drawText(112, 134, "PRECISIONSHOT", TEXT_LIGHT, 1);
  drawText(28, 154, "SERVICE", ACCENT, 1);
  drawText(112, 154, "8C7A0001", TEXT_LIGHT, 1);
  drawText(28, 174, "TX NOTIFY", ACCENT, 1);
  drawText(112, 174, "8C7A0002", TEXT_LIGHT, 1);
  drawText(28, 194, "RX WRITE", ACCENT, 1);
  drawText(112, 194, "8C7A0003", TEXT_LIGHT, 1);

  char activity[40];
  snprintf(activity, sizeof(activity), "HITS %lu  RX %lu  LAST %.12s",
           static_cast<unsigned long>(hitCount), static_cast<unsigned long>(rxCount), lastRx);
  fillRect(270, 134, 180, 78, PANEL);
  drawText(280, 194, activity, MUTED, 1);
}

void drawHitButton(bool pressed) {
  fillRect(HIT_X, HIT_Y, HIT_W, HIT_H, pressed ? ACCENT_DARK : ACCENT);
  drawText(phoneConnected ? 126 : 84, HIT_Y + 22,
           phoneConnected ? "SEND TEST HIT" : "START BLE SEARCH", TEXT_DARK, 2);
}

void drawUi() {
  fillScreen(BACKGROUND);
  fillRect(0, 0, LCD_WIDTH, 44, HEADER);
  drawText(16, 15, "PRECISIONSHOT BLE DEBUG", ACCENT, 2);
  drawStatus();
  drawActivity();
  drawHitButton(false);
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

  Serial.printf("[BLE] Advertising as %s\n", DEVICE_NAME);
  Serial.printf("[BLE] Service: %s\n", SERVICE_UUID);
  Serial.printf("[BLE] TX notify: %s\n", TX_UUID);
  Serial.printf("[BLE] RX write: %s\n", RX_UUID);
}

void sendTestHit() {
  if (!phoneConnected) {
    bleAdvertising->stop();
    bleAdvertising->start();
    Serial.println("[BLE] Advertising restarted by touchscreen");
    drawStatus();
    drawHitButton(true);
    return;
  }

  ++hitCount;
  const int score = 10 - ((hitCount - 1) % 4);
  char packet[21];
  // Stay within the default 20-byte BLE notification payload so this test
  // works even before the phone negotiates a larger MTU.
  snprintf(packet, sizeof(packet), "{\"hit\":%lu,\"score\":%d}",
           static_cast<unsigned long>(hitCount % 10), score);
  txCharacteristic->setValue(reinterpret_cast<uint8_t *>(packet), strlen(packet));
  txCharacteristic->notify();
  Serial.printf("[BLE TX] %s (%u bytes)\n", packet, static_cast<unsigned>(strlen(packet)));
  drawActivity();
  drawHitButton(true);
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("=== PrecisionShot BLE diagnostic boot ===");

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

  drawUi();
  initializeBle();
  Serial.println("[READY] Scan for PrecisionShot from the phone");
}

void loop() {
  if (restartAdvertising) {
    restartAdvertising = false;
    delay(100);
    bleAdvertising->start();
    Serial.println("[BLE] Advertising restarted");
  }

  if (connectionChanged) {
    connectionChanged = false;
    if (phoneConnected) ++connectionCount;
    drawStatus();
    drawHitButton(false);
  }

  if (rxChanged) {
    portENTER_CRITICAL(&bleMux);
    rxChanged = false;
    portEXIT_CRITICAL(&bleMux);
    ++rxCount;
    drawActivity();

    if (phoneConnected && strcmp(lastRx, "PING") == 0) {
      txCharacteristic->setValue("{\"pong\":1}");
      txCharacteristic->notify();
      Serial.println("[BLE TX] {\"pong\":1}");
    }
  }

  uint16_t x = 0;
  uint16_t y = 0;
  const bool touchIsDown = readTouch(x, y);
  if (touchIsDown && !touchWasDown && inside(x, y, HIT_X, HIT_Y, HIT_W, HIT_H)) {
    buttonPressed = true;
    sendTestHit();
  } else if (!touchIsDown && touchWasDown && buttonPressed) {
    buttonPressed = false;
    drawHitButton(false);
  }
  touchWasDown = touchIsDown;
  delay(15);
}
