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

void tl_usart_rx_add_bytes(const void *src, size_t count);

uint16_t tl_usart_tx_char(char* out_char);

void usart_host_command_init(void);

#endif //__CROS_EC_USART_HOST_COMMAND_H
