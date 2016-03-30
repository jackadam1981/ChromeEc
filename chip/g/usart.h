/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "consumer.h"
#include "producer.h"
#include "registers.h"

#ifndef __CROS_FORWARD_UART_H
#define __CROS_FORWARD_UART_H

struct uart_config {
	int uart;
	struct producer const producer;
	struct consumer const consumer;
};

extern struct consumer_ops const uart_consumer_ops;
extern struct producer_ops const uart_producer_ops;

#define USART_CONFIG(UART,						\
		     RX_QUEUE,						\
		     TX_QUEUE)						\
	((struct uart_config const) {					\
		.uart      = UART,					\
		.consumer  = {						\
			.queue = &TX_QUEUE,				\
			.ops   = &uart_consumer_ops,			\
		},							\
		.producer  = {						\
			.queue = &RX_QUEUE,				\
			.ops   = &uart_producer_ops,			\
		},							\
	})

/* Read data from UART and add it to the producer queue */
void send_data_to_usb(struct uart_config const *config);

/* Read data from the consumer queue and send it to the UART */
void get_data_from_usb(struct uart_config const *config);
#endif  /* __CROS_FORWARD_UART_H */
