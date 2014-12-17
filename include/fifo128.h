/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * Simplo FIFO or Circular Buffer
 *  - Implementation: mirroring method
 *  - Max size: 128 elements
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

void fifo128_init(struct fifo128 *que);
int fifo128_is_full(struct fifo128 *que);
int fifo128_is_empty(struct fifo128 *que);
int fifo128_get_size(struct fifo128 *que);
int fifo128_enque8(struct fifo128 *que, uint8_t k);
uint8_t *fifo128_at8(struct fifo128 *que, int ith);
uint8_t *fifo128_top8(struct fifo128 *que);
int fifo128_deque8(struct fifo128 *que, uint8_t *pK);
int fifo128_enque32(struct fifo128 *que, uint32_t k);
uint32_t *fifo128_at32(struct fifo128 *que, int ith);
uint32_t *fifo128_top32(struct fifo128 *que);
int fifo128_deque32(struct fifo128 *que, uint32_t *pK);

#endif
