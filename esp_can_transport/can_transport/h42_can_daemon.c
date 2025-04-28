#include "h42_can_daemon.h"
#include "h42_can_types.h"
#include "h42_packet_queue.h"

#include "isotp.h"

#include <driver/twai.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <string.h>

/*
Arbitration ID format: (29 bits)
 | 8 bits random seed | 2 bits: unused | 3 bits: msg type | 8 bits: src address
| 8 bits: destination address |


  MSG_TYPE_ADDRESS_REQUEST:
    Send by a node to the master(0x00) to request address.
    Payload (6 bytes):
      6 bytes: node mac (chip id)

  MSG_TYPE_ADDRESS_RESPONSE:
    Broadcasted by the master(0x00) to announce a new node address.
    Payload (8 bytes):
      6 bytes: node mac (chip id)
      1 byte: status: 0 - success, 1 - failure
      1 byte: on success - new node address
*/

#define H42_CAN_ADDRESS_MASTER 0x00
#define H42_CAN_ADDRESS_BROADCAST 0xFF

typedef enum {
  MSG_TYPE_PACKET_ISOTP = 0,
  MSG_TYPE_ADDRESS_REQUEST = 5,
  MSG_TYPE_ADDRESS_RESPONSE = 6,
} h42_can_msg_type_t;

typedef enum {
  DAEMON_STATE_OBTAINING_ADDRESS = (1 << 0),
  DAEMON_STATE_SERVING = (1 << 1),
} h42_can_daemon_state_t;

#define ISOTP_BUFSIZE 4095

typedef struct h42_out_packet {
  uint8_t data[ISOTP_BUFSIZE];
  uint32_t size;
  TaskHandle_t sender;
} h42_out_packet_t;

typedef struct h42_can_daemon {
  EventGroupHandle_t state;
  h42_can_address_t address;
  h42_packet_queue_handle_t in_packet_queue;
  h42_packet_handle_t last_popped_packet;
  int last_popped_packet_read_pos;

  IsoTpLink isotp_link;
  uint8_t isotp_recv_internal_buf[ISOTP_BUFSIZE];
  uint8_t isotp_send_internal_buf[ISOTP_BUFSIZE];
  uint8_t isotp_recv_buf[ISOTP_BUFSIZE];
  uint8_t isotp_last_send_status;

  QueueHandle_t out_packet_queue;
} h42_can_daemon_t;
static h42_can_daemon_t g_daemon = {0};

static const char *TAG = "overcan-daemon";

static inline uint32_t _msg_make_id(h42_can_msg_type_t req_type,
                                    h42_can_address_t src_addr,
                                    h42_can_address_t dst_addr) {
  return ((req_type & 7) << 16) | (src_addr << 8) | dst_addr;
}

static inline h42_can_msg_type_t _msg_type(const twai_message_t *msg) {
  return (h42_can_msg_type_t)((msg->identifier >> 16) & 7);
}

static inline h42_can_address_t _msg_dst_addr(const twai_message_t *msg) {
  return (h42_can_address_t)(msg->identifier & 0xFF);
}

static inline h42_can_address_t _msg_src_addr(const twai_message_t *msg) {
  return (h42_can_address_t)((msg->identifier >> 8) & 0xFF);
}

static void _daemon_isotp_reset(h42_can_daemon_t *daemon) {
  isotp_init_link(&daemon->isotp_link, 0x000, daemon->isotp_send_internal_buf,
                  sizeof(daemon->isotp_send_internal_buf),
                  daemon->isotp_recv_internal_buf,
                  sizeof(daemon->isotp_recv_internal_buf));
  daemon->isotp_last_send_status = ISOTP_SEND_STATUS_IDLE;
}

/**
 * @brief Loop and try to obtain an address for this node
 */
