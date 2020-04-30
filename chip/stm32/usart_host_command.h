/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USART_HOST_COMMAND_H
#define __CROS_EC_USART_HOST_COMMAND_H

#include <stdarg.h>  /* For va_list */
#include "common.h"
#include "gpio.h"
#include "usart.h"

/*
 * Function to handle incoming bytes from DMA interrupt handler
 */
void usart_host_command_rx_add_bytes(const void *src, size_t count);

/*
 * Get response char to be sent by tl_usart_tx_interrupt_handler
 */
size_t usart_host_command_tx_char(char *out_char);

/*
 * Initialize USART host command layer.
 */
void usart_host_command_init(void);

#endif /* __CROS_EC_USART_HOST_COMMAND_H */
