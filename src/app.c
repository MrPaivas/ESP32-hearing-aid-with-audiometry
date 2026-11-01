#include "app.h"

#include "audio_engine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fitting.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage.h"

static const char *TAG = "app";

#define DEFERRED_SAVE_US (3 * 1000 * 1000)

static SemaphoreHandle_t s_mutex;
static ha_settings_t s_settings;
static ha_audiogram_t s_audiogram;
static bool s_muted;
static int64_t s_save_due_us; // 0 = nada pendente

void app_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!storage_load_settings(&s_settings))
        ESP_LOGI(TAG, "configurações padrão");
    if (!storage_load_audiogram(&s_audiogram))
        ESP_LOGI(TAG, "sem audiograma salvo");
}

void app_lock(ha_settings_t **s, ha_audiogram_t **ag)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s)
        *s = &s_settings;
    if (ag)
        *ag = &s_audiogram;
}

void app_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

void app_apply_fitting(void)
{
    ha_dsp_params_t p;
    app_lock(NULL, NULL);
    fitting_build_params(&s_audiogram, &s_settings, s_muted, &p);
    app_unlock();
    engine_set_aid_params(&p);
}

void app_set_mute(bool mute)
{
    s_muted = mute;
    app_apply_fitting();
}

bool app_is_muted(void)
{
    return s_muted;
}

void app_save_settings(void)
{
    ha_settings_t copy;
    app_lock(NULL, NULL);
    copy = s_settings;
    s_save_due_us = 0;
    app_unlock();
    storage_save_settings(&copy);
}

void app_save_settings_later(void)
{
    s_save_due_us = esp_timer_get_time() + DEFERRED_SAVE_US;
}

void app_poll_deferred_save(void)
{
    if (s_save_due_us != 0 && esp_timer_get_time() >= s_save_due_us)
        app_save_settings();
}

void app_save_audiogram(void)
{
    ha_audiogram_t copy;
    app_lock(NULL, NULL);
    copy = s_audiogram;
    app_unlock();
    storage_save_audiogram(&copy);
}
