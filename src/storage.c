#include "storage.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "storage";
static const char *NS = "ha";

void storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static bool load_blob(const char *key, void *buf, size_t size)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK)
        return false;
    size_t len = size;
    esp_err_t err = nvs_get_blob(h, key, buf, &len);
    nvs_close(h);
    return err == ESP_OK && len == size;
}

static void save_blob(const char *key, const void *buf, size_t size)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, key, buf, size);
        if (err == ESP_OK)
            err = nvs_commit(h);
        nvs_close(h);
    }
    if (err != ESP_OK)
        ESP_LOGE(TAG, "falha ao salvar %s: %s", key, esp_err_to_name(err));
}

bool storage_load_settings(ha_settings_t *s)
{
    if (load_blob("settings", s, sizeof(*s)) && s->version == HA_SETTINGS_VERSION) {
        ha_settings_sanitize(s);
        return true;
    }
    ha_settings_defaults(s);
    return false;
}

bool storage_load_audiogram(ha_audiogram_t *ag)
{
    if (load_blob("audiogram", ag, sizeof(*ag)))
        return true;
    ha_audiogram_clear(ag);
    return false;
}

void storage_save_settings(const ha_settings_t *s)
{
    save_blob("settings", s, sizeof(*s));
}

void storage_save_audiogram(const ha_audiogram_t *ag)
{
    save_blob("audiogram", ag, sizeof(*ag));
}
