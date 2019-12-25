/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "ps2_chip.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_PS2, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_PS2, format, ## args)

#define TASK_EVENT_TX_TEST  TASK_EVENT_CUSTOM_BIT(0)
#define PS2_MAX_BUFFER_SIZE    64

struct ps2_task_data_buf {
	uint8_t buf[PS2_MAX_BUFFER_SIZE];
	uint8_t index;
};

struct ps2_task_data_buf ps2_task_buf[NPCX_PS2_CH_COUNT];

/* PS/2 task 0 connects to a PS/2 emulator and execute the ping-pong test */
void ps2_task0_rx_callback(uint8_t data)
{
	struct ps2_task_data_buf *buf_ptr = &ps2_task_buf[NPCX_PS2_CH0];

	buf_ptr->buf[buf_ptr->index++] = data;

	task_wake(TASK_ID_PS2_0);
}

void ps2_task0(void *u)
{
	uint32_t evt;
	struct ps2_task_data_buf *buf_ptr = &ps2_task_buf[NPCX_PS2_CH0];
	static uint32_t tx_data;

	ccprintf("Task PS2-0 Init\n");
	ps2_enable_channel(NPCX_PS2_CH0, 1, ps2_task0_rx_callback);
	while (1) {
		evt = task_wait_event(-1);
		ccprintf("evt=0x%08x\n", evt);
		if (evt & TASK_EVENT_WAKE) {
			if (buf_ptr->index > 0) {
				ccprintf("PS2-0 Rx:[");
				for (int i = 0; i < buf_ptr->index; i++)
					ccprintf("0x%02x ", buf_ptr->buf[i]);
				ccprintf("]\n");
				buf_ptr->index = 0;
			}
			if (tx_data == 256)
				tx_data = 0;
			else
				ps2_transmit_byte(NPCX_PS2_CH0, tx_data++);
		}
	}
}

void ps2_task1_rx_callback(uint8_t data)
{
	struct ps2_task_data_buf *buf_ptr = &ps2_task_buf[NPCX_PS2_CH1];

	buf_ptr->buf[buf_ptr->index++] = data;

	task_wake(TASK_ID_PS2_1);
}

/*
 * PS/2 task1 connects to the standard PS/2 keyboard
 * It prints the scan code when receive data when key is entered
 */
void ps2_task1(void *u)
{
	uint32_t evt;
	struct ps2_task_data_buf *buf_ptr = &ps2_task_buf[NPCX_PS2_CH1];

	ccprintf("Task PS2-1 Init\n");
	ps2_enable_channel(NPCX_PS2_CH1, 1, ps2_task1_rx_callback);
	while (1) {
		evt = task_wait_event(-1);
		ccprintf("evt=0x%08x\n", evt);
		if (evt & TASK_EVENT_WAKE) {
			if (buf_ptr->index > 0) {
				ccprintf("PS2-1 Rx:[");
				for (int i = 0; i < buf_ptr->index; i++)
					ccprintf("0x%02x ", buf_ptr->buf[i]);
				ccprintf("]\n");
				buf_ptr->index = 0;
			}
		}
		if (evt & TASK_EVENT_TX_TEST)
			ps2_transmit_byte(NPCX_PS2_CH1, 0xF2);
	}
}

/*
 * THe hook deferred function will send the command 0xF2 (Read ID) to keyboard
 * every 500 mili-second
 */
static void ps2_task1_tx_test(void);
DECLARE_DEFERRED(ps2_task1_tx_test);
static void ps2_task1_tx_test(void)
{
	task_set_event(TASK_ID_PS2_1, TASK_EVENT_TX_TEST, 0);
	hook_call_deferred(&ps2_task1_tx_test_data, 500 * MSEC);
}

void ps2_task2_rx_callback(uint8_t data)
{
	struct ps2_task_data_buf *buf_ptr = &ps2_task_buf[NPCX_PS2_CH2];

	buf_ptr->buf[buf_ptr->index++] = data;

	task_wake(TASK_ID_PS2_2);
}

/*
 * PS/2 task2 connects to the standard PS/2 keyboard
 * It prints the scan code when receive data when key is entered
 */
void ps2_task2(void *u)
{
	uint32_t evt;
	struct ps2_task_data_buf *buf_ptr = &ps2_task_buf[NPCX_PS2_CH2];

	ccprintf("Task PS2-2 Init\n");
	ps2_enable_channel(NPCX_PS2_CH2, 1, ps2_task2_rx_callback);
	while (1) {
		evt = task_wait_event(-1);
		ccprintf("evt=0x%08x\n", evt);
		if (evt & TASK_EVENT_WAKE) {
			if (buf_ptr->index > 0) {
				ccprintf("PS2-2 Rx:[");
				for (int i = 0; i < buf_ptr->index; i++)
					ccprintf("0x%02x ", buf_ptr->buf[i]);
				ccprintf("]\n");
				buf_ptr->index = 0;
			}
		}
		if (evt & TASK_EVENT_TX_TEST)
			ps2_transmit_byte(NPCX_PS2_CH2, 0xF2);
	}
}

/*
 * THe hook deferred function will send the command 0xF2 (Read ID) to keyboard
 * every 500 mili-second
 */
static void ps2_task2_tx_test(void);
DECLARE_DEFERRED(ps2_task2_tx_test);
static void ps2_task2_tx_test(void)
{
	task_set_event(TASK_ID_PS2_2, TASK_EVENT_TX_TEST, 0);
	hook_call_deferred(&ps2_task2_tx_test_data, 500 * MSEC);
}

static int command_ps2_tx_test(int argc, char **argv)
{
	uint8_t ch;
	int enable;
	char *e;

	ch = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	enable = strtoi(argv[2], &e, 0);
	if (enable)
		enable = 0;
	else
		enable = -1;

	switch (ch) {
	case NPCX_PS2_CH0:
		task_wake(TASK_ID_PS2_0);
		break;
	case NPCX_PS2_CH1:
		hook_call_deferred(&ps2_task1_tx_test_data, enable);
		break;
	case NPCX_PS2_CH2:
		hook_call_deferred(&ps2_task2_tx_test_data, enable);
		break;
	default:
		break;
	}
	return 0;
}
DECLARE_CONSOLE_COMMAND(ps2_tx_test, command_ps2_tx_test, "", "");
