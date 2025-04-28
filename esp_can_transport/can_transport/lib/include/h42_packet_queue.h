#pragma once

//#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct h42_packet_queue *h42_packet_queue_handle_t;

typedef struct h42_packet *h42_packet_handle_t;

//
// Packet Queue API
//
h42_packet_queue_handle_t h42_packet_queue_create(uint32_t max_packets,
                                                  uint32_t max_size_bytes);
void h42_packet_queue_destroy(h42_packet_queue_handle_t *queue);
// Takes ownership of the packet!
bool h42_packet_queue_push_acquire(h42_packet_queue_handle_t queue,
                                   h42_packet_handle_t *packet);
// Releases ownership of the packet! Caller must free the packet!
h42_packet_handle_t
h42_packet_queue_pop_release(h42_packet_queue_handle_t queue, int timeout_ms);
bool h42_packet_queue_wait_data_available(h42_packet_queue_handle_t queue,
                                          int timeout_ms);

//
// Packet API
//
h42_packet_handle_t h42_packet_alloc(uint32_t size);
bool h42_packet_append_data(h42_packet_handle_t packet, const uint8_t *data,
                            uint32_t size);
void h42_packet_free(h42_packet_handle_t *packet);
uint8_t *h42_packet_data(h42_packet_handle_t packet);
uint32_t h42_packet_size(h42_packet_handle_t packet);
uint32_t h42_packet_capacity(h42_packet_handle_t packet);

#ifdef __cplusplus
}
#endif