// ESP32 Hearing Aid with Audiometry
//
// Controles:
//   girar encoder ........ volume (passos de 2 dB)
//   clique ............... mudo liga/desliga
//   duplo clique ......... nível da bateria nos LEDs (1 a 4 piscadas)
//   segurar 3 s .......... inicia a audiometria (durante o teste: aborta)
//   segurar ao ligar ..... modo configuração (Wi-Fi + página web)
// Na audiometria, aperte o botão sempre que ouvir o tom.
//
// NÃO é um dispositivo médico. Veja docs/hardware.md.

#include "app.h"
#include "audio_engine.h"
#include "audiometry_runner.h"
#include "battery.h"
#include "board.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input.h"
#include "leds.h"
#include "storage.h"
#include "webui.h"

static const char *TAG = "main";

#define VOLUME_STEP_DB        2.0f
#define VOLUME_MIN_DB         (-30.0f)
#define BATTERY_CHECK_US      (10 * 1000 * 1000)
#define LOW_BATT_WARN_US      (60 * 1000 * 1000)
#define STATS_LOG_US          (10 * 1000 * 1000)
#define FIRST_RUN_DELAY_MS    3000

static void change_volume(int detents)
{
    ha_settings_t *s;
    app_lock(&s, NULL);
    float v = s->volume_db + VOLUME_STEP_DB * detents;
    bool at_limit = false;
    if (v >= s->volume_max_db) {
        v = s->volume_max_db;
        at_limit = true;
    }
    if (v <= VOLUME_MIN_DB) {
        v = VOLUME_MIN_DB;
        at_limit = true;
    }
    s->volume_db = v;
    app_unlock();

    app_apply_fitting();
    app_save_settings_later();
    if (at_limit)
        leds_flash(80);
    ESP_LOGI(TAG, "volume %.0f dB", v);
}

static void show_battery(void)
{
    int pct = battery_percent(battery_voltage());
    int blinks = pct > 75 ? 4 : pct > 50 ? 3 : pct > 25 ? 2 : 1;
    leds_blink(blinks, 250, 250);
}

static void handle_event(const input_event_t *ev)
{
    if (audiometry_running()) {
        if (ev->type == INPUT_BTN_DOWN)
            audiometry_notify_press();
        else if (ev->type == INPUT_LONG_PRESS)
            audiometry_abort();
        return;
    }

    switch (ev->type) {
    case INPUT_ROTATE:
        change_volume(ev->value);
        break;
    case INPUT_CLICK:
        app_set_mute(!app_is_muted());
        // Mudo: 2 piscadas curtas. Som ligado: 1 piscada longa.
        if (app_is_muted())
            leds_blink(2, 100, 100);
        else
            leds_flash(400);
        ESP_LOGI(TAG, "%s", app_is_muted() ? "mudo" : "som ligado");
        break;
    case INPUT_DOUBLE_CLICK:
        show_battery();
        break;
    case INPUT_LONG_PRESS:
        audiometry_start(false);
        break;
    default:
        break;
    }
}

static void check_battery(int64_t now, int64_t *last_warn)
{
    if (!BATT_MONITOR_ENABLED)
        return;
    float v = battery_voltage();
    if (v < 2.5f)
        return; // abaixo do corte da proteção: divisor ausente/mal ligado
    if (v < BATT_CRITICAL_V) {
        ESP_LOGW(TAG, "bateria crítica (%.2f V): desligando", v);
        audiometry_abort();
        engine_set_mode(ENGINE_SILENT);
        leds_blink(5, 100, 100);
        esp_deep_sleep_start();
    }
    if (v < BATT_LOW_V && now - *last_warn > LOW_BATT_WARN_US) {
        *last_warn = now;
        ESP_LOGW(TAG, "bateria baixa (%.2f V)", v);
        if (!audiometry_running())
            leds_blink(3, 100, 100);
    }
}

void app_main(void)
{
    storage_init();
    leds_init();
    input_init();
    battery_init();
    app_state_init();

    // Botão pressionado no boot = modo configuração.
    vTaskDelay(pdMS_TO_TICKS(50));
    bool config_mode = input_button_pressed();

    engine_start();
    app_apply_fitting();
    engine_set_mode(ENGINE_AID);

    if (config_mode) {
        leds_set(true);
        webui_start();
        // Descarta eventos gerados pelo botão segurado no boot.
        vTaskDelay(pdMS_TO_TICKS(500));
        while (input_button_pressed())
            vTaskDelay(pdMS_TO_TICKS(20));
        vTaskDelay(pdMS_TO_TICKS(400));
        xQueueReset(input_queue());
        leds_set(false);
        leds_blink(3, 100, 100);
    } else {
        ha_audiogram_t *ag;
        app_lock(NULL, &ag);
        bool has_audiogram = ha_audiogram_has_data(ag, HA_EAR_LEFT) || ha_audiogram_has_data(ag, HA_EAR_RIGHT);
        app_unlock();
        if (!has_audiogram) {
            ESP_LOGI(TAG, "primeiro uso: iniciando audiometria");
            vTaskDelay(pdMS_TO_TICKS(FIRST_RUN_DELAY_MS));
            audiometry_start(false);
        }
    }

    int64_t last_batt = 0, last_warn = -LOW_BATT_WARN_US, last_stats = 0;
    for (;;) {
        input_event_t ev;
        if (xQueueReceive(input_queue(), &ev, pdMS_TO_TICKS(200)))
            handle_event(&ev);

        int64_t now = esp_timer_get_time();
        app_poll_deferred_save();
        if (now - last_batt > BATTERY_CHECK_US) {
            last_batt = now;
            check_battery(now, &last_warn);
        }
        if (now - last_stats > STATS_LOG_US) {
            last_stats = now;
            engine_stats_t st;
            engine_get_stats(&st);
            ESP_LOGI(TAG, "DSP %lu/%lu us (média/máx) de 1000 us; mic slot0 %.1f dBFS, slot1 %.1f dBFS",
                     (unsigned long)st.proc_us_avg, (unsigned long)st.proc_us_max, st.mic_rms_dbfs[0],
                     st.mic_rms_dbfs[1]);
        }
    }
}
