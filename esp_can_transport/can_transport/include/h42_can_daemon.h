#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "h42_can_types.h"
#include <esp_err.h>

esp_err_t h42_can_daemon_start();
esp_err_t h42_can_daemon_recv(uint8_t *buf, uint32_t buf_size,
                                  uint32_t *recv_size, int timeout_ms);
esp_err_t h42_can_daemon_send(const uint8_t *buf, uint32_t buf_size,
                                  int timeout_ms);
esp_err_t h42_can_daemon_connect(int timeout_ms);
// Needed for esp_transport
esp_err_t h42_can_daemon_poll_read(int timeout_ms);
esp_err_t h42_can_daemon_poll_write(int timeout_ms);

uint16_t h42_max_packet_size();
#ifdef __cplusplus
}
#endif