/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "queue.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/* Event log FIFO */
QUEUE_CONFIG(log_events, CONFIG_USB_PD_LOG_SIZE, uint8_t);

void pd_log_event(uint8_t type, uint8_t size_port,
		  uint16_t data, void *payload)
{
	struct ec_response_pd_log r;
	unsigned payload_size = PD_LOG_SIZE(size_port);

	/* Out of space : discard the oldest entry */
	while (queue_space(&log_events) < sizeof(r) + payload_size) {
		size_t head = log_events.state->head &
				(CONFIG_USB_PD_LOG_SIZE - 1);
		struct ec_response_pd_log *oldest =
			(void *)(log_events.buffer+head);
		unsigned additional = PD_LOG_SIZE(oldest->size_port);
		/* TODO race condition with queue_remove_units */
		log_events.state->head += sizeof(r) + additional;
	}

	r.timestamp = get_time().val >> PD_LOG_TIMESTAMP_SHIFT;
	r.type = type;
	r.size_port = size_port;
	r.data = data;

	queue_add_units(&log_events, &r, sizeof(r));
	if (payload_size)
		queue_add_units(&log_events, payload, payload_size);
}

static int hc_pd_get_log_entry(struct host_cmd_handler_args *args)
{
	struct ec_response_pd_log *r = args->response;
	uint32_t now = get_time().val >> PD_LOG_TIMESTAMP_SHIFT;
	unsigned payload_size;

	/* The log FIFO is empty */
	if (queue_is_empty(&log_events)) {
		r->timestamp = PD_LOG_TIMESTAMP_NO_ENTRY;
		args->response_size = sizeof(*r);
		return EC_RES_SUCCESS;
	}

	queue_remove_units(&log_events, r, sizeof(*r));
	payload_size = PD_LOG_SIZE(r->size_port);
	if (payload_size)
		queue_remove_units(&log_events, r->payload, payload_size);

	/* fixup the timestamp : number of milliseconds in the past */
	r->timestamp = now - r->timestamp;

	args->response_size = sizeof(*r) + payload_size;
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_GET_LOG_ENTRY,
		     hc_pd_get_log_entry,
		     EC_VER_MASK(0));
