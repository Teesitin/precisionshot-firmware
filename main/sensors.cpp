#include "sensors.h"

#include <cstdio>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_check.h"

namespace sensors {
Reading regulator33;
Reading regulator5;
Reading input;
namespace {
adc_oneshot_unit_handle_t adc = nullptr;
adc_cali_handle_t calibration = nullptr;
uint64_t nextSample = 0;
// Main Board V4: GPIO9, GPIO10, and GPIO1 use ADC1 channels 8, 9, and 0.
constexpr adc_channel_t channels[] = {ADC_CHANNEL_8, ADC_CHANNEL_9, ADC_CHANNEL_0};

Reading read(adc_channel_t channel) {
    Reading reading;
    int sum = 0;
    for (int i = 0; i < 16; ++i) {
        int raw = 0;
        if (adc_oneshot_read(adc, channel, &raw) != ESP_OK)
            return reading;
        sum += raw;
    }
    reading.raw = (sum + 8) / 16;
    if (calibration &&
        adc_cali_raw_to_voltage(calibration, reading.raw, &reading.millivolts) != ESP_OK)
        reading.millivolts = -1;
    return reading;
}
} // namespace

void initialize() {
    adc_oneshot_unit_init_cfg_t unit = {};
    unit.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &adc));
    adc_oneshot_chan_cfg_t config = {};
    config.atten = ADC_ATTEN_DB_12;
    config.bitwidth = ADC_BITWIDTH_12;
    for (auto channel : channels)
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, channel, &config));
    adc_cali_curve_fitting_config_t curve = {};
    curve.unit_id = ADC_UNIT_1;
    curve.atten = config.atten;
    curve.bitwidth = config.bitwidth;
    if (adc_cali_create_scheme_curve_fitting(&curve, &calibration) != ESP_OK) {
        calibration = nullptr;
        puts("[SENSORS] ADC calibration unavailable; raw readings only");
    }
    puts("[SENSORS] MCP9700: GPIO9/10; sensor voltage: GPIO1, J_PSB pin 9");
}

bool update(uint64_t now) {
    // Average a few samples five times per second to keep the numbers steady.
    if (now < nextSample)
        return false;
    nextSample = now + 200;
    regulator33 = read(channels[0]);
    regulator5 = read(channels[1]);
    input = read(channels[2]);
    return true;
}

int temperatureTenths(const Reading &reading) {
    // MCP9700 gives 500 mV at zero Celsius and adds 10 mV per degree.
    if (reading.millivolts < 100 || reading.millivolts > 1750)
        return -9999;
    return reading.millivolts - 500;
}
} // namespace sensors
