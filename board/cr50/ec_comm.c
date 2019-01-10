/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC command handler
 */

/*
 * Cr50 and EC are connected by UART_TX, UART_RX, and CCD_MODE_L. UART_* are
 * used as a pass-through when CCD is enabled. UART_* are also used to exchange
 * EC-Host protocol V4 packets.
 *
 * The two modes can't be enabled at the same time due to the limited CPU
 * resource. So, Cr50 listens to UART_RX briefly (~1sec) when:
 *
 * 1. Cr50 resets
 * 2. Cr50 wakes up from deep sleep
 * 3. CCD_MODE_L is asserted
 *
 * For this period, Cr50 looks for 4 consecutive 0xECs. If they're found,
 * Cr50 enters packet mode.
 */

#include "common.h"
#include "board.h"
#include "console.h"
#include "ec_comm.h"
#include "gpio.h"
#include "hooks.h"
#include "stdint.h"
#include "task.h"

#include "queue_policies.h"
#include "timer.h"
#include "usart.h"
#include "uartn.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

static enum device_state state = DEVICE_STATE_OFF;

/* Called when command is dequeued */
static void command_read(struct producer const *producer, size_t count)
{
}

/* Called when command is enqueued */
static void command_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_EC_COMMAND);
}

struct producer_ops const command_producer_ops = { .read = command_read };
struct consumer_ops const command_consumer_ops = { .written = command_written };

extern struct queue const ec_uart_to_cmd;
extern struct queue const ec_cmd_to_uart;

static void set_state(enum device_state s)
{
	ccprintf("%s: %d\n", __func__, s);
	state = s;
}

static void ccd_mode(void);
DECLARE_DEFERRED(ccd_mode);
static void ccd_mode(void)
{
	int level = gpio_get_level(GPIO_CCD_MODE_L);
	ccprintf("%s: level=%d\n", __func__, level);

	switch (state) {
	case DEVICE_STATE_OFF:
		if (level)
			break;
		set_state(DEVICE_STATE_DEBOUNCING);
		hook_call_deferred(&ccd_mode_data, 10 * MSEC);
		return;
	case DEVICE_STATE_DEBOUNCING:
		if (!level)
			break;
		set_state(DEVICE_STATE_ON);
		task_set_event(TASK_ID_EC_COMMAND, TASK_EVENT_WAKE, 0);
		return;
	default:
		break;
	}

	set_state(DEVICE_STATE_OFF);
	gpio_enable_interrupt(GPIO_CCD_MODE_L);
}

void ccd_mode_asserted(enum gpio_signal signal)
{
	gpio_disable_interrupt(GPIO_CCD_MODE_L);
	hook_call_deferred(&ccd_mode_data, 5 * MSEC);
}

/*
 * Open command handler
 *
 * 1. Stop servo detection. Disconnect servo.
 * 2. Stop EC detection. Disconnect EC.
 * 3. Enable UART TX & RX for the host command protocol.
 */
static void open_command_handler(void)
{
	if (state == DEVICE_STATE_CONNECTED)
		/* Already opened */
		return;

	/* Disable ec_usb */

	/* Connect UART and Command */
	usart_set_ec_uart_queue(&ec_uart_to_cmd, &ec_cmd_to_uart);

	set_state(DEVICE_STATE_CONNECTED);
	ccd_update_state();
}

/*
 * Close command handler
 *
 * 1. Re-enable servo detection
 * 2. Re-enable EC detection
 *
 * UART TX & RX should be re-configured as EC and servo are re-discovered.
 */
static void close_command_handler(void)
{
	if (state == DEVICE_STATE_OFF)
		/* Already closed */
		return;
	set_state(DEVICE_STATE_OFF);
}

static void process_command(void)
{
	uint8_t buf[EC_COMM_PACKET_SIZE];
	//int len;

	ccprintf("Received from EC:");
	while (queue_count(&ec_uart_to_cmd)) {
		int len = queue_remove_unit(&ec_uart_to_cmd, &buf);
		int i;
		/* Execute command */
		uint8_t *d = (uint8_t *)&buf;
		for (i = 0; i < len; i++) {
			if (isprint(d[i]))
				ccprintf(" %02x(%c)", d[i], d[i]);
			else
				ccprintf(" %02x( )", d[i], d[i]);
		}
	}
	ccprintf("\n");
	uartn_write_char(ec_uart.uart, 0xce);
	uartn_tx_start(ec_uart.uart);
	/*
	memset(buf, 0xce, sizeof(buf));
	len = queue_add_units(&ec_cmd_to_uart, buf, sizeof(buf));
	ccprintf("Sent %d bytes\n", len);
	get_data_from_usb(&ec_uart);
	*/
}

void ec_command_task(void)
{
	gpio_enable_interrupt(GPIO_CCD_MODE_L);

	while (1) {
		task_wait_event(-1);

		if (!ec_is_in_ro()) {
			CPRINTS("WARN: EC entering RW from RW");
			assert_ec_rst();
			usleep(200);
			deassert_ec_rst();
			continue;
		}

		switch (state) {
		case DEVICE_STATE_ON:
			open_command_handler();
			break;
		case DEVICE_STATE_DISCONNECTED:
			close_command_handler();
			break;
		case DEVICE_STATE_CONNECTED:
			process_command();
			break;
		case DEVICE_STATE_OFF:
		default:
			;
		}
	}
}

/*
 * APIs
 */

int ec_is_speaking(void)
{
	return state == DEVICE_STATE_CONNECTED;
}

static int command_ec(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		char *e;
		int c = strtoi(argv[i], &e, 0);
		if (*e) {
			ccputs("invalid char\n");
			return EC_ERROR_INVAL;
		}
		uartn_write_char(ec_uart.uart, c);
		uartn_tx_start(ec_uart.uart);
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec, command_ec, "<v1> <v2> ...", "Send 0xce to EC");
