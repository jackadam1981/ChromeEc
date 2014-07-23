/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef CHIP_STM32_USART_H
#define CHIP_STM32_USART_H

/* STM32 USART driver for Chrome EC */

#include "fifo.h"
#include "in_stream.h"
#include "out_stream.h"

#include <stdint.h>

/*
 * Per-USART state stored in RAM.  This structure will be zero initialized by
 * BSS init.  Most importantly, irq_lock will be zero, ensuring that shared
 * interrupts don't cause problems.
 */
typedef struct
{
	int paused;

	uint32_t irq_lock;

	fifo_state tx;
	fifo_state rx;
} usart_state;

/*
 * Per-USART hardware configuration stored in flash.  Instances of this
 * structure are provided by each variants driver, one per physical USART.
 */
typedef struct
{
	intptr_t base;
	int      irq;

	uint32_t volatile * clock_register;
	uint32_t            clock_enable;
} usart_hw_config;

/*
 * Compile time Per-USART configuration stored in flash.  Instances of this
 * structure are provided by the user of the USART.  This structure binds
 * together all information required to operate a USART.
 */
typedef struct
{
	/*
	 * Pointer to USART HW configuration.  There is one HW configuration
	 * per physical USART.
	 */
	usart_hw_config const * hw;

	/*
	 * Pointer to USART state structure.  The state structure maintains per
	 * USART information (head and tail pointers for the FIFOs for
	 * instance).
	 */
	usart_state volatile * state;

	/*
	 * Baud rate for USART.
	 */
	int baud;

	/*
	 * TX and RX FIFO configs.  The state for the fifo is stored separately
	 * in the usart_state.
	 */
	fifo_config rx;
	fifo_config tx;

	/*
	 * In and Out streams, these contain pointers to the virtual function
	 * tables that implement in and out streams.  They can be used by any
	 * code that wants to read or write to a stream interface.
	 */
	in_stream  in;
	out_stream out;
} usart_config;

/*
 * These function tables are defined by the USART driver and are used to
 * initialize the in and out streams in the usart_config.
 */
extern in_stream_ops const  usart_in_stream_ops;
extern out_stream_ops const usart_out_stream_ops;

/*
 * Convenience macro for defining USARTs and their associated state and buffers.
 * NAME is used to construct the names of the fifo buffers, usart_state struct,
 * and usart_config struct, the latter is just called NAME.  RX_SIZE and TX_SIZE
 * are the size in bytes of the RX and TX buffers respectively.  RX_READY and
 * TX_READY are the callback functions for the in and out streams.  The USART
 * baud rate is specified by the BAUD parameter.
 *
 * If you want to share a FIFO with other code, you can manually initialize a
 * usart_config to use the shared FIFO, or you can use this macro and then get
 * the fifo buffers as <NAME>_tx_buffer, and <NAME>_rx_buffer.
 */
#define USART_CONFIG(NAME,					\
		     HW,					\
		     BAUD,					\
		     RX_SIZE,					\
		     TX_SIZE,					\
		     RX_READY,					\
		     TX_READY)					\
	static uint8_t NAME##_tx_buffer[TX_SIZE];		\
	static uint8_t NAME##_rx_buffer[RX_SIZE];		\
								\
	static usart_state NAME##_state;			\
	usart_config const NAME =				\
	{							\
		.hw    = &HW,					\
		.state = &NAME##_state,				\
		.baud  = BAUD,					\
		.rx = {						\
			.state  = &NAME##_state.rx,		\
			.buffer = NAME##_rx_buffer,		\
			.size   = RX_SIZE,			\
		},						\
		.tx = {						\
			.state  = &NAME##_state.tx,		\
			.buffer = NAME##_tx_buffer,		\
			.size   = TX_SIZE,			\
		},						\
		.in  = {					\
			.ready = RX_READY,			\
			.ops   = &usart_in_stream_ops,		\
		},						\
		.out = {					\
			.ready = TX_READY,			\
			.ops   = &usart_out_stream_ops,		\
		},						\
	};

/*
 * Initialize the given USART.  Once init is finished the USART streams are
 * available for operating on, and the stream ready callbacks could be called
 * at any time.
 */
void usart_init(usart_config const * config);

#endif //CHIP_STM32_USART_H
