/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common input command.
 */

#include "common.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "mkbp_event.h"
#include "queue.h"
#include "task.h"
#include "ec_commands.h"
#include "util.h"

#define CONFIG_INPUT_FIFO 32

struct ec_input_info {
	uint32_t input_type;
	uint32_t code;
	uint32_t value;
};

/*
 * Mutex to protect input data
 */
static struct mutex g_input_mutex;

struct queue input_fifo = QUEUE_NULL(CONFIG_INPUT_FIFO, struct ec_input_info);

void input_send_event(struct ec_input_info* info)
{
	mutex_lock(&g_input_mutex);
	queue_add_unit(&input_fifo, info);
	mutex_unlock(&g_input_mutex);

	ccprintf("ec_input_info = %d, %d, %d\n", info->input_type, info->code,
			info->value);

	mkbp_send_event(EC_MKBP_EVENT_INPUT_EVENT);
}

void input_deque_event(struct ec_input_info* info)
{
	mutex_lock(&g_input_mutex);
	queue_remove_unit(&input_fifo, info);
	mutex_unlock(&g_input_mutex);
}

static int input_command(int argc, char **argv)
{
	struct ec_input_info info; 
	char *e;

	if (argc < 4)
		return EC_ERROR_INVAL;

	info.input_type = strtoi(argv[1], &e, 0);
	if (*e) {
		ccputs("Invalid param\n");
		return EC_ERROR_INVAL;
	}

	info.code = strtoi(argv[2], &e, 0);
	if (*e) {
		ccputs("Invalid param\n");
		return EC_ERROR_INVAL;
	}

	info.value = strtoi(argv[3], &e, 0);
	if (*e) {
		ccputs("Invalid param\n");
		return EC_ERROR_INVAL;
	}

	input_send_event(&info);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(input, input_command,
			"input-type code value",
			"send a simulated input event",
			NULL);

static int input_get_next_event(uint8_t *out)
{
	union ec_response_get_next_data *data =
		(union ec_response_get_next_data *)out;
	struct ec_input_info* e = (struct ec_input_info*)&data->ec_input_info;
	input_deque_event(e);
	return sizeof(data->ec_input_info);
}

DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_INPUT_EVENT, input_get_next_event);