static esp_err_t _daemon_obtain_address(h42_can_daemon_t *daemon) {
  uint8_t chip_id[8] = {0}; // only first 6 bytes matter
  esp_efuse_mac_get_default(&chip_id[0]);
  twai_message_t addr_request_msg = {
      .identifier =
          _msg_make_id(MSG_TYPE_ADDRESS_REQUEST, H42_CAN_ADDRESS_BROADCAST,
                       H42_CAN_ADDRESS_MASTER),
      .extd = 1,
      .data_length_code = 6,
  };
  memcpy(addr_request_msg.data, chip_id, 6);
  ESP_LOGI(TAG, "Obtaining address...");

  // Reset ISOTP link. If there were packets in flight, too bad.
  _daemon_isotp_reset(daemon);

  for (;;) {
    // Wait 0-250ms so we don't collide with other nodes if there was a
    // broadcast address reobtain request
    vTaskDelay(pdMS_TO_TICKS(esp_random() % 250));

    // Send address request
    esp_err_t err = twai_transmit(&addr_request_msg, portMAX_DELAY);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to transmit address request (%d). Pausing...", err);
      vTaskDelay(pdMS_TO_TICKS(10 * 1000));
      continue;
    }
    ESP_LOGI(TAG, "Address request sent");

    // Wait for address response
    twai_message_t rx_message;
    TickType_t resp_wait_start = xTaskGetTickCount();
    for (;;) {
      err = twai_receive(&rx_message, pdMS_TO_TICKS(1000));
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to receive address response (%d)", err);
      } else {
        if (rx_message.rtr || !rx_message.extd) {
          ESP_LOGW(TAG, "Undesired message received. RTR: %d, EXTD: %d",
                   rx_message.rtr, rx_message.extd);
          continue;
        }

        if (_msg_type(&rx_message) == MSG_TYPE_ADDRESS_RESPONSE &&
            _msg_dst_addr(&rx_message) == H42_CAN_ADDRESS_BROADCAST &&
            rx_message.data_length_code == 8 &&
            memcmp(rx_message.data, chip_id, 6) == 0) {
          if (rx_message.data[6] != 0) {
            ESP_LOGE(
                TAG,
                "Address request failed with status %d. Going to sleep for "
                "a while.",
                rx_message.data[6]);
            vTaskDelay(pdMS_TO_TICKS(30 * 1000));
            break;
          }

          // This is our new address
          daemon->address = rx_message.data[7];
          // Update ISOTP sender address as well
          daemon->isotp_link.send_arbitration_id = _msg_make_id(
              MSG_TYPE_PACKET_ISOTP, daemon->address, H42_CAN_ADDRESS_MASTER);

          ESP_LOGI(TAG, "Address received: %d", daemon->address);
          return ESP_OK;
        }
      }
      if (xTaskGetTickCount() - resp_wait_start > pdMS_TO_TICKS(3000)) {
        // Stop waiting for response and resend the request.
        break;
      }
    }
  }
  return ESP_OK; // Never reached.
}

