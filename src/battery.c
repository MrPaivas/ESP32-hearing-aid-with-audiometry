#include "battery.h"

#include "board.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "battery";

#define BATT_CHANNEL ADC_CHANNEL_6 // GPIO34
#define SAMPLES      16

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static SemaphoreHandle_t s_mutex; // lido pelo loop principal e pelo servidor HTTP

void battery_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    adc_oneshot_unit_init_cfg_t unit_cfg = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));
    adc_oneshot_chan_cfg_t chan_cfg = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, BATT_CHANNEL, &chan_cfg));

    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "sem calibração de fábrica do ADC; leitura aproximada");
        s_cali = NULL;
    }
}

float battery_voltage(void)
{
    int sum_mv = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < SAMPLES; i++) {
        int raw = 0, mv = 0;
        adc_oneshot_read(s_adc, BATT_CHANNEL, &raw);
        if (s_cali)
            adc_cali_raw_to_voltage(s_cali, raw, &mv);
        else
            mv = raw * 3300 / 4095;
        sum_mv += mv;
    }
    xSemaphoreGive(s_mutex);
    return (float)sum_mv / SAMPLES / 1000.0f * BATT_DIVIDER;
}

int battery_percent(float v)
{
    // Curva aproximada de descarga de uma célula Li-ion sob carga leve.
    static const float volts[] = {3.30f, 3.50f, 3.60f, 3.70f, 3.75f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
    static const int pct[] = {0, 5, 10, 20, 30, 40, 60, 75, 90, 100};
    const int n = sizeof(volts) / sizeof(volts[0]);
    if (v <= volts[0])
        return 0;
    if (v >= volts[n - 1])
        return 100;
    for (int i = 1; i < n; i++) {
        if (v < volts[i]) {
            float x = (v - volts[i - 1]) / (volts[i] - volts[i - 1]);
            return pct[i - 1] + (int)(x * (pct[i] - pct[i - 1]));
        }
    }
    return 100;
}
