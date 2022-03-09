/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <kernel.h>
#include <logging/log.h>

#include <ap_power.h>
#include <ap_events.h>

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

static sys_slist_t callbacks;

static int ap_ev_manage_callback(struct ap_ev_callback *cb, bool set)
{
	__ASSERT(callback, "No callback!");
	__ASSERT(callback->handler, "No callback handler!");

	if (!sys_slist_is_empty(&callbacks)) {
		if (!sys_slist_find_and_remove(&callbacks, &cb->node)) {
			if (!set) {
				return -EINVAL;
			}
		}
	}
	if (set) {
		sys_slist_prepend(&callbacks, &cb->node);
	}
	return 0;
}

int ap_ev_add_callback(struct ap_ev_callback *cb)
{
	return ap_ev_manage_callback(cb, true);
}

int ap_ev_remove_callback(struct ap_ev_callback *cb)
{
	return ap_ev_manage_callback(cb, false);
}

/*
 * Run the callback list
 */
void ap_ev_send_callbacks(enum ap_events event)
{
	struct ap_ev_data data;
	struct ap_ev_callback *cb, *tmp;

	data.event = event;
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&callbacks, cb, tmp, node) {
		if (cb->events & event) {
			cb->handler(cb, data);
		}
	}
}
