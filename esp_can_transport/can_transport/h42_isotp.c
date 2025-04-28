#include "isotp.h"
#include "isotp_defines.h"
#include "isotp_user.h"
#include <stdarg.h>
#include <stdio.h>

#include <driver/twai.h>
#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const char *TAG = "ISOTP";

void isotp_user_debug(const char *message, ...) {
  va_list args;
  va_start(args, message);
  esp_log_writev(ESP_LOG_WARN, TAG, message, args);
  va_end(args);
}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data,
                        const uint8_t size) {
  twai_message_t tx_message = {
      .extd = 1,
      .identifier = arbitration_id,
      .data_length_code = size,
  };

  uint32_t r = esp_random() & 0x1FE00000;
  tx_message.identifier |= r;

  memcpy(tx_message.data, data, size);
  esp_err_t err = twai_transmit(&tx_message, portMAX_DELAY);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to transmit CAN message (%d)", err);
    return ISOTP_RET_ERROR;
  }
  return ISOTP_RET_OK;
}

uint32_t isotp_user_get_us(void) {
  TickType_t t = xTaskGetTickCount();
  return pdTICKS_TO_MS(t) * 1000;
}
