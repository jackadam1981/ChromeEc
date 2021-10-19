/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "board.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "cros_version.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "kernel.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_timer.h"
#include "usb_prl_sm.h"
#include "tcpm/tcpm.h"
#include "usb_pe_sm.h"
#include "usb_prl_sm.h"
#include "usb_sm.h"
#include "usb_tc_sm.h"
#include "usbc_ppc.h"
#include <stdbool.h>
#include <stdio.h>

#define USBC_EVENT_TIMEOUT (5 * MSEC)
#define USBC_MIN_EVENT_TIMEOUT (1 * MSEC)

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/*
 * If CONFIG_ASSERT_CCD_MODE_ON_DTS_CONNECT is not defined then
 * _GPIO_CCD_MODE_ODL is not needed. Declare as extern so IS_ENABLED will work.
 */
#ifndef CONFIG_ASSERT_CCD_MODE_ON_DTS_CONNECT
extern int _GPIO_CCD_MODE_ODL;
#else
#define _GPIO_CCD_MODE_ODL GPIO_CCD_MODE_ODL
#endif /* CONFIG_ASSERT_CCD_MODE_ON_DTS_CONNECT */

static uint8_t paused[CONFIG_USB_PD_PORT_MAX_COUNT];

__test_only K_MUTEX_DEFINE(pd_mutex);

/* Used by PD task to signal to unit test of suspend state change. */
__test_only K_CONDVAR_DEFINE(pd_condvar);

/* Used to notify a task to safely suspend itself */
__test_only static volatile bool port_to_suspend[CONFIG_USB_PD_PORT_MAX_COUNT];

__test_only void usbc_pd_task_suspend(task_id_t task)
{
	int port = TASK_ID_TO_PD_PORT(task);

	k_mutex_lock(&pd_mutex, K_FOREVER);

	port_to_suspend[port] = true;

	/* Task might be sleeping, wake it up so we can suspend it our way. */
	tc_start_event_loop(port);

	printf("started suspend condvar wait\n");
	k_condvar_wait(&pd_condvar, &pd_mutex, K_FOREVER);
	printf("condvar was signaled\n");

	k_mutex_unlock(&pd_mutex);
}

__test_only void usbc_pd_task_resume(task_id_t task)
{
	int port = TASK_ID_TO_PD_PORT(task);

	k_mutex_lock(&pd_mutex, K_FOREVER);
	port_to_suspend[port] = false;
	k_thread_resume(task_get_zephyr_tid(task));

	k_condvar_wait(&pd_condvar, &pd_mutex, K_FOREVER);

	k_mutex_unlock(&pd_mutex);
}

void tc_pause_event_loop(int port)
{
	if(IS_ENABLED(CONFIG_ZTEST))
		k_mutex_lock(&pd_mutex, K_FOREVER);

	paused[port] = 1;

	if(IS_ENABLED(CONFIG_ZTEST))
		k_mutex_unlock(&pd_mutex);
}

bool tc_event_loop_is_paused(int port)
{
	return paused[port];
}

void tc_start_event_loop(int port)
{
	if(IS_ENABLED(CONFIG_ZTEST))
		k_mutex_lock(&pd_mutex, K_FOREVER);

	/*
	 * Only generate TASK_EVENT_WAKE event if state
	 * machine is transitioning to un-paused
	 */
	if (paused[port]) {
		paused[port] = 0;
		task_set_event(PD_PORT_TO_TASK_ID(port), TASK_EVENT_WAKE);
	}

	if(IS_ENABLED(CONFIG_ZTEST))
		k_mutex_unlock(&pd_mutex);
}

