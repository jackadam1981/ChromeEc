/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "console.h"
#include "hwtimer.h"
#include "trace.h"
#include "util.h"

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#ifndef CONFIG_TRACE_BUFFER_SIZE
#define CONFIG_TRACE_BUFFER_SIZE 512
#endif

struct trace_event {
	uint32_t time;
	const char *evt;
	enum trace_type type;
};

struct trace_event events[CONFIG_TRACE_BUFFER_SIZE];
int curevent;

void add_trace_event(const char *evt, enum trace_type type)
{
	events[curevent].time = __hw_clock_source_read();
	events[curevent].evt = evt;
	events[curevent].type = type;
	if (++curevent == ARRAY_SIZE(events))
		curevent = 0;
}

static int command_trace(int argc, char **argv)
{
	int i, j;

	CPRINTF("===BEGIN TRACE===\n");

	for (i = 0, j = curevent; i < ARRAY_SIZE(events); i++, j++) {
		if (j == ARRAY_SIZE(events))
			j = 0;

		if (!events[j].evt)
			continue;

		CPRINTF("%u,%c,%s\n",
			events[j].time, events[j].type, events[j].evt);
		cflush();
	}

	CPRINTF("===END TRACE===\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(trace, command_trace,
			"",
			"Dump trace buffer to console");