static esp_err_t _daemon_on_packet_received(h42_can_daemon_t *daemon,
                                            uint32_t data_size) {
  h42_packet_handle_t pkt = h42_packet_alloc(data_size);
  if (pkt == NULL) {
    ESP_LOGE(TAG, "Failed to allocate packet");
    return ESP_ERR_NO_MEM;
  }
  h42_packet_append_data(pkt, daemon->isotp_recv_buf, data_size);
  if (!h42_packet_queue_push_acquire(daemon->in_packet_queue, &pkt)) {
    ESP_LOGE(TAG,
             "Failed to enqueue received packet. No one is listening to it?");
    h42_packet_free(&pkt);
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

static h42_can_daemon_state_t _daemon_get_state(h42_can_daemon_t *daemon) {
  return (h42_can_daemon_state_t)xEventGroupGetBits(daemon->state);
}

static void _daemon_set_state(h42_can_daemon_t *daemon,
                              h42_can_daemon_state_t state) {
  xEventGroupClearBits(daemon->state, 0x00FFFFFF);
  xEventGroupSetBits(daemon->state, (EventBits_t)state);
}

static bool _daemon_out_packet_queue_pop(h42_can_daemon_t *daemon,
                                         h42_out_packet_t *item) {
  return daemon->isotp_link.send_status != ISOTP_SEND_STATUS_INPROGRESS &&
         daemon->isotp_link.receive_status != ISOTP_RECEIVE_STATUS_INPROGRESS &&
         xQueueReceive(daemon->out_packet_queue, item, 0) == pdTRUE;
}

static void _out_packet_send_finish(h42_out_packet_t *out_packet,
                                    esp_err_t err) {
  if (out_packet->sender == NULL) {
    return;
  }
  if (xTaskNotify(out_packet->sender, (uint32_t)err,
                  eSetValueWithoutOverwrite) != pdPASS) {
    ESP_LOGE(TAG, "Failed to notify sender task");
  }
  out_packet->sender = NULL;
  ESP_LOGI(TAG, "Packet sent. size: %d, err: %d", (int)out_packet->size, err);
}

/**
 * h42_can_daemon_recv_packet
 *
 * @details We assume there is only one task that calls this function, so no
 * synchronization is needed.
 */
esp_err_t h42_can_daemon_recv(uint8_t *buf, uint32_t buf_size,
                              uint32_t *recv_size, int timeout_ms) {
  h42_can_daemon_t *daemon = &g_daemon;

  // Need next packet?
  if (daemon->last_popped_packet == NULL) {
    daemon->last_popped_packet =
        h42_packet_queue_pop_release(daemon->in_packet_queue, timeout_ms);
    if (daemon->last_popped_packet == NULL) {
      return ESP_ERR_TIMEOUT;
    }
    daemon->last_popped_packet_read_pos = 0;
  }

  int bytes_left_in_packet = h42_packet_size(daemon->last_popped_packet) -
                             daemon->last_popped_packet_read_pos;
  int bytes_to_copy =
      bytes_left_in_packet < buf_size ? bytes_left_in_packet : buf_size;
  memcpy(buf,
         h42_packet_data(daemon->last_popped_packet) +
             daemon->last_popped_packet_read_pos,
         bytes_to_copy);
  daemon->last_popped_packet_read_pos += bytes_to_copy;
  *recv_size = bytes_to_copy;

  if (daemon->last_popped_packet_read_pos ==
      h42_packet_size(daemon->last_popped_packet)) {
    h42_packet_free(&daemon->last_popped_packet);
    daemon->last_popped_packet_read_pos = 0;
  }
  return ESP_OK;
}

esp_err_t h42_can_daemon_send(const uint8_t *buf, uint32_t buf_size,
                              int timeout_ms) {
  h42_can_daemon_t *daemon = &g_daemon;
  if (_daemon_get_state(daemon) != DAEMON_STATE_SERVING) {
    return ESP_ERR_INVALID_STATE;
  }
  if (buf_size > ISOTP_BUFSIZE) {
    return ESP_ERR_INVALID_SIZE;
  }
  h42_out_packet_t *out_packet =
      (h42_out_packet_t *)malloc(sizeof(h42_out_packet_t));
  if (out_packet == NULL) {
    ESP_LOGE(TAG, "Failed to allocate send item");
    return ESP_ERR_NO_MEM;
  }

  out_packet->size = buf_size;
  out_packet->sender = xTaskGetCurrentTaskHandle();
  memcpy(out_packet->data, buf, buf_size);
  BaseType_t res = xQueueSend(daemon->out_packet_queue, out_packet,
                              pdMS_TO_TICKS(timeout_ms));
  free(out_packet);
  if (res != pdTRUE) {
    ESP_LOGE(TAG, "Failed to queue send item (%d)", res);
    return ESP_ERR_TIMEOUT;
  }

  // Wait for the send to complete
  uint32_t notif_val;
  // ESP_LOGI(TAG, "Waiting for send completion notification");
  if (xTaskNotifyWait(0, 0, &notif_val, portMAX_DELAY) != pdPASS) {
    ESP_LOGE(TAG, "Failed to wait for send completion notification");
    return ESP_FAIL;
  }
  return (esp_err_t)notif_val;
}

esp_err_t h42_can_daemon_connect(int timeout_ms) {
  h42_can_daemon_t *daemon = &g_daemon;
  _daemon_set_state(daemon, DAEMON_STATE_OBTAINING_ADDRESS);
  EventBits_t bits =
      xEventGroupWaitBits(daemon->state, DAEMON_STATE_SERVING, pdFALSE, pdTRUE,
                          pdMS_TO_TICKS(timeout_ms));
  return (bits & DAEMON_STATE_SERVING) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t h42_can_daemon_poll_read(int timeout_ms) {
  h42_can_daemon_t *daemon = &g_daemon;
  return h42_packet_queue_wait_data_available(daemon->in_packet_queue,
                                              timeout_ms)
             ? ESP_OK
             : ESP_ERR_TIMEOUT;
}

esp_err_t h42_can_daemon_poll_write(int timeout_ms) {
  h42_can_daemon_t *daemon = &g_daemon;

  // I don't know how to wait for a free slot in a queue without polling.
  TickType_t startTicks = xTaskGetTickCount();
  while (uxQueueSpacesAvailable(daemon->out_packet_queue) == 0) {
    if (xTaskGetTickCount() - startTicks > pdMS_TO_TICKS(timeout_ms)) {
      return ESP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return ESP_OK;
}

uint16_t h42_max_packet_size() { return ISOTP_BUFSIZE - 1; }

/**
 * vTaskCanTransportDaemonBusWatchdog
 *
 * @brief Watchdog task for the CAN bus
 */
static const char *WDTAG = "bus-watchdog";
static const uint32_t H42_TWAI_ALERT_FLAGS =
    TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_ERR_PASS |
    TWAI_ALERT_BELOW_ERR_WARN | TWAI_ALERT_ERR_ACTIVE | TWAI_ALERT_BUS_OFF;
void vTaskCanBusWatchdog(void *pvParameters) {
  ESP_LOGI(WDTAG, "Bus Watchdog task started");
  twai_reconfigure_alerts(H42_TWAI_ALERT_FLAGS, NULL);
  for (;;) {
    uint32_t alerts;
    twai_read_alerts(&alerts, portMAX_DELAY);
    if (alerts & TWAI_ALERT_ABOVE_ERR_WARN) {
      ESP_LOGI(WDTAG, "Above warning level");
    }
    if (alerts & TWAI_ALERT_ERR_PASS) {
      ESP_LOGI(WDTAG, "Entered Error Passive state");
    }
    if (alerts & TWAI_ALERT_ERR_ACTIVE) {
      ESP_LOGI(WDTAG, "Returned to Error Active state");
    }
    if (alerts & TWAI_ALERT_BELOW_ERR_WARN) {
      ESP_LOGI(WDTAG, "Below warning level");
    }
    if (alerts & TWAI_ALERT_BUS_OFF) {
      ESP_LOGI(WDTAG, "Bus Off state, initiating recovery");

      twai_reconfigure_alerts(TWAI_ALERT_BUS_RECOVERED, NULL);
      twai_initiate_recovery(); // Needs 128 occurrences of bus free signal
    }
    if (alerts & TWAI_ALERT_BUS_RECOVERED) {
      ESP_LOGI(WDTAG, "Bus Recovered");
      if (twai_start() != ESP_OK) {
        ESP_LOGE(WDTAG, "Failed to start TWAI after recovery");
      }
      twai_reconfigure_alerts(H42_TWAI_ALERT_FLAGS, NULL);
    }
  }
}

// ISOTP
void vTaskCanTransportDaemon(void *pvParameters) {
  esp_err_t err;
  twai_message_t rx_message;
  h42_can_daemon_t *daemon = (h42_can_daemon_t *)pvParameters;

  h42_out_packet_t send_item = {0};
  for (;;) {

    if (_daemon_get_state(daemon) == DAEMON_STATE_OBTAINING_ADDRESS) {
      err = _daemon_obtain_address(daemon);
      if (err == ESP_OK) {
        _daemon_set_state(daemon, DAEMON_STATE_SERVING);
      } else {
        // Never actually happens.
        ESP_LOGE(TAG, "Failed to get address (%d). Will wail a minute", err);
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
        continue;
      }
    }

    // Enter ISO-TP
    // Spin faster if we are sending
    uint32_t recv_timeout =
        daemon->isotp_link.send_status == ISOTP_SEND_STATUS_INPROGRESS ? 5 : 50;
    esp_err_t err = twai_receive(&rx_message, pdMS_TO_TICKS(recv_timeout));
    if (err == ESP_OK) {
      h42_can_address_t dst_address = _msg_dst_addr(&rx_message);
      if (_msg_src_addr(&rx_message) != H42_CAN_ADDRESS_MASTER) {
        // Only interested in messages from the master
        continue;
      }
      if (dst_address != daemon->address &&
          dst_address != H42_CAN_ADDRESS_BROADCAST) {
        // This message is for someone else.
        continue;
      }
      if (_msg_type(&rx_message) == MSG_TYPE_ADDRESS_REQUEST) {
        // Master asked us to obtain a new address
        _daemon_set_state(daemon, DAEMON_STATE_OBTAINING_ADDRESS);
        // If there is a packet in progress - it will fail.
        _out_packet_send_finish(&send_item, ESP_FAIL);
        continue;
      }
      if (dst_address == H42_CAN_ADDRESS_BROADCAST) {
        // Not expecting any other broadcast messages
        ESP_LOGW(TAG, "Received unexpected broadcast message that is not "
                      "address request");
        continue;
      }
      isotp_on_can_message(&daemon->isotp_link, rx_message.data,
                           rx_message.data_length_code);

    } else if (err == ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "CAN driver is not installed");
      vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }

    isotp_poll(&daemon->isotp_link);
    uint16_t out_size;
    int ret = isotp_receive(&daemon->isotp_link, daemon->isotp_recv_buf,
                            sizeof(daemon->isotp_recv_buf), &out_size);
    if (ret == ISOTP_RET_OK) {
      // We have received an ISOTP message!
      // debug stuff!!
      // uint16_t i = daemon->isotp_recv_buf[0] | (daemon->isotp_recv_buf[1]
      // <<8); ESP_LOGI(TAG, "Packet received. Size: %d", out_size); end of
      // debug stuff
      _daemon_on_packet_received(daemon, out_size);
    }

    if (daemon->isotp_last_send_status == ISOTP_SEND_STATUS_INPROGRESS &&
        daemon->isotp_link.send_status != ISOTP_SEND_STATUS_INPROGRESS) {
      // Transmission finished.
      esp_err_t esp_err =
          daemon->isotp_link.send_status == ISOTP_SEND_STATUS_IDLE ? ESP_OK
                                                                   : ESP_FAIL;
      if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send. send_protocol_result:%d",
                 daemon->isotp_link.send_protocol_result);
      }
      _out_packet_send_finish(&send_item, esp_err);
    }
    daemon->isotp_last_send_status = daemon->isotp_link.send_status;

    // Check if we have something to send
    if (_daemon_out_packet_queue_pop(daemon, &send_item)) {
      ret = isotp_send(&daemon->isotp_link, send_item.data, send_item.size);
      if (ret != ISOTP_RET_OK) {
        ESP_LOGE(TAG, "isotp_send failed (%d)", ret);
        _out_packet_send_finish(&send_item, ESP_FAIL);
      } else if (send_item.size < 8) {
        // Send is done in one packet.
        assert(daemon->isotp_link.send_status != ISOTP_SEND_STATUS_INPROGRESS);
        _out_packet_send_finish(&send_item, ESP_OK);
      }
    }
  }
  vTaskDelete(NULL);
}
/**
 * h42_can_daemon_start
 */
esp_err_t h42_can_daemon_start() {
  h42_can_daemon_t *daemon = &g_daemon;

  ESP_LOGI(TAG, "Starting CAN transport daemon");

  // Initialize in packet queue
  daemon->in_packet_queue = h42_packet_queue_create(32, 16 * 1024);
  if (daemon->in_packet_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create packet queue");
    return ESP_ERR_NO_MEM;
  }
  daemon->last_popped_packet = NULL;
  daemon->last_popped_packet_read_pos = 0;

  // Initialize state
  daemon->state = xEventGroupCreate();
  if (daemon->state == NULL) {
    ESP_LOGE(TAG, "Failed to create event group");
    return ESP_ERR_NO_MEM;
  }
  _daemon_set_state(daemon, DAEMON_STATE_OBTAINING_ADDRESS);

  // Initialize ISO-TP
  _daemon_isotp_reset(daemon);

  // Init send queue
  daemon->out_packet_queue = xQueueCreate(1, sizeof(h42_out_packet_t));
  if (daemon->out_packet_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create send queue");
    vTaskDelete(NULL);
  }

  ESP_LOGI(TAG, "Queue created. Item size: %d", sizeof(h42_out_packet_t));

  // Start the watchdog task
  if (xTaskCreate(vTaskCanBusWatchdog, "can_bus_watchdog", 4096, daemon, 5,
                  NULL) != pdPASS) {
    ESP_LOGE(TAG, "Failed start bus watchdog task");
    return ESP_ERR_NO_MEM;
  }

  // Start the daemon task
  if (xTaskCreate(vTaskCanTransportDaemon, "can_transport_daemon",
                  4096 + sizeof(h42_out_packet_t), daemon, 5, NULL) != pdPASS) {
    ESP_LOGE(TAG, "Failed start transport daemon task");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}
