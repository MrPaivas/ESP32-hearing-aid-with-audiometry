// Persistência em NVS.
#pragma once

#include "ha_settings.h"
#include "ha_types.h"

void storage_init(void);
// Retornam false (e preenchem padrões) se não houver dado válido salvo.
bool storage_load_settings(ha_settings_t *s);
bool storage_load_audiogram(ha_audiogram_t *ag);
void storage_save_settings(const ha_settings_t *s);
void storage_save_audiogram(const ha_audiogram_t *ag);
