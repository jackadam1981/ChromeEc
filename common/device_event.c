/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Device event commands for Chrome EC */

#include "atomic.h"
#include "common.h"
#include "console.h"
#include "host_command.h"
#include "lpc.h"
#include "mkbp_event.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_EVENTS, outstr)
#define CPRINTS(format, args...) cprints(CC_EVENTS, format, ## args)

static uint32_t device_events;
static uint32_t device_events_mask;

uint32_t device_get_events(void)
{
	return device_events;
}

static uint32_t device_get_and_clear_events(void)
{
	return atomic_read_clear(&device_events);
}

static uint32_t device_get_events_mask(void)
{
	return device_events_mask;
}

void device_set_events(uint32_t mask)
{
	/* Ignore events that are not enabled */
	mask &= device_events_mask;

	if ((device_events & mask) != mask)
		CPRINTS("device event set 0x%08x", mask);

	atomic_or(&device_events, mask);

	/* Signal host that a device event is pending */
	host_set_single_event(EC_HOST_EVENT_DEVICE);
}

void device_clear_events(uint32_t mask)
{
	/* Only print if something's about to change */
	if (device_events & mask)
		CPRINTS("device_event clear 0x%08x", mask);

	atomic_clear(&device_events, mask);
}

static void device_set_events_mask(uint32_t mask)
{
	if ((device_events_mask & mask) != mask)
		CPRINTS("device event mask set 0x%08x", mask);

	device_events_mask = mask;
}

/*****************************************************************************/
/* Console commands */

static int command_device_event(int argc, char **argv)
{
	/* Handle sub-commands */
	if (argc == 3) {
		char *e;
		int i = strtoi(argv[2], &e, 0);

		if (*e)
			return EC_ERROR_PARAM2;
		else if (!strcasecmp(argv[1], "set"))
			device_set_events(i);
		else if (!strcasecmp(argv[1], "clear"))
			device_clear_events(i);
		else if (!strcasecmp(argv[1], "mask"))
			device_set_events_mask(i);
		else
			return EC_ERROR_PARAM1;
	}

	ccprintf("Events:    0x%08x\n", device_get_events());
	ccprintf("Mask:      0x%08x\n", device_get_events_mask());

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(deviceevent, command_device_event,
			"[set | clear | mask] [mask]",
			"Print / set device event state");

/*****************************************************************************/
/* Host commands */

static int device_event_cmd(struct host_cmd_handler_args *args)
{
	const struct ec_params_device_event *p = args->params;
	struct ec_response_device_event *r = args->response;

	switch (p->param) {
	case EC_DEVICE_EVENT_PARAM_GET_EVENTS:
		r->event_mask = device_get_and_clear_events();
		break;
	case EC_DEVICE_EVENT_PARAM_GET_MASK:
		r->event_mask = device_get_events_mask();
		break;
	case EC_DEVICE_EVENT_PARAM_SET_MASK:
		device_set_events_mask(p->event_mask);
		r->event_mask = device_get_events_mask();
		break;
	}

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_DEVICE_EVENT, device_event_cmd, EC_VER_MASK(0));
