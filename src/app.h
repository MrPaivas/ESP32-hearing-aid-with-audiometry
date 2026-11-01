// Estado global da aplicação (configurações + audiograma) protegido por mutex.
#pragma once

#include "ha_settings.h"
#include "ha_types.h"

void app_state_init(void);

// Acesso exclusivo: app_lock() retorna ponteiros válidos até app_unlock().
void app_lock(ha_settings_t **s, ha_audiogram_t **ag);
void app_unlock(void);

// Recalcula a prescrição e envia ao engine (não bloqueia o áudio).
void app_apply_fitting(void);
void app_set_mute(bool mute);
bool app_is_muted(void);

// Persistência. save_settings_later agrupa gravações (ex.: girar o volume).
void app_save_settings(void);
void app_save_settings_later(void);
void app_save_audiogram(void);
void app_poll_deferred_save(void);
