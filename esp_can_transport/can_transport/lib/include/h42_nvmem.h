#pragma once

#include <esp_err.h>

esp_err_t h42_nvmem_save_i32(const char *key, int32_t value);
esp_err_t h42_nvmem_load_i32(const char *key, int32_t *value);
esp_err_t h42_nvmem_erase(const char *key);


