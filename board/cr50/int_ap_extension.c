/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * INT_AP_L extension
 */

#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hwtimer.h"
#include "registers.h"
#include "stdbool.h"
#include "task.h"

/* TODO: delete this before commit */
#define DEBUG_EXTENDED_INT_AP		1
#define USEC_TO_TIMEHS_LOAD(usec)	((usec) *  (PCLK_FREQ/1000000))

#define CPRINTS(format, args...) cprints(CC_TASK, "INT_AP: " format, ## args)

/*
 * Minimum duration of the INT_AP_L pulse in microseconds.
 * Zero value means that INT_AP_L extension is disabled.
 */
static uint16_t min_duration_int_ap_;

/* Most recent timestamp when INT_AP_L went high. */
static uint32_t last_time_deassert_;

enum int_ap_sts_e {
	DEASSERTED,
	ASSERTED,
};
/* NOTE: int_ap_sts_ should be changed in shedule_timer_() only. */
static enum int_ap_sts_e int_ap_sts_;

/*
 * Schedule timer to assert, deassert INT_AP or cancel any scheduled action.
 * Note: this function should be called in an isr only.
 *
 * @param usec     countdown in microseconds. Must be positive.
 */
static void schedule_timer_(uint32_t usec)
{
	if (!usec) {
		CPRINTS("ERR: %s: usec should be positive.", __func__);
		return;
	}

	GR_TIMEHS_LOAD(0, 1) = USEC_TO_TIMEHS_LOAD(usec);
	GR_TIMEHS_CONTROL(0, 1) = GC_TIMEHS_TIMER1CONTROL_ONESHOT_MASK |
				  GC_TIMEHS_TIMER1CONTROL_SIZE_MASK |
				  GC_TIMEHS_TIMER1CONTROL_INTENABLE_MASK |
				  GC_TIMEHS_TIMER1CONTROL_ENABLE_MASK;
}

void timer_int_ap_irq_handler(void)
{
	/* Clear interrupt status of TIMEHS0 TIMER1. */
	GR_TIMEHS_INTCLR(0, 1) = 1;

	if (int_ap_sts_ == ASSERTED) {
		/* Deassert INT_AP_L. */
		gpio_set_level(GPIO_INT_AP_L, 1);
		int_ap_sts_ = DEASSERTED;

		last_time_deassert_ = __hw_clock_source_read();

		/* Disable Timer. */
		GR_TIMEHS_CONTROL(0, 1) = 0;
	} else {	/* int_ap_sts == DEASSERTED */
		/* Assert INT_AP_L. */
		gpio_set_level(GPIO_INT_AP_L, 0);
		int_ap_sts_ = ASSERTED;

		/* Schedule to deassert INT_AP_L in min_duration_int_ap_. */
		schedule_timer_(min_duration_int_ap_);
	}
}
DECLARE_IRQ(GC_IRQNUM_TIMEHS0_TIMINT1, timer_int_ap_irq_handler, 1);

int ap_start_ack_completion(void)
{
	uint32_t diff_usec;

	if (!min_duration_int_ap_)
		return 0;

	diff_usec = __hw_clock_source_read() - last_time_deassert_;
	/*
	 * If INT_AP_L has been deasserted for min_duration_int_ap_ or
	 * longer, then let's assert it asap.
	 */
	if (diff_usec >= min_duration_int_ap_)
		timer_int_ap_irq_handler();
	else
		schedule_timer_(diff_usec);

	return 1;
}

void ap_stop_ack_completion(void)
{
	/* If INT_AP_L is asserted now, let's deassert it now. */
	if (int_ap_sts_ == ASSERTED)
		timer_int_ap_irq_handler();
}

void int_ap_extension_enable(void)
{
	if (min_duration_int_ap_)
		/* Enable IRQNUM_TIMEHS0_TIMINT1 */
		task_enable_irq(GC_IRQNUM_TIMEHS0_TIMINT1);
}

void int_ap_extension_disable(void)
{
	/* Enable IRQNUM_TIMEHS0_TIMINT1 */
	task_disable_irq(GC_IRQNUM_TIMEHS0_TIMINT1);
}

int int_ap_extension_set_duration(int usec)
{
	min_duration_int_ap_ = usec;

	int_ap_extension_enable();

	return EC_SUCCESS;
}

#ifdef DEBUG_EXTENDED_INT_AP
/* For debugging purpose only */
static int command_int_ap(int argc, char **argv)
{
	if (argc > 2)
		return EC_ERROR_PARAM_COUNT;

	if (argc == 2) {
		int usec;
		char *e;

		usec = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		int_ap_extension_set_duration(usec);
	}

	ccprintf("INT_AP_L duration extension: ");
	if (min_duration_int_ap_)
		ccprintf("enabled, %d usec\n", min_duration_int_ap_);
	else
		ccprintf("disabled\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(int_ap, command_int_ap,
			"[usec|0]]",
			"display or change INT_AP_L duration."
			" setting 0 to disable.");
#endif  /* DEBUG_EXTENDED_INT_AP */
