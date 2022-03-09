/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Several task need to perform actions at a change of AP power state.
 * This was traditionally implemented using hook_notify.
 * Here the responsibility of calling this is on each event generator.
 * This implements API to let a client register a callback to be notified
 * for a particular state it is interested in.
 */

/* The states that are supported to be notified are below */
#ifndef STATE_NOTIFIER_H_
#define STATE_NOTIFIER_H_

#include <sys/slist.h>
#include <zephyr/types.h>

enum ap_pwrseq_notify_type {
	/*
	 * Initialization for components such as PMU to be done before host
	 * chipset/AP starts up.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_PRE_INIT,

	/* System is starting up.  All suspend rails are now on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_STARTUP,

	/*
	 * System is resuming from suspend, or booting and has reached the
	 * point where all voltage rails are on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_RESUME,

	/*
	 * System is suspending, or shutting down; all voltage rails are still
	 * on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_SUSPEND,

#ifdef CONFIG_CHIPSET_RESUME_INIT_HOOK
	/*
	 * Initialization before the system resumes, like enabling the SPI
	 * driver such that it can receive a host resume event.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_RESUME_INIT,

	/*
	 * System has suspended. It is paired with CHIPSET_RESUME_INIT hook,
	 * like reverting the initialization of the SPI driver.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_SUSPEND_COMPLETE,
#endif

	/*
	 * System is shutting down.  All suspend rails are still on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_SHUTDOWN,

	/*
	 * System has already shut down. All the suspend rails are already off.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_SHUTDOWN_COMPLETE,

	/*
	 * System is in G3.  All power rails are now turned off.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_HARD_OFF,

	/*
	 * System reset in S0.  All rails are still up.
	 *
	 * Hook routines are called from the chipset task.
	 */
	NOTIFY_CHIPSET_RESET,
	NOTIFY_CHIPSET_COUNT,
};

typedef void (*ap_pwrseq_handler_t) (void);

struct ap_pwrseq_callback {
    sys_snode_t node;     /* For internal list management */
    ap_pwrseq_handler_t handler; /* Callback routine */
    uint32_t ap_power_state_mask;/* Mask of AP power states */
    int order;
};

/* API to register callback for the interested chipset state */
int ap_pwrseq_init_callback(struct ap_pwrseq_callback *callback,
			ap_pwrseq_handler_t handler,
			int priority,
			uint32_t ap_power_state_mask);

void ap_pwrseq_notify(int notify_chipset_state);

#endif /* STATE_NOTIFIER_H */
