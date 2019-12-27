/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC-CR50 communication
 */
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "system.h"

#define CPRINTS(format, args...) cprints(CC_TASK, "EC-COMM: " format, ## args)

/*
 * Context of EC-CR50 communication
 */
static struct ec_comm_context_ {
	uint8_t uart;	/* Current UART ID in packet mode.        */
			/* UART_NULL if no UART is in packet mode */
} ec_comm_ctx;


/*
 * Initialize EC-CR50 communication context.
 */
static void ec_comm_init_(void)
{
	ec_comm_ctx.uart = UART_NULL;

	if (!board_has_ec_cr50_comm_support())
		return;

	CPRINTS("Initializtion");

	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_EN);
	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_DIS);

	/* If DIOB3 is already high, then enable the packet mode. */
	if (gpio_get_level(GPIO_EC_PACKET_MODE_EN))
		ec_comm_packet_mode_en(GPIO_EC_PACKET_MODE_EN);
}
DECLARE_HOOK(HOOK_INIT, ec_comm_init_, HOOK_PRIO_DEFAULT + 1);

void ec_comm_packet_mode_en(enum gpio_signal unsed)
{
	if (board_deep_sleep_allowed())
		disable_deep_sleep();

	/* TODO: Initialize packet context */

	ec_comm_ctx.uart = UART_EC;  /* Enable Packet Mode */
	ccd_update_state();
}

void ec_comm_packet_mode_dis(enum gpio_signal unsed)
{
	ec_comm_ctx.uart = UART_NULL;	 /* Disable Packet Mode. */
	ccd_update_state();

	/* TODO: flush USART TX for EC */

	/* If AP is still off, then let's resume deep sleep checking. */
	if (!gpio_get_level(GPIO_TPM_RST_L))
		tpm_rst_asserted(GPIO_TPM_RST_L);
}

int ec_comm_is_uart_in_packet_mode(int uart)
{
	return uart == ec_comm_ctx.uart;
}
