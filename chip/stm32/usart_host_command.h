/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USART_HOST_COMMAND_H
#define __CROS_EC_USART_HOST_COMMAND_H

#include "common.h"
#include "gpio.h"
#include "host_command.h"
#include "usart.h"

#include <stdarg.h> /* For va_list */

/*
 * Add data to host command layer buffer.
 */
size_t usart_host_command_rx_append_data(struct usart_config const *config,
					 const uint8_t *src, size_t count);

/*
 * Remove data from the host command layer buffer.
 */
size_t usart_host_command_tx_remove_data(struct usart_config const *config,
					 uint8_t *dest);
/*
 * Initialize USART host command layer.
 */
void usart_host_command_init(void);

/*
 * Protocol info getters for usart and spi
 */
uint16_t usart_max_request_size(void);
uint16_t usart_max_response_size(void);
uint16_t spi_max_request_size(void);
uint16_t spi_max_response_size(void);

#endif /* __CROS_EC_USART_HOST_COMMAND_H */
