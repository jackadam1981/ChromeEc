/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC command handler
 */

/*
 * Cr50 and EC are connected by UART_EC_TX & UART_EC_RX and there are 3 modes:
 *
 *   1. Normal mode: UART_RX/TX are not used
 *   2. CCD mode: UART_RX/TX are used to pass-through byte stream to/from USB.
 *   3. Packet mode: UART_RX/TX are used to transfer data packets
 *      (more specifically host protocol V4 packets)
 *
 * Normal and CCD modes are handled by rdd.c. Note that other CCD mode users
 * (e.g. AP console) are not affected.
 *
 * Since current version of Cr50 doesn't support break code on UART, we make
 * Cr50 watch UART_RX for 'marker' (= 0xec 0xec ...). When a marker is detected,
 * Cr50 transitions to the packet mode.
 *
 * States:
 *
 *   DISCONNECTED: Normal mode or CCD mode
 *   CONNECTED: Packet mode
 */

#include "common.h"
#include "board.h"
#include "console.h"
#include "ec_comm.h"
#include "gpio.h"
#include "hooks.h"
#include "stdint.h"
#include "task.h"
#include "timer.h"
#include "usart.h"
#include "uartn.h"

#define CPRINTS(format, args...) cprints(CC_VBOOT,"VB " format, ## args)
#define CPRINTF(format, args...) cprintf(CC_VBOOT,"VB " format, ## args)
#ifndef DEBUG_EC_CR50_COMM
# define CPRINTD(format, args...) \
	cprints(CC_VBOOT,"VB %s: " format, __func__, ## args)
#else
# define CPRINTD(format, args...)
#endif

#define EC_COMM_PACKET_SIZE	32
#define TASK_EVENT_PACKET_MODE_REQUESTED	TASK_EVENT_CUSTOM(1 << 0)
#define TASK_EVENT_PACKET_RECEIVED		TASK_EVENT_CUSTOM(1 << 1)

static enum device_state state = DEVICE_STATE_DISCONNECTED;
static uint8_t packet_buf[EC_COMM_PACKET_SIZE];
static int packet_len;

static void set_state(enum device_state s)
{
	if (state == s)
		return;
	CPRINTD("%d", s);
	state = s;
}

static void enable_packet_mode(void)
{
	if (state == DEVICE_STATE_CONNECTED)
		return;

	/* TODO: Connect UART_EC_TX instead of wait until it's connected. */
	if (!uart_tx_is_connected(UART_EC)) {
		CPRINTD("Reject (UART_EC_TX not ready)");
		set_state(DEVICE_STATE_DISCONNECTED);
		return;
	}

	/* Return ACK */
	uartn_write_char(ec_uart.uart, 0xec);
	uartn_tx_start(ec_uart.uart);

	packet_len = 0;
	memset(packet_buf, 0, sizeof(packet_buf));

	set_state(DEVICE_STATE_CONNECTED);

	/* TODO: Flush the remaining 0xec... */
	/* TODO: Disconnect UART_EC_TX if it's previously disconnected. */
}

static void disable_packet_mode(void)
{
	set_state(DEVICE_STATE_DISCONNECTED);
}

static void hexdump(const uint8_t *data, int len)
{
	int i, j;

	if (!len)
		return;

	for (i = 0; i < (len-1) / 16 + 1; i++) {
		for (j = i*16; j < i*16 + 16; j++) {
			if (j < len)
				ccprintf(" %02x", data[j]);
			else
				ccprintf("   ");
		}
		ccprintf(" |");
		for (j = i*16; j < i*16 + 16; j++) {
			if (j < len) {
				if (isprint(data[j]))
					ccprintf("%c", data[j]);
				else
					ccprintf(".");
			} else {
				ccprintf(" ");
			}
		}
		ccprintf("|\n");
	}
}

static void process_command(void)
{
	/* Execute command */
	ccprintf("EC says:\n");
	hexdump(packet_buf, packet_len);

	/* Send ACK */
	uartn_write_char(ec_uart.uart, 0xec);
	uartn_tx_start(ec_uart.uart);

	/* Disconnect
	 * TODO: Keep it open for back-to-back packets */
	disable_packet_mode();
}

/*
 * Task loop & State machine
 */
void ec_command_task(void *u)
{
	while (1) {
		uint32_t evt = task_wait_event(-1);
		CPRINTD("evt=0x%x", evt);

		switch (state) {
		case DEVICE_STATE_DISCONNECTED:
			if (evt & TASK_EVENT_PACKET_MODE_REQUESTED)
				enable_packet_mode();
			break;
		case DEVICE_STATE_CONNECTED:
			if (evt & TASK_EVENT_PACKET_RECEIVED)
				process_command();
			break;
		default:
			;
		}
	}
}

/*
 * APIs and Console commands
 */
static int command_ec(int argc, char **argv)
{
	hexdump(packet_buf, packet_len);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec, command_ec, NULL, "Dump EC packet buffer");

int packet_mode_is_enabled(uint8_t *buffer, int len)
{
	static int counter = 0;

	/*
	 * Return 1 to consume data. Return 0 to forward data to USB.
	 *
	 * This whole function must be compact to forward data to USB timely.
	 * Handling len == 0 isn't necessary. State transition and processing
	 * data should be deferred to the command task.
	 */

	if (state == DEVICE_STATE_CONNECTED) {
		memcpy(packet_buf + packet_len, buffer, len);
		packet_len += len;
		if (packet_len >= EC_COMM_PACKET_SIZE)
			/* Wake up for processing a packet */
			task_set_event(TASK_ID_EC_COMMAND,
				       TASK_EVENT_PACKET_RECEIVED, 0);
		return 1;
	}

	while (len--) {
		if (*buffer++ != 0xec) {
			/* Sequence is broken. Return asap. */
			counter = 0;
			return 0;
		}
		counter++;
	}

	if (counter >= 16) {
		/* Wake up for entering packet mode */
		task_set_event(TASK_ID_EC_COMMAND,
			       TASK_EVENT_PACKET_MODE_REQUESTED, 0);
		return 1;
	}

	/* Forward data (including 0xec...) to USB */
	return 0;
}
