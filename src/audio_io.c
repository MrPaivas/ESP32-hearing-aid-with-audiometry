#include "audio_io.h"

#include "board.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "ha_types.h"

static const char *TAG = "audio_io";

static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;

esp_err_t audio_io_init(int sample_rate)
{
    gpio_config_t mute_cfg = {
        .pin_bit_mask = 1ULL << PIN_DAC_XSMT,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&mute_cfg);
    audio_io_mute(true);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = I2S_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = HA_BLOCK_SIZE;
    chan_cfg.auto_clear = true; // em underrun o DAC recebe silêncio, não lixo
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, &s_rx), TAG, "i2s_new_channel");

    // INMP441 exige 64 BCLK por quadro (slots de 32 bits); o PCM5102A gera o
    // clock interno a partir do BCLK (pino SCK em GND).
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = PIN_I2S_BCLK,
                .ws = PIN_I2S_WS,
                .dout = PIN_I2S_DOUT,
                .din = PIN_I2S_DIN,
                .invert_flags = {0},
            },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg), TAG, "init tx");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std_cfg), TAG, "init rx");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "enable tx");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx), TAG, "enable rx");
    return ESP_OK;
}

void audio_io_mute(bool mute)
{
    gpio_set_level(PIN_DAC_XSMT, mute ? 0 : 1);
}

void audio_io_read(int32_t *buf, size_t frames)
{
    size_t got = 0;
    i2s_channel_read(s_rx, buf, frames * 2 * sizeof(int32_t), &got, portMAX_DELAY);
}

void audio_io_write(const int32_t *buf, size_t frames)
{
    size_t done = 0;
    i2s_channel_write(s_tx, buf, frames * 2 * sizeof(int32_t), &done, portMAX_DELAY);
}
