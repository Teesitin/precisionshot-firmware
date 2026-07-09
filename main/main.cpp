#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#define HIT_THRESHOLD_MV 1800

static int raw_to_mv(int raw)
{
    return (raw * 3300) / 4095;
}

extern "C" void app_main(void)
{
    printf("PrecisionShot 3-sensor ADC test starting...\n");
    printf("Reading GPIO4, GPIO5, GPIO6 every 100ms\n");

    adc_oneshot_unit_handle_t adc1_handle;

    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = ADC_UNIT_1;

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.bitwidth = ADC_BITWIDTH_12;
    channel_config.atten = ADC_ATTEN_DB_12;

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &channel_config)); // GPIO4
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &channel_config)); // GPIO5
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &channel_config)); // GPIO6

    while (true)
    {
        int raw1 = 0;
        int raw2 = 0;
        int raw3 = 0;

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &raw1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &raw2));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_5, &raw3));

        int mv1 = raw_to_mv(raw1);
        int mv2 = raw_to_mv(raw2);
        int mv3 = raw_to_mv(raw3);

        bool hit1 = mv1 > HIT_THRESHOLD_MV;
        bool hit2 = mv2 > HIT_THRESHOLD_MV;
        bool hit3 = mv3 > HIT_THRESHOLD_MV;

        printf("S1 GPIO4: %4d raw / %4d mV [%s] | "
               "S2 GPIO5: %4d raw / %4d mV [%s] | "
               "S3 GPIO6: %4d raw / %4d mV [%s]\n",
               raw1, mv1, hit1 ? "HIT" : "---",
               raw2, mv2, hit2 ? "HIT" : "---",
               raw3, mv3, hit3 ? "HIT" : "---");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}