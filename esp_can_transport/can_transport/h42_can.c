#include "h42_can.h"

#include "h42_can_daemon.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/twai.h>
#include <nvs_flash.h>

#define CAN_TRANSPORT_SPEED TWAI_TIMING_CONFIG_20KBITS()
#define CAN_TRANSPORT_TX_GPIO 21
#define CAN_TRANSPORT_RX_GPIO 20

typedef struct h42_can_transport {
  h42_can_address_t address;
  bool initialized;
  // esp_transport_handle_t esp_transport;
} h42_can_transport_t;
static h42_can_transport_t g_can_transport = {0};

/**
 * @note In msg_id and mask the lower 11 bits matter. Don't shift left, the
 * function will do it.
 */
static esp_err_t can_transport_twai_driver_install() {
  const twai_timing_config_t t_config = CAN_TRANSPORT_SPEED;

  const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
      CAN_TRANSPORT_TX_GPIO, CAN_TRANSPORT_RX_GPIO, TWAI_MODE_NORMAL);

  return twai_driver_install(&g_config, &t_config, &f_config);
}

static int can_transport_connect(esp_transport_handle_t t, const char *host,
                                 int port, int timeout_ms) {
  // Wait until the OverCAN daemon has obtained an address
  esp_err_t res = h42_can_daemon_connect(timeout_ms);
  return res == ESP_OK ? 0 : -1;
}

static int can_transport_write(esp_transport_handle_t t, const char *buffer,
                               int len, int timeout_ms) {
  len = len > h42_max_packet_size() ? h42_max_packet_size() : len;
  if (h42_can_daemon_send((const uint8_t *)buffer, len, timeout_ms) != ESP_OK) {
    return -1;
  }
  return len;
}

static int can_transport_read(esp_transport_handle_t t, char *buffer, int len,
                              int timeout_ms) {
  uint32_t recv_len = 0;
  esp_err_t err =
      h42_can_daemon_recv((uint8_t *)buffer, len, &recv_len, timeout_ms);
  if (err == ESP_ERR_TIMEOUT) {
    // esp mqtt wants 0 if timeout.
    return 0;
  }
  if (err != ESP_OK) {
    return -1;
  }
  return recv_len;
}

static int can_transport_poll_read(esp_transport_handle_t t, int timeout_ms) {
  return h42_can_daemon_poll_read(timeout_ms) == ESP_OK ? 1 : 0;
}

static int can_transport_poll_write(esp_transport_handle_t t, int timeout_ms) {
  return h42_can_daemon_poll_write(timeout_ms) == ESP_OK ? 1 : 0;
}

static int can_transport_destroy(esp_transport_handle_t t) {
  // TODO: Implement counter not allowing more than one instance of the
  // transport

  // Do nothing
  return 0;
}

esp_err_t h42_can_init() {
  esp_err_t err;
  h42_can_transport_t *t = &g_can_transport;

  if (t->initialized) {
    // Already initialized
    return ESP_OK;
  }

  // Init TWAI
  err = can_transport_twai_driver_install();
  if (err != ESP_OK) {
    goto error;
  }
  err = twai_start();
  if (err != ESP_OK) {
    goto error;
  }

  // Start the CAN transport daemon
  err = h42_can_daemon_start();
  if (err != ESP_OK) {
    goto error;
  }

  t->initialized = true;

  return ESP_OK;
error:
  twai_stop();
  twai_driver_uninstall();
  return err;
}

esp_transport_handle_t h42_can_make_esp_transport() {
  // Initialize ESP transport
  esp_transport_handle_t t = esp_transport_init();
  if (t == NULL) {
    return NULL;
  }

  // Set transport functions
  esp_transport_set_func(t, can_transport_connect, can_transport_read,
                         can_transport_write, NULL, can_transport_poll_read,
                         can_transport_poll_write, can_transport_destroy);

  return t;
}