static void pd_task_init(int port)
{
	if (IS_ENABLED(CONFIG_USB_TYPEC_SM))
		tc_state_init(port);
	paused[port] = 0;

	/*
	 * Since most boards configure the TCPC interrupt as edge
	 * and it is possible that the interrupt line was asserted between init
	 * and calling set_state, we need to process any pending interrupts now.
	 * Otherwise future interrupts will never fire because another edge
	 * never happens. Note this needs to happen after set_state() is called.
	 */
	if (IS_ENABLED(CONFIG_HAS_TASK_PD_INT))
		schedule_deferred_pd_interrupt(port);

	/*
	 * GPIO_CCD_MODE_ODL must be initialized with GPIO_ODR_HIGH
	 * when CONFIG_ASSERT_CCD_MODE_ON_DTS_CONNECT is enabled
	 */
	if (IS_ENABLED(CONFIG_ASSERT_CCD_MODE_ON_DTS_CONNECT))
		ASSERT(gpio_get_default_flags(_GPIO_CCD_MODE_ODL) &
		       GPIO_ODR_HIGH);
}

static int pd_task_timeout(int port)
{
	int timeout;

	if (paused[port])
		timeout = -1;
	else {
		timeout = pd_timer_next_expiration(port);
		if (timeout < 0 || timeout > USBC_EVENT_TIMEOUT)
			timeout = USBC_EVENT_TIMEOUT;
		else if (timeout < USBC_MIN_EVENT_TIMEOUT)
			timeout = USBC_MIN_EVENT_TIMEOUT;
	}
	return timeout;
}

static bool pd_task_loop(int port)
{
	if (IS_ENABLED(CONFIG_ZTEST) && port_to_suspend[port])
		return false;

	printf("port=%d before task_wait\n", port);
	/* wait for next event/packet or timeout expiration */
	const uint32_t evt = task_wait_event(pd_task_timeout(port));
	printf("port=%d after task_wait\n", port);

	/* Manage expired PD Timers on timeouts */
	if (evt & TASK_EVENT_TIMER)
		pd_timer_manage_expired(port);

	/*
	 * Re-use TASK_EVENT_RESET_DONE in tests to restart the USB task
	 * if this code is running in a unit test.
	 */
	if (IS_ENABLED(TEST_BUILD) && (evt & TASK_EVENT_RESET_DONE))
		return false;

	/* handle events that affect the state machine as a whole */
	if (IS_ENABLED(CONFIG_USB_TYPEC_SM))
		tc_event_check(port, evt);

	/*
	 * run port controller task to check CC and/or read incoming
	 * messages
	 */
	if (IS_ENABLED(CONFIG_USB_PD_TCPC))
		tcpc_run(port, evt);

	/* Run policy engine state machine */
	if (IS_ENABLED(CONFIG_USB_PE_SM))
		pe_run(port, evt, tc_get_pd_enabled(port));

	/* Run protocol state machine */
	if (IS_ENABLED(CONFIG_USB_PRL_SM) || IS_ENABLED(CONFIG_TEST_USB_PE_SM))
		prl_run(port, evt, tc_get_pd_enabled(port));

	/* Run TypeC state machine */
	if (IS_ENABLED(CONFIG_USB_TYPEC_SM))
		tc_run(port);
	
	return true;
}

void pd_task(void *u)
{
	int port = TASK_ID_TO_PD_PORT(task_get_current());

	/*
	 * If port does not exist, return
	 */
	if (port >= board_get_usb_pd_port_count())
		return;

	while (1) {
		if (IS_ENABLED(CONFIG_ZTEST) && port_to_suspend[port]) {
			k_mutex_lock(&pd_mutex, K_FOREVER);
			k_condvar_signal(&pd_condvar);
			k_mutex_unlock(&pd_mutex);

			k_thread_suspend(task_get_zephyr_tid(task_get_current()));

			k_condvar_signal(&pd_condvar);
		}

		pd_timer_init(port);
		pd_task_init(port);

		/* As long as pd_task_loop returns true, keep running the loop.
		 * pd_task_loop returns false when the code needs to re-init
		 * the task, so once the code breaks out of the inner while
		 * loop, the re-init code at the top of the outer while loop
		 * will run.
		 */
		while (pd_task_loop(port))
			continue;
	}
}
