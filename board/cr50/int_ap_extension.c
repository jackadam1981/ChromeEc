/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * INT_AP_L extension
 */

#include "config.h"
#include "gpio.h"
#include "hwtimer.h"
#include "registers.h"
#include "stdbool.h"
#include "task.h"

/* Minimum time length (in usec) of INT_AP_L assertion that AP requires */
#define MIN_USEC_INT_AP_PULSE		4
#define DEBUG_EXTENDED_INT_AP		1

/* TODO: get a fine formular for TIMEHS */
#define USEC_TO_TIMEHS_LOAD(usec)	((usec) * 23)

/*
 * Minimum time duration to keep INT_AP_L level.
 * Zero value means that INT_AP_L extension is disabled.
 */
static uint32_t min_duration_int_ap_;

/* TIMEHS_LOAD value for extended INT_AP_L. */
static uint32_t timehs_load_;

/* Most recent timestamp when INT_AP_L went high. */
static uint32_t last_time_deassert_;

enum timer_sts_e {
	NON_SCHEDULED = 0,
	DEASSERT_SCHEDULED,
	ASSERT_SCHEDULED,
};
static enum timer_sts_e int_ap_timer_sts_;

/* Activate timer with the given countdown load, and change the timer status. */
static void activate_timer_(uint32_t timer_load, enum timer_sts_e status)
{
	GR_TIMEHS_LOAD(0, 1) = timer_load;
	GR_TIMEHS_CONTROL(0, 1) =
			GC_TIMEHS_TIMER1CONTROL_ONESHOT_MASK |
			GC_TIMEHS_TIMER1CONTROL_SIZE_MASK |
			GC_TIMEHS_TIMER1CONTROL_INTENABLE_MASK |
			GC_TIMEHS_TIMER1CONTROL_ENABLE_MASK;
	int_ap_timer_sts_ = status;
}

static void assert_gpio_int_ap_(void)
{
	gpio_set_level(GPIO_INT_AP_L, 0);

	activate_timer_(timehs_load_, DEASSERT_SCHEDULED);
}

static void deassert_gpio_int_ap_(void)
{
	gpio_set_level(GPIO_INT_AP_L, 1);
	last_time_deassert_ = __hw_clock_source_read();

	/* Disable Timer */
	GR_TIMEHS_CONTROL(0, 1) = 0;
	int_ap_timer_sts_ = NON_SCHEDULED;
}

void timer_int_ap_irq_handler(void)
{
	/* Clear interrupt status of TIMEHS0 TIMER1 */
	GR_TIMEHS_INTCLR(0, 1) = 1;

	switch (int_ap_timer_sts_) {
	case DEASSERT_SCHEDULED:
		deassert_gpio_int_ap_();
		break;

	case ASSERT_SCHEDULED:
		assert_gpio_int_ap_();
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
			assert_gpio_int_ap_();
		else
			/* Let's deassert INT_AP_L diff_usec later. */
			activate_timer_(USEC_TO_TIMEHS_LOAD(diff_usec),
					ASSERT_SCHEDULED);
	}

	return !!min_duration_int_ap_;
}

void ap_stop_ack_completion(void)
{
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
	if (min_duration_int_ap_) {
		timehs_load_ = USEC_TO_TIMEHS_LOAD(min_duration_int_ap_);

		/* Enable IRQNUM_TIMEHS0_TIMINT1 */
		task_enable_irq(GC_IRQNUM_TIMEHS0_TIMINT1);
	}
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

void int_ap_extension_init(void)
{
	deassert_gpio_int_ap_();
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
