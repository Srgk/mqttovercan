#include "h42_nvmem.h"

#include <nvs_flash.h>

static const char *NV_NAMESPACE = "overcan";

esp_err_t h42_nvmem_save_i32(const char *key, int32_t value) {
  nvs_handle_t nvs_handle;
  esp_err_t err;

  err = nvs_open(NV_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_i32(nvs_handle, key, value);
  if (err != ESP_OK) {
    nvs_close(nvs_handle);
    return err;
  }

  err = nvs_commit(nvs_handle);
  if (err != ESP_OK) {
    nvs_close(nvs_handle);
    return err;
  }

  nvs_close(nvs_handle);
  return ESP_OK;
}

esp_err_t h42_nvmem_load_i32(const char *key, int32_t *value) {
  nvs_handle_t nvs_handle;
  esp_err_t err;

  err = nvs_open(NV_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_get_i32(nvs_handle, key, value);
  if (err != ESP_OK) {
    nvs_close(nvs_handle);
    return err;
  }

  nvs_close(nvs_handle);
  return ESP_OK;
}

esp_err_t h42_nvmem_erase(const char *key) {
  nvs_handle_t nvs_handle;
  esp_err_t err;

  err = nvs_open(NV_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_erase_key(nvs_handle, key);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(nvs_handle);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    nvs_close(nvs_handle);
    return err;
  }

  err = nvs_commit(nvs_handle);
  if (err != ESP_OK) {
    nvs_close(nvs_handle);
    return err;
  }

  nvs_close(nvs_handle);
  return ESP_OK;
}