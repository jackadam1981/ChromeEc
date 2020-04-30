/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USART_HOST_COMMAND_H
#define __CROS_EC_USART_HOST_COMMAND_H

#include <stdarg.h>  /* For va_list */
#include "common.h"
#include "gpio.h"



void usart_host_command_init(void);

#endif //__CROS_EC_USART_HOST_COMMAND_H
