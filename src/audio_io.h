// I2S full-duplex: quadros estéreo intercalados de 32 bits.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t audio_io_init(int sample_rate);
void audio_io_mute(bool mute);
// Bloqueiam até completar `frames` quadros estéreo.
void audio_io_read(int32_t *buf, size_t frames);
void audio_io_write(const int32_t *buf, size_t frames);
