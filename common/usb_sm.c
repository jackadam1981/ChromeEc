/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "task.h"
#include "usb_sm.h"
#include "usb_pd.h"

void init_state(int port, struct sm_obj *obj, sm_state target)
{
	obj->last_state = (void *)0;
	obj->task_state = target;
	obj->task_state(port, ENTRY_SIG);
}

void set_state(int port, struct sm_obj *obj, sm_state target)
{
	obj->task_state(port, EXIT_SIG);
	obj->last_state = obj->task_state;
	obj->task_state = target;
	obj->task_state(port, ENTRY_SIG);
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
}
