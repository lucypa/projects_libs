/*
 * Copyright 2022, UNSW
 * University of New South Wales (UNSW)
 */

#pragma once 

#include <stdint.h>
#include <stddef.h>
#include <sel4utils/sel4_zf_logif.h>
#include <utils/util.h>
#include <shared_ringbuffer/gen_config.h>

//#define RING_SIZE 512 // TODO: Can we set this up so that the user defines the size

/* Function pointer to be used to 'notify' components on either end of the shared memory */
typedef void (*notify_fn)(void);

/* Buffer descriptor */
typedef struct buff_desc {
    uintptr_t encoded_addr; /* encoded dma addresses */
    unsigned int len; /* associated memory lengths */
    void *cookie; /* index into client side metadata */
} buff_desc_t;

/* Circular buffer containing descriptors */
typedef struct ring_buffer {
    buff_desc_t buffers[CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT]; 
    uint32_t write_idx;
    uint32_t read_idx;
} ring_buffer_t;

/* A ring handle for enqueing/dequeuing into  */
typedef struct ring_handle {
    ring_buffer_t *used_ring;
    ring_buffer_t *avail_ring;
    /* Function to be used to signal that work is queued in the used_ring */
    notify_fn notify;
} ring_handle_t;

void ring_init(ring_handle_t *ring, ring_buffer_t *avail, ring_buffer_t *used, notify_fn notify, int buffer_init);
void register_notification(ring_handle_t *ring, notify_fn notify);

/**
 * Check if the ring buffer is empty
 *
 * @param ring ring buffer to check
 *
 * @return true indicates the buffer is empty, false otherwise.
 */
static inline int ring_empty(ring_buffer_t *ring) 
{
    return !((ring->write_idx - ring->read_idx) % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT);
}

/**
 * Check if the ring buffer is full
 *
 * @param ring ring buffer to check
 *
 * @return true indicates the buffer is full, false otherwise.
 */
static inline int ring_full(ring_buffer_t *ring)
{
    return !((ring->write_idx - ring->read_idx + 1) % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT);
}

static inline void notify(ring_handle_t *ring) 
{
    return ring->notify();
}

/**
 * Enqueue an element to a ring buffer
 *
 * @param ring Ring buffer to enqueue into
 * @param buffer address into shared memory where data is stored.
 * @param len length of data inside the buffer above
 * @param cookie optional pointer to data required on dequeueing. 
 *
 * @return -1 when ring is empty, 0 on success
 */
static inline int enqueue(ring_buffer_t *ring, uintptr_t buffer, unsigned int len, void *cookie) 
{
    if (ring_full(ring)) {
        ZF_LOGE("Ring full");
        return -1; 
    }

    ring->buffers[ring->write_idx % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT].encoded_addr = buffer;
    ring->buffers[ring->write_idx % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT].len = len;
    ring->buffers[ring->write_idx % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT].cookie = cookie;
    ring->write_idx++;

    THREAD_MEMORY_RELEASE();

    return 0;
}

/**
 * Dequeue an element to a ring buffer
 *
 * @param ring Ring buffer to Dequeue from
 * @param buffer pointer to the address of where to store buffer address.
 * @param len pointer to variable to store length of data dequeueing
 * @param cookie pointer optional pointer to data required on dequeueing. 
 *
 * @return -1 when ring is empty, 0 on success
 */
static inline int dequeue(ring_buffer_t *ring, uintptr_t *addr, unsigned int *len, void **cookie) 
{
    if (ring_empty(ring)) {
        ZF_LOGF("Ring is empty");
        return -1;
    }

    *addr = ring->buffers[ring->read_idx % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT].encoded_addr;
    *len = ring->buffers[ring->read_idx % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT].len;
    *cookie = ring->buffers[ring->read_idx % CONFIG_LIB_SHARED_RINGBUFFER_DESC_COUNT].cookie;

    THREAD_MEMORY_RELEASE();
    ring->read_idx++;

    return 0;
}

/**
 * Enqueue an element into an available ring buffer
 * This indicates the buffer address parameter is currently available for use. 
 *
 * @param ring Ring handle to enqueue into
 * @param buffer address into shared memory where data is stored.
 * @param len length of data inside the buffer above
 * @param cookie optional pointer to data required on dequeueing. 
 *
 * @return -1 when ring is empty, 0 on success
 */
static inline int enqueue_avail(ring_handle_t *ring, uintptr_t addr, unsigned int len, void *cookie) 
{
    return enqueue(ring->avail_ring, addr, len, cookie);
}

/**
 * Enqueue an element into a used ring buffer 
 * This indicates the buffer address parameter is currently in use.
 *
 * @param ring Ring handle to enqueue into
 * @param buffer address into shared memory where data is stored.
 * @param len length of data inside the buffer above
 * @param cookie optional pointer to data required on dequeueing. 
 *
 * @return -1 when ring is empty, 0 on success
 */
static inline int enqueue_used(ring_handle_t *ring, uintptr_t addr, unsigned int len, void *cookie) 
{
    return enqueue(ring->used_ring, addr, len, cookie);
}

/**
 * Dequeue an element from an available ring buffer
 *
 * @param ring Ring handle to dequeue from
 * @param buffer pointer to the address of where to store buffer address.
 * @param len pointer to variable to store length of data dequeueing
 * @param cookie pointer optional pointer to data required on dequeueing. 
 *
 * @return -1 when ring is empty, 0 on success
 */
static inline int dequeue_avail(ring_handle_t *ring, uintptr_t *addr, unsigned int *len, void **cookie) 
{
    return dequeue(ring->avail_ring, addr, len, cookie);
}

/**
 * Dequeue an element from a used ring buffer
 *
 * @param ring Ring handle to dequeue from
 * @param buffer pointer to the address of where to store buffer address.
 * @param len pointer to variable to store length of data dequeueing
 * @param cookie pointer optional pointer to data required on dequeueing. 
 *
 * @return -1 when ring is empty, 0 on success
 */
static inline int dequeue_used(ring_handle_t *ring, uintptr_t *addr, unsigned int *len, void **cookie) 
{
    return dequeue(ring->used_ring, addr, len, cookie);
}
