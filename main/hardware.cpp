#include "hardware.h"

#include <algorithm>
#include <cstdio>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace hardware {
namespace {
// Main Board V4 connections. These are GPIO numbers, not module pad numbers.
constexpr gpio_num_t SD_CS = GPIO_NUM_18;
constexpr gpio_num_t MISO = GPIO_NUM_13;
constexpr gpio_num_t BACKLIGHT = GPIO_NUM_17;
constexpr gpio_num_t SCLK = GPIO_NUM_12;
constexpr gpio_num_t MOSI = GPIO_NUM_11;
constexpr gpio_num_t DC = GPIO_NUM_15;
constexpr gpio_num_t RESET = GPIO_NUM_16;
constexpr gpio_num_t LCD_CS = GPIO_NUM_14;
constexpr gpio_num_t TOUCH_INT = GPIO_NUM_41;
constexpr gpio_num_t TOUCH_SDA = GPIO_NUM_39;
constexpr gpio_num_t TOUCH_RESET = GPIO_NUM_42;
constexpr gpio_num_t TOUCH_SCL = GPIO_NUM_40;
// Main Board V4 SPK: U1 pad 36 = GPIO44 -> LM386 input -> SPK1 and SPK2.
constexpr gpio_num_t SPEAKER = GPIO_NUM_44;
bool speakerActive = false;
const SpeakerNote *speakerNotes = nullptr;
size_t speakerCount = 0;
size_t speakerIndex = 0;
uint64_t speakerDeadline = 0;
uint16_t speakerFrequency = 0;

void silenceSpeaker() {
    // Disconnect PWM while idle and hold the amplifier input low.
    ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
    ESP_ERROR_CHECK(gpio_reset_pin(SPEAKER));
    ESP_ERROR_CHECK(gpio_set_level(SPEAKER, 0));
    ESP_ERROR_CHECK(gpio_set_direction(SPEAKER, GPIO_MODE_OUTPUT));
}

void speakerTone(uint16_t frequency) {
    if (frequency == speakerFrequency)
        return;
    // Turn sound off before changing the pitch.
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    if (frequency != 0) {
        ESP_ERROR_CHECK(ledc_set_pin(SPEAKER, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
        ESP_ERROR_CHECK(ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, frequency));
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    } else
        silenceSpeaker();
    speakerFrequency = frequency;
}
constexpr int DISPLAY_CLOCK_HZ = 20000000;
constexpr size_t PIXEL_BUFFER_SIZE = 4096;
spi_device_handle_t display = nullptr;
i2c_master_bus_handle_t touchBus = nullptr;
i2c_master_dev_handle_t touch = nullptr;
// Reuse this buffer when sending pixels to the display.
DMA_ATTR uint8_t pixels[PIXEL_BUFFER_SIZE];
} // namespace

void sleepMs(uint32_t milliseconds) {
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

uint64_t nowMs() {
    return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
}

// Set up the speaker, display pins, SPI display bus, and I2C touch bus.
void initialize() {
    ledc_timer_config_t speakerTimer = {};
    speakerTimer.speed_mode = LEDC_LOW_SPEED_MODE;
    speakerTimer.duty_resolution = LEDC_TIMER_10_BIT;
    speakerTimer.timer_num = LEDC_TIMER_0;
    speakerTimer.freq_hz = 2000;
    speakerTimer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&speakerTimer));
    ledc_channel_config_t speakerChannel = {};
    speakerChannel.gpio_num = SPEAKER;
    speakerChannel.speed_mode = LEDC_LOW_SPEED_MODE;
    speakerChannel.channel = LEDC_CHANNEL_0;
    speakerChannel.timer_sel = LEDC_TIMER_0;
    speakerChannel.duty = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&speakerChannel));
    silenceSpeaker();
    printf("[BOARD] Speakers: SPK GPIO=%d, shared LM386, 2000 Hz test\n", SPEAKER);
    printf(
        "[BOARD] Main Board V4: LCD MOSI=%d SCK=%d MISO=%d CS=%d DC=%d RST=%d BL=%d SD_CS=%d SPI=%d Hz\n",
        MOSI, SCLK, MISO, LCD_CS, DC, RESET, BACKLIGHT, SD_CS, DISPLAY_CLOCK_HZ);
    printf("[BOARD] Touch SDA=%d SCL=%d INT=%d RST=%d\n", TOUCH_SDA, TOUCH_SCL, TOUCH_INT,
        TOUCH_RESET);
    gpio_config_t outputs = {};
    outputs.pin_bit_mask = (1ULL << SD_CS) | (1ULL << LCD_CS) | (1ULL << RESET) | (1ULL << DC) |
        (1ULL << BACKLIGHT) | (1ULL << TOUCH_RESET);
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
    // Use the display speed that worked during board testing.
    device.clock_speed_hz = DISPLAY_CLOCK_HZ;
    device.mode = 0;
    // Keep display CS low until the whole write is finished.
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

bool startSpeakerTest() {
    // Keep old app commands compatible, but the single test beep is removed.
    stopSpeakerSequence();
    return true;
}

// Play stored notes. Only one sound can play at a time.
bool startSpeakerSequence(const SpeakerNote *notes, size_t count) {
    if (speakerActive || notes == nullptr || count == 0)
        return false;
    for (size_t i = 0; i < count; ++i) {
        if (notes[i].durationMs == 0 ||
            (notes[i].frequencyHz != 0 &&
                (notes[i].frequencyHz < 100 || notes[i].frequencyHz > 8000)))
            return false;
    }
    speakerNotes = notes;
    speakerCount = count;
    speakerIndex = 0;
    speakerDeadline = nowMs() + notes[0].durationMs;
    speakerActive = true;
    speakerTone(notes[0].frequencyHz);
    return true;
}

// Move to the next note when its time is up, without blocking the UI.
void advanceSpeakerTest(uint64_t now) {
    if (!speakerActive)
        return;
    while (now >= speakerDeadline) {
        if (++speakerIndex >= speakerCount) {
            stopSpeakerSequence();
            return;
        }
        speakerDeadline += speakerNotes[speakerIndex].durationMs;
    }
    speakerTone(speakerNotes[speakerIndex].frequencyHz);
}

void stopSpeakerSequence() {
    speakerTone(0);
    silenceSpeaker();
    speakerActive = false;
    speakerNotes = nullptr;
    speakerCount = 0;
}

bool speakerTestActive() {
    return speakerActive;
}

// Reset each device using its reset pin.
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

// Send display commands and pixels over SPI.
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
            for (size_t i = 0; i < count; ++i)
                transfer.tx_data[i] = bytes[i];
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
    // Send the register number and read its data in the same I2C transaction.
    return i2c_master_transmit_receive(touch, &reg, 1, bytes, length, 20) == ESP_OK;
}
} // namespace hardware
