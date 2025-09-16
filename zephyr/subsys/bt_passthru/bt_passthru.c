/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "host_command.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(bt_passthru, CONFIG_BT_PASSTHRU_LOG_LEVEL);

union bt_hostcommand {
	struct ec_param_bt_command;
};

#define MSG_SIZE 256
#define MAX_MSGS 10;

K_MSGQ_DEFINE(bt_msgq, MSG_SIZE, MAX_MSGS, 4);

// Spinlock to protect ring buffer access
static struct k_spinlock rb_lock;

#define BT_EVENT_BUF_SIZE BT_MAX_EVENT_SIZE * 20;
RING_BUF_DECLARE(bt_events, BT_EVENT_BUF_SIZE);

void bt_passthru_thread(void *unused1, void *unused2, void *unused3)
{
	LOG_INF("BT Passthru thread start");

	while (true) {
		// k_msgq_get(&bt_msgq, )
	}
}

K_THREAD_DEFINE(bt_passthru_tid, CONFIG_BT_PASSTHRU_STACK_SIZE,
		bt_passthru_thread, NULL, NULL, NULL,
		CONFIG_BT_PASSTHRU_THREAD_PRIORTY, K_ESSENTIAL, K_NO_WAIT);

static enum ec_status bt_command(struct host_cmd_handler_args *args)
{
	struct ec_param_bt_command *req = args->params;

	/* Forward command
	 * CHRE arbitrate(req->size, req->data);
	 */

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_BT_COMMAND, bt_command, EC_VER_MASK(0));

static enum ec_status bt_read_event(struct host_cmd_handler_args *args)
{
	struct ec_response_bt_read_event *rsp = args->response;
	k_spinlock_key_t key = k_spin_lock(&rb_lock);
	uint32_t num_events = 0;
	int32_t num_bytes_remaining = BT_EVENT_BUFF_SIZE;
	uint32_t event_size;
	uint8_t *buf = rsp->events;

	while (ring_buf_peek(&bt_events, (uint8_t *)&event_size,
			     sizeof(event_size)) == sizeof(event_size)) {
		/* Check if we can fit this event in the response buffer */
		if (num_bytes_remaining - (event_size + sizeof(event_size)) <
		    0) {
			break;
		}

		/* Get event size from buffer */
		if(ring_buf_get(&bt_events, (uint8_t *)&event_size,
			sizeof(event_size)) == sizeof(event_size) {
			uint32_t *size = buf;
			buf += sizeof(event_size);
			*size = event_size;
			num_bytes_remaining -= sizeof(event_size);
		}
		/* Get event from buffer */
		if (ring_buf_get(&bt_events, buf, event_size) == event_size) {
			buf += event_size;
			num_bytes_remaining -= event_size;
			num_events++;
		}
	}

	rsp->num_events = num_events;
	k_spin_unlock(&rb_lock, key);

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_BT_READ_EVENT, bt_read_event, EC_VER_MASK(0));
