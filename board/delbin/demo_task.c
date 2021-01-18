/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "task.h"
#include "timer.h"

#define CPRINTS(format, args...) do {\
	cprintf(CC_DEMO, "%s:", task_get_name(task_get_current())); \
	cprintf(CC_DEMO, format, ##args); \
	cprintf(CC_DEMO, "\n"); \
	} while (0)

/*
 * Section for adding the HOOK_INIT, the callback will be called
 * by the hook task and prior to the demo_task(). Please observe
 * the output log.
 */

/*
 * Please try the other module priority like HOOK_PRIO_INIT_DMA,
 * HOOK_PRIO_INIT_LPC and so on. The priority can be found from
 * ./include/hooks.h
 */

static void demo_hook_init_priority_first(void)
{
	CPRINTS("----%s is called----", __func__);
}
DECLARE_HOOK(HOOK_INIT, demo_hook_init_priority_first, HOOK_PRIO_FIRST);

static void demo_hook_init_priority_default(void)
{
	CPRINTS("----%s is called----", __func__);
}
DECLARE_HOOK(HOOK_INIT, demo_hook_init_priority_default, HOOK_PRIO_DEFAULT);

static void demo_hook_init_priority_last(void)
{
	CPRINTS("----%s is called----", __func__);
}
DECLARE_HOOK(HOOK_INIT, demo_hook_init_priority_last, HOOK_PRIO_LAST);

/*
 * Section for adding the HOOK_CHIPSET_SUSPEND/HOOK_CHIPSET_RESUME,
 * the callback will be called by the task which calles
 * hook_notify(HOOK_CHIPSET_SUSPEND/HOOK_CHIPSET_RESUME). In this case,
 * the chipset_task will call the callback function. Similar behavior
 * for the HOOK_* type.
 */
static void demo_hook_suspend(void)
{
	CPRINTS("----%s is called----", __func__);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, demo_hook_suspend, HOOK_PRIO_DEFAULT);

static void demo_hook_resume(void)
{
	CPRINTS("----%s is called----", __func__);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, demo_hook_resume, HOOK_PRIO_DEFAULT);

/*
 * Section for adding the HOOK_DEFFERED
 */

static int count;
/* Need to declare the function to avoid the build error */
static void demo_hook_deferred(void);
DECLARE_DEFERRED(demo_hook_deferred);

static void demo_hook_deferred(void)
{
	CPRINTS("----%s is called %d times----", __func__, ++count);
	CPRINTS("----wakeup demo_task----");
	/*
	 * Note: after waking up the demo_task, hook_task will be preempted
	 * because the priority of demo_task is greater than hook_task.
	 */
	task_wake(TASK_ID_DEMO_TASK);
	CPRINTS("----register %s with 3s----", __func__);
	/* Register the callback function again, run it after 3s */
	hook_call_deferred(&demo_hook_deferred_data, 3 * SECOND);
}

void demo_task(void *u)
{
	CPRINTS("----%s starting----", __func__);
	while (1) {
		if (!count) {
			CPRINTS("----register demo_hook_deferred "
				"with 10s----");
			hook_call_deferred(&demo_hook_deferred_data,
						10 * SECOND);
		}
		CPRINTS("----waiting someone to wake me up----");
		task_wait_event(-1);
		CPRINTS("----%s resumed----", __func__);
	}
	CPRINTS("----never go to here----");
}
