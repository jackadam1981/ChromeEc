/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * Simple FIFO or Circular Buffer
 *  - Implementation: mirroring method
 *  - Max size: 128 elements
 *  - supports the following usages:
 *       enque 1 byte at time; dequeue 1 byte at a time.
 *       enque 4 byte at time; dequeue 4 byte at a time.
 *       enque 1 byte at time; dequeue 4 byte at a time.
 *       enque 4 byte at time; dequeue 1 byte at a time.
 */
#ifndef __FIFO128_H__
#define __FIFO128_H__

#include <stdint.h>
#include <stdio.h>

#define FIFO_SIZE_128 128
#define FIFO_SIZE_MASK (FIFO_SIZE_128 - 1)

#define FIFO_SIZE_D8 FIFO_SIZE_128
#define FIFO_SIZE_D8_MASK (FIFO_SIZE_D8 - 1)

#define FIFO_SIZE_D32 (FIFO_SIZE_128 >> 2)
#define FIFO_SIZE_D32_MASK (FIFO_SIZE_D32 - 1)

struct fifo128 {
	union {
		uint32_t d32[FIFO_SIZE_D32];
		uint8_t d8[FIFO_SIZE_D8];
	};
	uint8_t read_ptr;  /**< read pointer */
	uint8_t write_ptr; /**< write pointer */
};

/* Init FIFO128 read and writer pointer to 0 */
void fifo128_init(struct fifo128 *que);

/* @return true if the fifo is full (128 elements)*/
int fifo128_is_full(struct fifo128 *que);

/* @return true if the fifo is empty */
int fifo128_is_empty(struct fifo128 *que);

/* @size  return the total number of elements in the fifo */
int fifo128_get_size(struct fifo128 *que);

/*
 * enque 1 byte into fifo
 * @return is_full :  0 on scucess; 1 on failure.
 */
int fifo128_enque8(struct fifo128 *que, uint8_t k);

/* peek the fifo at ith location; unit = 1 byte */
uint8_t *fifo128_at8(struct fifo128 *que, int ith);

/* peek the top 1-byte element of the fifo */
uint8_t *fifo128_top8(struct fifo128 *que);

/*
 * Deque 1 byte from the fifo
 * @return num of bytes dequed (0 or 1); return 1 on success.
 */
int fifo128_deque8(struct fifo128 *que, uint8_t *pK);

/*
 * enqueue 4-byte into the fifo
 * @return is_full :  0 on scucess; 1 on failure.
 */
int fifo128_enque32(struct fifo128 *que, uint32_t k);

/* peek the fifo at ith location; unit = 4 byte */
uint32_t *fifo128_at32(struct fifo128 *que, int ith);

/* peek the top 4-byte element of the fifo */
uint32_t *fifo128_top32(struct fifo128 *que);

/* Dequeue 4-byte from the fifo
 * @return num of bytes dequed (0 or 4); return 4 on success.
 */
int fifo128_deque32(struct fifo128 *que, uint32_t *pK);

#endif
