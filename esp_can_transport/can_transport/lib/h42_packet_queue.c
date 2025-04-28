#include "h42_packet_queue.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdlib.h>
#include <string.h>

typedef struct h42_packet_queue {
  uint32_t max_size_bytes;
  uint32_t current_size_bytes;
  QueueHandle_t os_queue;
  SemaphoreHandle_t lock;
} h42_packet_queue_t;

typedef struct h42_packet {
  uint32_t size;
  uint32_t capacity;
  uint8_t *data;
} h42_packet_t;

h42_packet_queue_handle_t h42_packet_queue_create(uint32_t max_packets,
                                                  uint32_t max_size_bytes) {
  h42_packet_queue_handle_t queue = malloc(sizeof(h42_packet_queue_t));
  if (queue == NULL) {
    return NULL;
  }
  queue->max_size_bytes = max_size_bytes;
  queue->current_size_bytes = 0;
  queue->os_queue = xQueueCreate(max_packets, sizeof(h42_packet_t));
  if (queue->os_queue == NULL) {
    free(queue);
    return NULL;
  }
  queue->lock = xSemaphoreCreateMutex();
  if (queue->lock == NULL) {
    vQueueDelete(queue->os_queue);
    free(queue);
    return NULL;
  }

  return queue;
}

void h42_packet_queue_destroy(h42_packet_queue_handle_t *queue) {
  assert(queue != NULL && *queue != NULL);
  // Assumes no other tasks are using the queue.
  // Destroy all packets in the queue.
  h42_packet_handle_t packet = NULL;
  while ((packet = h42_packet_queue_pop_release(*queue, 0)) != NULL) {
    h42_packet_free(&packet);
  }
  vQueueDelete((*queue)->os_queue);
  vSemaphoreDelete((*queue)->lock);
  free(*queue);
  *queue = NULL;
}

bool h42_packet_queue_push_acquire(h42_packet_queue_handle_t queue,
                                   h42_packet_handle_t *packet) {
  assert(queue != NULL && packet != NULL && *packet != NULL);

  if (xSemaphoreTake(queue->lock, portMAX_DELAY) != pdTRUE) {
    return false;
  }
  if (queue->current_size_bytes + (*packet)->size > queue->max_size_bytes) {
    xSemaphoreGive(queue->lock);
    return false;
  }
  queue->current_size_bytes += (*packet)->size;
  xSemaphoreGive(queue->lock);

  if (xQueueSend(queue->os_queue, (const void *)*packet, 0) != pdTRUE) {
    return false;
  }
  free(*packet);
  *packet = NULL;
  return true;
}

h42_packet_handle_t
h42_packet_queue_pop_release(h42_packet_queue_handle_t queue, int timeout_ms) {
  h42_packet_handle_t packet = malloc(sizeof(h42_packet_t));
  if (packet == NULL) {
    return NULL;
  }

  if (xQueueReceive(queue->os_queue, (void *)packet,
                    pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
    free(packet);
    return NULL;
  }

  assert(xSemaphoreTake(queue->lock, portMAX_DELAY) == pdTRUE);
  queue->current_size_bytes -= packet->size;
  xSemaphoreGive(queue->lock);

  return packet;
}

bool h42_packet_queue_wait_data_available(h42_packet_queue_handle_t queue,
                                          int timeout_ms) {
  assert(queue != NULL);
  h42_packet_t dummy_packet;
  return xQueuePeek(queue->os_queue, &dummy_packet,
                    pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

h42_packet_handle_t h42_packet_alloc(uint32_t size) {
  h42_packet_handle_t packet = malloc(sizeof(h42_packet_t));
  if (packet == NULL) {
    return NULL;
  }
  packet->data = malloc(size);
  if (packet->data == NULL) {
    free(packet);
    return NULL;
  }
  packet->size = 0;
  packet->capacity = size;
  return packet;
}

bool h42_packet_append_data(h42_packet_handle_t packet, const uint8_t *data,
                            uint32_t size) {

  assert(packet != NULL && data != NULL);
  if (packet->size + size > packet->capacity) {
    return false;
  }
  memcpy(packet->data + packet->size, data, size);
  packet->size += size;
  return true;
}

void h42_packet_free(h42_packet_handle_t *packet) {
  assert(packet != NULL && *packet != NULL);
  free((*packet)->data);
  free(*packet);
  *packet = NULL;
}

uint8_t *h42_packet_data(h42_packet_handle_t packet) {
  assert(packet != NULL);
  return packet->data;
}

uint32_t h42_packet_size(h42_packet_handle_t packet) {
  assert(packet != NULL);
  return packet->size;
}

uint32_t h42_packet_capacity(h42_packet_handle_t packet) {
  assert(packet != NULL);
  return packet->capacity;
}
