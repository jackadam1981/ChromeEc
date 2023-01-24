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
 * Max data size for a version 3 request/response packet.  This is big enough
 * to handle a request/response header, flash write offset/size and 512 bytes
 * of request payload or 224 bytes of response payload.
 */
#define USART_MAX_REQUEST_SIZE 0x220
#define USART_MAX_RESPONSE_SIZE 0x100

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

#endif /* __CROS_EC_USART_HOST_COMMAND_H */
