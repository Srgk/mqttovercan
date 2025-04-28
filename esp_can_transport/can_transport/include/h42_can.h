#pragma once

#include "h42_can_types.h"

#include <esp_err.h>
#include <esp_transport.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t h42_can_init();
esp_transport_handle_t h42_can_make_esp_transport();

#ifdef __cplusplus
}
#endif
