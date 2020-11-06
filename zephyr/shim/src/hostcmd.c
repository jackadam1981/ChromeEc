/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "host_command.h"

/*
 * TODO(b:172678200): Implement host commands shim.  This is a minimal
 * stub implementation for now just to get some things compiling.
 */

static host_event_t events;

void host_set_single_event(enum host_event_code event)
{
	events |= EC_HOST_EVENT_MASK(event);
}

void host_clear_events(host_event_t mask)
{
	events &= ~mask;
}

host_event_t host_get_events(void)
{
	return events;
}

int host_is_event_set(enum host_event_code event)
{
	return (events & EC_HOST_EVENT_MASK(event)) ? 1 : 0;
}
