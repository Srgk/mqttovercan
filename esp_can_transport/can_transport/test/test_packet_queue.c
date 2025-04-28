#include <unity.h>

#include "h42_packet_queue.h"
#include <stdio.h>

TEST_CASE("test_create_destroy", "[packet_queue]") {
  h42_packet_queue_handle_t queue = h42_packet_queue_create(10, 100);
  TEST_ASSERT_NOT_NULL(queue);
  h42_packet_queue_destroy(&queue);
  TEST_ASSERT_NULL(queue);
}

TEST_CASE("test_packet", "[packet_queue]") {
  h42_packet_handle_t packet = h42_packet_alloc(10);
  TEST_ASSERT_NOT_NULL(packet);
  TEST_ASSERT_EQUAL(10, h42_packet_capacity(packet));
  TEST_ASSERT_EQUAL(0, h42_packet_size(packet));

  TEST_ASSERT_TRUE(h42_packet_append_data(packet, (const uint8_t *)"hello", 5));
  TEST_ASSERT_EQUAL(5, h42_packet_size(packet));

  TEST_ASSERT_FALSE(
      h42_packet_append_data(packet, (const uint8_t *)"banana", 6));
  TEST_ASSERT_EQUAL(5, h42_packet_size(packet));

  TEST_ASSERT_EQUAL_STRING_LEN("hello", h42_packet_data(packet), 5);

  h42_packet_free(&packet);
  TEST_ASSERT_NULL(packet);
}

TEST_CASE("test_push_pop", "[packet_queue]") {
  h42_packet_queue_handle_t queue = h42_packet_queue_create(10, 100);
  TEST_ASSERT_NOT_NULL(queue);

  h42_packet_handle_t packet = h42_packet_alloc(10);
  TEST_ASSERT_NOT_NULL(packet);

  TEST_ASSERT_TRUE(h42_packet_append_data(packet, (const uint8_t *)"hello", 5));

  TEST_ASSERT_TRUE(h42_packet_queue_push_acquire(queue, &packet));
  TEST_ASSERT_NULL(packet);

  h42_packet_handle_t popped = h42_packet_queue_pop_release(queue, 100);
  TEST_ASSERT_NOT_NULL(popped);
  TEST_ASSERT_EQUAL_STRING_LEN("hello", h42_packet_data(popped), 5);

  h42_packet_free(&popped);
  h42_packet_queue_destroy(&queue);
}

TEST_CASE("test_queue_underflow", "[packet_queue]") {
  h42_packet_queue_handle_t queue = h42_packet_queue_create(10, 100);
  TEST_ASSERT_NOT_NULL(queue);

  h42_packet_handle_t popped = h42_packet_queue_pop_release(queue, 100);
  TEST_ASSERT_NULL(popped);

  h42_packet_queue_destroy(&queue);
}

TEST_CASE("test_queue_overflow_bytes", "[packet_queue]") {
  // 10 items, 10 bytes in total
  h42_packet_queue_handle_t queue = h42_packet_queue_create(10, 10);
  TEST_ASSERT_NOT_NULL(queue);

  h42_packet_handle_t packet = h42_packet_alloc(8);
  TEST_ASSERT_TRUE(
      h42_packet_append_data(packet, (const uint8_t *)"hello123", 8));

  TEST_ASSERT_TRUE(h42_packet_queue_push_acquire(queue, &packet));

  packet = h42_packet_alloc(5);
  TEST_ASSERT_TRUE(h42_packet_append_data(packet, (const uint8_t *)"world", 5));
  TEST_ASSERT_NOT_NULL(packet);

  TEST_ASSERT_FALSE(h42_packet_queue_push_acquire(queue, &packet));
  TEST_ASSERT_NOT_NULL(packet);

  h42_packet_free(&packet);
  h42_packet_queue_destroy(&queue);
}

TEST_CASE("test_queue_overflow_items", "[packet_queue]") {
  h42_packet_queue_handle_t queue = h42_packet_queue_create(2, 100);
  TEST_ASSERT_NOT_NULL(queue);

  h42_packet_handle_t packet = h42_packet_alloc(5);
  h42_packet_append_data(packet, (const uint8_t *)"world", 5);
  TEST_ASSERT_TRUE(h42_packet_queue_push_acquire(queue, &packet));

  packet = h42_packet_alloc(5);
  h42_packet_append_data(packet, (const uint8_t *)"world", 5);
  TEST_ASSERT_TRUE(h42_packet_queue_push_acquire(queue, &packet));

  packet = h42_packet_alloc(5);
  h42_packet_append_data(packet, (const uint8_t *)"world", 5);
  TEST_ASSERT_FALSE(h42_packet_queue_push_acquire(queue, &packet));
  TEST_ASSERT_NOT_NULL(packet);
  h42_packet_free(&packet);

  h42_packet_queue_destroy(&queue);
}

TEST_CASE("test_queue_wait_data", "[packet_queue]") {
  h42_packet_queue_handle_t queue = h42_packet_queue_create(2, 100);
  TEST_ASSERT_NOT_NULL(queue);

  TEST_ASSERT_FALSE(h42_packet_queue_wait_data_available(queue, 10));

  h42_packet_handle_t packet = h42_packet_alloc(5);
  h42_packet_append_data(packet, (const uint8_t *)"world", 5);
  TEST_ASSERT_TRUE(h42_packet_queue_push_acquire(queue, &packet));

  TEST_ASSERT_TRUE(h42_packet_queue_wait_data_available(queue, 10));

  h42_packet_queue_destroy(&queue);
}
