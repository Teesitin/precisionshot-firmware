#include "hardware.h"

#include <algorithm>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace hardware {
namespace {
constexpr gpio_num_t SD_CS = GPIO_NUM_4;
constexpr gpio_num_t MISO = GPIO_NUM_9;
constexpr gpio_num_t BACKLIGHT = GPIO_NUM_10;
constexpr gpio_num_t SCLK = GPIO_NUM_11;
constexpr gpio_num_t MOSI = GPIO_NUM_12;
constexpr gpio_num_t DC = GPIO_NUM_13;
constexpr gpio_num_t RESET = GPIO_NUM_14;
constexpr gpio_num_t LCD_CS = GPIO_NUM_3;
constexpr gpio_num_t TOUCH_INT = GPIO_NUM_5;
constexpr gpio_num_t TOUCH_SDA = GPIO_NUM_6;
constexpr gpio_num_t TOUCH_RESET = GPIO_NUM_7;
constexpr gpio_num_t TOUCH_SCL = GPIO_NUM_8;
constexpr size_t PIXEL_BUFFER_SIZE = 4096;
spi_device_handle_t display = nullptr;
i2c_master_bus_handle_t touchBus = nullptr;
i2c_master_dev_handle_t touch = nullptr;
// DMA storage is internal, aligned and reused only by the UI task.
DMA_ATTR uint8_t pixels[PIXEL_BUFFER_SIZE];
}

void sleepMs(uint32_t milliseconds) {
  vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

uint64_t nowMs() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
}

void initialize() {
  gpio_config_t outputs = {};
  outputs.pin_bit_mask = (1ULL << SD_CS) | (1ULL << LCD_CS) |
      (1ULL << RESET) | (1ULL << DC) | (1ULL << BACKLIGHT) |
      (1ULL << TOUCH_RESET);
  outputs.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&outputs));
  for (gpio_num_t pin : {SD_CS, LCD_CS, RESET, DC, BACKLIGHT, TOUCH_RESET}) {
    ESP_ERROR_CHECK(gpio_set_level(pin, 1));
  }
  gpio_config_t input = {};
  input.pin_bit_mask = 1ULL << TOUCH_INT;
  input.mode = GPIO_MODE_INPUT;
  input.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK(gpio_config(&input));

  spi_bus_config_t bus = {};
  bus.mosi_io_num = MOSI;
  bus.miso_io_num = MISO;
  bus.sclk_io_num = SCLK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.data4_io_num = -1;
  bus.data5_io_num = -1;
  bus.data6_io_num = -1;
  bus.data7_io_num = -1;
  bus.max_transfer_sz = PIXEL_BUFFER_SIZE;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
  spi_device_interface_config_t device = {};
  device.clock_speed_hz = 80000000;
  device.mode = 0;
  // Keep CS asserted across chunks of a rectangle, as on the original display.
  device.spics_io_num = -1;
  device.queue_size = 1;
  device.flags = SPI_DEVICE_HALFDUPLEX;
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &device, &display));

  i2c_master_bus_config_t i2c = {};
  i2c.i2c_port = I2C_NUM_0;
  i2c.sda_io_num = TOUCH_SDA;
  i2c.scl_io_num = TOUCH_SCL;
  i2c.clk_source = I2C_CLK_SRC_DEFAULT;
  i2c.glitch_ignore_cnt = 7;
  i2c.flags.enable_internal_pullup = true;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c, &touchBus));
  i2c_device_config_t touchConfig = {};
  touchConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  touchConfig.device_address = 0x38;
  touchConfig.scl_speed_hz = 400000;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(touchBus, &touchConfig, &touch));
}

void resetDisplay() {
  ESP_ERROR_CHECK(gpio_set_level(RESET, 0));
  sleepMs(20);
  ESP_ERROR_CHECK(gpio_set_level(RESET, 1));
  sleepMs(20);
}

void resetTouch() {
  ESP_ERROR_CHECK(gpio_set_level(TOUCH_RESET, 0));
  sleepMs(20);
  ESP_ERROR_CHECK(gpio_set_level(TOUCH_RESET, 1));
  sleepMs(300);
}

void beginDisplayWrite(bool dataMode) {
  ESP_ERROR_CHECK(spi_device_acquire_bus(display, portMAX_DELAY));
  ESP_ERROR_CHECK(gpio_set_level(DC, dataMode ? 1 : 0));
  ESP_ERROR_CHECK(gpio_set_level(LCD_CS, 0));
}

void endDisplayWrite() {
  ESP_ERROR_CHECK(gpio_set_level(LCD_CS, 1));
  spi_device_release_bus(display);
}

void writeDisplay(const uint8_t *bytes, size_t length) {
  while (length != 0) {
    const size_t count = std::min(length, PIXEL_BUFFER_SIZE);
    spi_transaction_t transfer = {};
    transfer.length = count * 8;
    if (count <= sizeof(transfer.tx_data)) {
      transfer.flags = SPI_TRANS_USE_TXDATA;
      for (size_t i = 0; i < count; ++i) transfer.tx_data[i] = bytes[i];
    } else {
      transfer.tx_buffer = bytes;
    }
    ESP_ERROR_CHECK(spi_device_polling_transmit(display, &transfer));
    bytes += count;
    length -= count;
  }
}

void writePixels(uint16_t color, size_t count) {
  const size_t buffered = std::min(count, sizeof(pixels) / 2);
  for (size_t i = 0; i < buffered; ++i) {
    pixels[2 * i] = static_cast<uint8_t>(color >> 8);
    pixels[2 * i + 1] = static_cast<uint8_t>(color);
  }
  while (count != 0) {
    const size_t chunk = std::min(count, buffered);
    writeDisplay(pixels, chunk * 2);
    count -= chunk;
  }
}

bool readTouchRegisters(uint8_t reg, uint8_t *bytes, size_t length) {
  // One transaction preserves the register-select/repeated-START sequence.
  return i2c_master_transmit_receive(touch, &reg, 1, bytes, length, 20) == ESP_OK;
}
}
