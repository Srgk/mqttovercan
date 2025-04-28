/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "h42_can.h"
#include "h42_can_daemon.h"
#include "sdkconfig.h"
#include <driver/twai.h>
#include <esp_log.h>
#include <stdio.h>

// sender task
void sender_task(void *pvParameters) {
  uint8_t data[512];
  for (int i = 0; i < sizeof(data); i++) {
    data[i] = i % 256;
  }
  printf("Waiting to connect...\n");
  esp_err_t err = h42_can_daemon_connect(1000 * 1000);
  if (err != ESP_OK) {
    printf("Failed to connect - %d\n", err);
    vTaskDelete(NULL);
  }
  printf("Sending...\n");
  for (int i = 0; i < 10000; i++) {
    data[0] = i & 0xFF;
    data[1] = i >> 8;
    esp_err_t err = h42_can_daemon_send(data, sizeof(data), 1000 * 1000);
    if (err != ESP_OK) {
      printf("Failed to send data - %d (%d)\n", i, err);
      if (err == ESP_ERR_INVALID_STATE) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    } else {
      printf("Data sent - %d\n", i);
      vTaskDelay(pdMS_TO_TICKS(esp_random() % 250));
    }
  }

  printf("==== Sender task done ====\n");
  vTaskDelete(NULL);
}

void app_main(void) {
  printf("========= Hello world! ============\n");

  uint8_t mac[8] = {0};
  esp_efuse_mac_get_default(&mac[0]);
  printf("Chip ID: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1],
         mac[2], mac[3], mac[4], mac[5], mac[6], mac[7]);

  if (h42_can_init() != ESP_OK) {
    printf("Failed to initialize CAN transport\n");
  }

  // start sender task
  xTaskCreate(sender_task, "sender_task", 4096, NULL, 1, NULL);

  for (int i = 100; i >= 0; i--) {
    // printf("Restarting in %d seconds...\n", i * 20);
    vTaskDelay(pdMS_TO_TICKS(20000));
  }
  printf("Restarting now.\n");
  fflush(stdout);
  esp_restart();
}
