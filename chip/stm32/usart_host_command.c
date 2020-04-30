/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "clock.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "queue_policies.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "usart.h"
#include "usart_host_command.h"
#include "usart-stm32f4.h"
#include "util.h"

/*
 *
 */
#define CPRINTS(format, args...) cprints(CC_UART_TL, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)



/*
 *
 */
static struct usart_config const tl_usart = {
        &CONFIG_TL_UART_HW,
        &tl_usart_rx_interrupt,
		&tl_usart_tx_interrupt,
		&((struct usart_state){}),
		CONFIG_TL_UART_BAUD_RATE,
		0,
		.consumer = {
                .queue = NULL,
                .ops = NULL,
                },
		.producer = {
                .queue = NULL,
                .ops = NULL,
        }
};

/*
 *
 */
void tl_usart_request_char_in(char in_char) {

}

/*
 *
 */
void usart_host_command_init(void) {

    /* Initialize transport uart */
	usart_init(&tl_usart);

}
