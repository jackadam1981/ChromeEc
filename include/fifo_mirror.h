/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * Simple FIFO or Circular Buffer
 *  - Implementation: mirroring method
 *  - Max capacity requirement: 2^n elements (power of two)
 *    For example:
 *      max capacity = 4
 *          [ 0 1 2 3 | 4 5 6 7 ], where '|' means mirroring.
 *          2-bit buffer index address.
 *      write pointer [2:0] (3-bit), wptr
 *      read pointer [2:0] (3-bit), rptr
 *          3-bit pointer address.
 *          one extra bit to tell the difference between is_empty and is_full.
 *
 *  - Supports the following usages:
 *       enque 1 byte at time; dequeue 1 byte at a time.
 *       enque 4-byte (aligned) at time; dequeue 4 byte (aligned) at a time.
 *       enque 1 byte at time; dequeue 4 byte (aligned) at a time.
 *       enque 4 byte (aligned) at time; dequeue 1 byte at a time.
 */
#ifndef __FIFO_MIRROR_H__
#define __FIFO_MIRROR_H__

#include <stdint.h>
#include <stdio.h>

/* FIFO Data Structure */
struct fifo_mirror {
	/*
	 * FIFO Buffer Base Address
	 */
	union {
		volatile uint32_t *base32;
		volatile uint8_t  *base8;
		volatile void     *base;
	};

	/*
	 * 16-bit read index address
	 */
	uint16_t *p_read_idx;  /**< read idx */

	/*
	 * 16-bit write index address
	 */
	uint16_t *p_write_idx; /**< write idx */

	/*
	 * Max Capacity of the fifo:
	 *  only saved n since the size is power of 2. (2^n)
	 */
	uint8_t  size_n;       /**<  fifo size: (1 << n) */
};

/*
 * Init FIFO_MIRROR data structure.
 *  - Init FIFO base address
 *  - Init read and write pointer address
 *  - Init read and writer pointer value to 0
 * @param p_read_idx, a pointer to read index
 * @param p_write_idx, a pointer to write index
 * @param size_n,  fifo degree
 */
void fifo_mirror_init(struct fifo_mirror *que,
	void *base,
	uint16_t *p_read_idx,
	uint16_t *p_write_idx,
	uint8_t size_n);

/*
 * fifo_mirror_is_empty
 *      return wptr == rptr
 *
 *      Empty Case 1: wptr = rptr = 0
 *        |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *        |-rptr
 *
 *      Empty Case 2: wptr = rptr = 1
 *          |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *          |-rptr
 *
 *      Empty Case 3: wptr = rptr = 7
 *                        |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *                        |-rptr
 * @return true if the fifo is empty
 */
int fifo_mirror_is_empty(struct fifo_mirror *que);

/*
 * fifo_mirror_is_full
 *      return (rptr ^ wptr) == max_capacity
 *
 *      Full Case 1: wptr = 4; rptr = 0
 *                  |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *        |-rptr
 *
 *      Full Case 2: wptr = 5; rptr = 1
 *                    |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *          |-rptr
 *
 *      Full Case 3: wptr = 7; rptr = 3
 *                        |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *              |-rptr
 *
 *      Full Case 4: wptr = 0; rptr = 4
 *        |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *                  |-rptr
 *
 *      Full Case 5: wptr = 3; rptr = 7
 *              |-wptr
 *      [ 0 1 2 3 | 4 5 6 7 ]
 *                        |-rptr
 * @return true if the fifo is full (Example: 4 elements)
 */
int fifo_mirror_is_full(struct fifo_mirror *que);


/*
 * fifo_mirror_get_size
 * @return the total number of elements in the fifo.
 */
int fifo_mirror_get_size(struct fifo_mirror *que);

/*
 * fifo_mirror_enque8
 * Enque 1 byte into fifo
 * @param val, 8-bit value to eqnue.
 * @return number of bytes enqued: 0 or 1
 */
int fifo_mirror_enque8(struct fifo_mirror *que, uint8_t val);

/* fifo_mirror_at8
 * Check the fifo at ith location.
 * @param ith, a unit is 1 byte.
 * @return the address of ith element.
 *        or NULL if the ith element is valid.
 */
volatile uint8_t *fifo_mirror_at8(struct fifo_mirror *que, int ith);

/*
 * fifo_mirror_top8
 * Check the top 1-byte element of the fifo.
 * @return the address of top (0th) element
 *        or NULL if fifo is empty.
 */
volatile uint8_t *fifo_mirror_top8(struct fifo_mirror *que);

/*
 * fifo_mirror_deque8
 *    Deque 1 byte from the fifo
 * @param p_val, output, return dequed value.
 * @return num of bytes dequed (0 or 1)
 */
int fifo_mirror_deque8(struct fifo_mirror *que, uint8_t *p_val);

/*
 * fifo_mirror_enque32
 *    Enque 4-byte into the fifo
 * @param val, 32-bit value to eqnue.
 * @return number of bytes enque, 0 or 4
 * return 0 if the current write pointer is not at 4-byte aligned address.
 */
int fifo_mirror_enque32(struct fifo_mirror *que, uint32_t val);

/*
 * fifo_mirror_at32
 * Check the fifo at ith location.
 * @param ith, a unit is 4 byte.
 * @return the address of ith element.
 *        or NULL if the ith element is valid.
 */
volatile uint32_t *fifo_mirror_at32(struct fifo_mirror *que, int ith);

/*
 * fifo_mirror_top32
 * Check the top 4-byte element of the fifo.
 * @return the address of top (0th) element
 *        or NULL if fifo is empty.
 */
volatile uint32_t *fifo_mirror_top32(struct fifo_mirror *que);

/*
 * fifo_mirror_deque32
 *    Deque 4 byte from the fifo
 * @param p_val, output, return dequed value.
 * @return num of bytes dequed (0 or 4)
 *  return 0 if the current read pointer is not at 4-byte aligned address.
 */
int fifo_mirror_deque32(struct fifo_mirror *que, uint32_t *p_val);

#endif
