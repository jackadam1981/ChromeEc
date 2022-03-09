/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Several components need to perform actions at a change of AP power state.
 * This was traditionally implemented using hook_notify.
 * Here the responsibility of calling this is on each event generator.
 * This implements API to let a client register a callback to be notified
 * for a particular state it is interested in*/

/* The states that are supported to be notified are below */

#include <logging/log.h>
#include "state_notifier.h"
#include <sys/__assert.h>

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_DBG);

sys_slist_t ap_pwrseq_cb_list[NOTIFY_CHIPSET_COUNT];

/* This could be split into init and add callback */
int ap_pwrseq_init_callback(struct ap_pwrseq_callback *cb,
				ap_pwrseq_handler_t handler,
				int priority,
				uint32_t ap_power_state_mask)
{
	__ASSERT(cb->handler, "No callback handler");
	__ASSERT(mask >= 0 && mask < NOTIFY_CHIPSET_COUNT,
				"Invalid pwrseq notifier");

	cb->order = priority;
	cb->handler = handler;
	cb->ap_power_state_mask = ap_power_state_mask;

	sys_slist_append(&ap_pwrseq_cb_list[ap_power_state_mask], &cb->node);
	return 0;
}

/* API to notify the client to fire callback */
void ap_pwrseq_notify(int notify_chipset_state)
{
	/*
	 * Loop through all registered callbacks,
	 * invoke callbacks that registered for chipset_state 
	 */
	struct ap_pwrseq_callback *cb, *temp;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&ap_pwrseq_cb_list[notify_chipset_state],
								cb, temp, node) {
		__ASSERT(cb->handler, "No callback handler!");
		cb->handler();
	}
}
