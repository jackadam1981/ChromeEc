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

enum timer_sts_e {
	NONE_SCHEDULED = 0,
	DEASSERT_SCHEDULED,
	ASSERT_SCHEDULED,
};
/* NOTE: int_ap_timer_sts_ should be changed in shedule_timer_() only. */
static enum timer_sts_e int_ap_timer_sts_;

/*
 * Schedule timer to assert, deassert INT_AP or cancel any scheduled action.
 *
 * @param status   DEASSERT_SCHEDULE to deassert INT_AP_L in usec, or
 *                 ASSERT_SCHEDULE to assert INT_AP_L in usec, or
 *                 NONE_SCHEDULED to cancel any scheduled action right now.
 *
 * @param usec     countdown in microseconds.
 *                 if status is NONE_SCHEDULED, usec shall be ignored.
 *
 * @return Boolean true if it was scheduled, or false otherwise.
 */
static bool schedule_timer_(enum timer_sts_e status, uint32_t usec)
{
	interrupt_disable();

	if (status == NONE_SCHEDULED) {
		/* Whatever is scheduled earlier, just cancel it. */
		/* Disable Timer */
		GR_TIMEHS_CONTROL(0, 1) = 0;
	} else  {
		/*
		 * If ASSERT schedule is requested while DEASSERT is already
		 * scheduled or vice versa, do not process it unless the caller
		 * is IRQ handler.
		 */
		if (!in_interrupt_context() &&
		    int_ap_timer_sts_ != NONE_SCHEDULED &&
		    status != int_ap_timer_sts_) {
			interrupt_enable();
			return false;
		}

		/* If usec is 0, then trigger the IRQ now. */
		if (usec) {
			GR_TIMEHS_LOAD(0, 1) = USEC_TO_TIMEHS_LOAD(usec);
			GR_TIMEHS_CONTROL(0, 1) =
					GC_TIMEHS_TIMER1CONTROL_ONESHOT_MASK |
					GC_TIMEHS_TIMER1CONTROL_SIZE_MASK |
					GC_TIMEHS_TIMER1CONTROL_INTENABLE_MASK |
					GC_TIMEHS_TIMER1CONTROL_ENABLE_MASK;
		} else {
			task_trigger_irq(GC_IRQNUM_TIMEHS0_TIMINT1);
		}
	}

	int_ap_timer_sts_ = status;

	interrupt_enable();
	return true;
}

void timer_int_ap_irq_handler(void)
{
	/* Clear interrupt status of TIMEHS0 TIMER1 */
	GR_TIMEHS_INTCLR(0, 1) = 1;

	switch (int_ap_timer_sts_) {
	case DEASSERT_SCHEDULED:
		gpio_set_level(GPIO_INT_AP_L, 1);
		last_time_deassert_ = __hw_clock_source_read();

		/* Disable Timer */
		schedule_timer_(NONE_SCHEDULED, 0);
		break;

	case ASSERT_SCHEDULED:
		gpio_set_level(GPIO_INT_AP_L, 0);

		schedule_timer_(DEASSERT_SCHEDULED, min_duration_int_ap_);
		break;

	default:
		break;
	}
}
DECLARE_IRQ(GC_IRQNUM_TIMEHS0_TIMINT1, timer_int_ap_irq_handler, 1);

int ap_start_ack_completion(void)
{
	if (min_duration_int_ap_) {
		uint32_t diff_usec;

		diff_usec = __hw_clock_source_read() - last_time_deassert_;
		/*
		 * If INT_AP_L has been deasserted for min_duration_int_ap_ or
		 * longer, then let's assert it asap.
		 */
		if (diff_usec >= min_duration_int_ap_)
			diff_usec = 0;

		/* Let's deassert INT_AP_L diff_usec in diff_usec. */
		if (!schedule_timer_(ASSERT_SCHEDULED, diff_usec)) {
			CPRINTS("WARN: INT_AP assertion failed to schedule.");
#ifdef DEBUG_EXTENDED_INT_AP
			CPRINTS("check if ap_stop_ack_completion() was called"
				" earlier.");
#endif /* DEBUG_EXTENDED_INT_AP */
		}
	}

	return !!min_duration_int_ap_;
}

void ap_stop_ack_completion(void)
{
	if (board_tpm_uses_i2c())
		gpio_disable_interrupt(GPIO_MONITOR_I2CS_SDA);

	/* If INT_AP_L is already deasserted, then do nothing. */
	if (gpio_get_level(GPIO_INT_AP_L))
		return;

	/*
	 * INT_AP_L is being asserted. Let's trigger the timer so that
	 * any scheduled deassertion can be expired and processed now.
	 */
	task_trigger_irq(GC_IRQNUM_TIMEHS0_TIMINT1);
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
