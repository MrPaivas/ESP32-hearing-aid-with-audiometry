#include "leds.h"

#include "board.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static volatile bool s_base;
static esp_timer_handle_t s_flash_timer;

static void led_write(bool on)
{
    gpio_set_level(PIN_LED, on ? 1 : 0);
}

static void flash_done(void *arg)
{
    led_write(s_base);
}

void leds_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_LED,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    led_write(false);
    esp_timer_create_args_t args = {.callback = flash_done, .name = "led"};
    esp_timer_create(&args, &s_flash_timer);
}

void leds_set(bool on)
{
    s_base = on;
    if (!esp_timer_is_active(s_flash_timer))
        led_write(on);
}

void leds_flash(int ms)
{
    esp_timer_stop(s_flash_timer);
    led_write(!s_base);
    esp_timer_start_once(s_flash_timer, (uint64_t)ms * 1000);
}

void leds_blink(int times, int on_ms, int off_ms)
{
    esp_timer_stop(s_flash_timer);
    for (int i = 0; i < times; i++) {
        led_write(true);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        led_write(false);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
    led_write(s_base);
}
