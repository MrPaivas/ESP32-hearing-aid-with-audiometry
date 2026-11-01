#include "input.h"

#include "board.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "freertos/task.h"

#define POLL_MS         5
#define DEBOUNCE_MS     20
#define DOUBLE_GAP_MS   300
#define LONG_PRESS_MS   3000
#define PCNT_LIMIT      1000
#define PCNT_RESET_AT   800

static QueueHandle_t s_queue;
static pcnt_unit_handle_t s_pcnt;

static void post(input_event_type_t type, int value)
{
    input_event_t ev = {.type = type, .value = value};
    xQueueSend(s_queue, &ev, 0);
}

static void encoder_init(void)
{
    pcnt_unit_config_t unit_cfg = {.low_limit = -PCNT_LIMIT, .high_limit = PCNT_LIMIT};
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &s_pcnt));
    pcnt_glitch_filter_config_t filter = {.max_glitch_ns = 1000};
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_pcnt, &filter));

    // Decodificação x4 (padrão do exemplo rotary_encoder do ESP-IDF).
    pcnt_chan_config_t a_cfg = {.edge_gpio_num = PIN_ENC_A, .level_gpio_num = PIN_ENC_B};
    pcnt_chan_config_t b_cfg = {.edge_gpio_num = PIN_ENC_B, .level_gpio_num = PIN_ENC_A};
    pcnt_channel_handle_t ch_a, ch_b;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt, &a_cfg, &ch_a));
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt, &b_cfg, &ch_b));
    pcnt_channel_set_edge_action(ch_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(ch_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(ch_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(ch_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // O PCNT não configura pull-ups; encoder mecânico fecha para GND.
    gpio_set_pull_mode(PIN_ENC_A, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_ENC_B, GPIO_PULLUP_ONLY);

    ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt));
    ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt));
}

bool input_button_pressed(void)
{
    return gpio_get_level(PIN_ENC_BTN) == 0;
}

static void input_task(void *arg)
{
    int last_count = 0;
    int residual = 0;

    bool stable = input_button_pressed();
    bool raw_prev = stable;
    int raw_ms = 0;
    int held_ms = 0;
    bool long_sent = false;
    int clicks = 0;
    int since_release_ms = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));

        // Encoder
        int count = 0;
        pcnt_unit_get_count(s_pcnt, &count);
        residual += count - last_count;
        last_count = count;
        if (count > PCNT_RESET_AT || count < -PCNT_RESET_AT) {
            pcnt_unit_clear_count(s_pcnt);
            last_count = 0;
        }
        int detents = residual / ENC_STEPS_PER_DETENT;
        if (detents != 0) {
            residual -= detents * ENC_STEPS_PER_DETENT;
            post(INPUT_ROTATE, detents);
        }

        // Botão com debounce
        bool raw = input_button_pressed();
        if (raw != raw_prev) {
            raw_prev = raw;
            raw_ms = 0;
        } else if (raw_ms < DEBOUNCE_MS) {
            raw_ms += POLL_MS;
        }
        if (raw_ms >= DEBOUNCE_MS && raw != stable) {
            stable = raw;
            if (stable) {
                held_ms = 0;
                long_sent = false;
                post(INPUT_BTN_DOWN, 0);
            } else if (!long_sent) {
                clicks++;
                since_release_ms = 0;
            }
        }

        if (stable) {
            held_ms += POLL_MS;
            if (!long_sent && held_ms >= LONG_PRESS_MS) {
                long_sent = true;
                clicks = 0;
                post(INPUT_LONG_PRESS, 0);
            }
        } else if (clicks > 0) {
            since_release_ms += POLL_MS;
            if (clicks >= 2) {
                post(INPUT_DOUBLE_CLICK, 0);
                clicks = 0;
            } else if (since_release_ms >= DOUBLE_GAP_MS) {
                post(INPUT_CLICK, 0);
                clicks = 0;
            }
        }
    }
}

void input_init(void)
{
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << PIN_ENC_BTN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);
    encoder_init();
    s_queue = xQueueCreate(16, sizeof(input_event_t));
    xTaskCreatePinnedToCore(input_task, "input", 3072, NULL, 5, NULL, 0);
}

QueueHandle_t input_queue(void)
{
    return s_queue;
}
