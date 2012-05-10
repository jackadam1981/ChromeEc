/*
 * This handles incoming commands and provides responses.
 *
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC <--> AP message handling.
 */

#include "board.h"
#include "message.h"
#include "keyboard_scan.h"
#include "util.h"

/* FIXME: for task... */
#include "console.h"
#include "task.h"
#include "timer.h"

#define CPRINTF(format, args...) cprintf(CC_KEYSCAN, format, ## args)

//static int fifo_delay;
//static struct mutex message_lock;
static int fifo_work_in_progress;

/* EC ID */
/* (TODO(dhendrix): Define this in board-specific code */
static const char ec_id[] = "Google Chrome EC";

/* Protocol version (least significant byte in lowest byte position) */
static const uint8_t proto_ver[] = { 1, 0, 0, 0 };

/**
 * Get the response to a given command
 *
 * @param cmd		Command byte to respond to
 * @param buffp		Buffer to use for data, or it can be updated to
 *			point to a new buffer
 * @param max_len	Number of bytes free at *buffp for the response.
 *			If the supplied buffer is used, this size must not
 *			be exceeded.
 * @return number of bytes in response (which is in *buffp), or -1 on error
 */
static int message_get_response(int cmd, uint8_t **buffp, int max_len)
{
	int ret = -1;

	/*
	 * Invalid commands are ignored, just returning a stream of 0xff
	 * bytes.
	 */
	switch (cmd) {
	case CMDC_PROTO_VER:
		*buffp = (uint8_t *)proto_ver;
#ifdef CONFIG_TASK_KEYSCAN
		keyboard_clear_state();
#endif
		ret = sizeof(proto_ver);
		break;
	case CMDC_NOP:
		break;
	case CMDC_ID:
		*buffp = (char *)ec_id;
		ret = sizeof(ec_id) - 1;
		break;
#ifdef CONFIG_TASK_KEYSCAN
	case CMDC_KEY_STATE:
		ret = keyboard_get_scan(buffp, max_len);
		break;
#endif
	default:
		ret = -1;
		break;
	}

	return ret;
}

int message_process_cmd(int cmd, uint8_t *out_msg, int max_len)
{
	uint8_t *msg;
	int msg_len;
	int need_copy;
	int sum = 0;
	int i;

	msg = out_msg;
	msg_len = message_get_response(cmd, &msg, max_len - MSG_PROTO_BYTES);
	if (msg_len < 0)
		return msg_len;

	/*
	 * We add MSG_PROTO_BYTES bytes of overhead: truncate the reply
	 * if needed.
	 */
	if (msg_len + MSG_PROTO_BYTES > max_len)
		msg_len = max_len - MSG_PROTO_BYTES;
	ASSERT(msg_len >= 0 && msg_len < 0xffff);
	need_copy = msg != out_msg;
	ASSERT(!need_copy ||
		msg + msg_len < out_msg ||
		msg > out_msg + sizeof(out_msg));

	for (i = 0; i < msg_len; i++) {
		if (need_copy)
			out_msg[i] = msg[i];
		sum += msg[i];
	}
	out_msg[i] = sum;

	/* give the caller a chance to do work before fifo task kicks in */
	fifo_work_in_progress = 1;
	return msg_len + MSG_PROTO_BYTES;
}

void fifo_task(void)
{
	/* FIXME: Disable this task if we're in sleep mode or the AP is off */
	while (1) {
		usleep(100000);

		/* if a transaction which will use the FIFO is already in
		   progress, add an extra delay period to avoid interrupting
		   the AP too much */
		if (fifo_work_in_progress) {
			fifo_work_in_progress = 0;
			continue;
		}

		if (!keyboard_fifo_empty()) {
			CPRINTF("interrupting host\n");
			board_interrupt_host();
		}
	}
}
