/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Keyboard haptic feedback */

#include "common.h"
#include "console.h"
#include "dac_chip.h"
#include "feedback_samples.h"
#include "gpio.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_KEYSCAN, outstr)
#define CPRINTF(format, args...) cprintf(CC_KEYSCAN, format, ## args)

static int feedback_row;
static int feedback_col;

void trigger_feedback(int gpio_row, int gpio_col)
{
	feedback_row = gpio_row;
	feedback_col = gpio_col;
	task_wake(TASK_ID_HAPTICS);
}

static void produce_feedback(int gpio_row, int gpio_col)
{
	/* turn on the high-voltage amplifier and its LED */
	gpio_set_level(GPIO_AMP_WARN, 0);
	gpio_set_level(GPIO_AMP_EN, 1);
	/* select the proper point for the feedback */
	gpio_set_level(gpio_col, 1);
	gpio_set_level(gpio_row, 1);
	/* play haptics effect */
	dac_play_samples(dac_samples, ARRAY_SIZE(dac_samples), SAMPLE_RATE);
	/* turn off feedback */
	gpio_set_level(gpio_row, 0);
	gpio_set_level(gpio_col, 0);
	/* turn off the high-voltage amplifier and its LED */
	gpio_set_level(GPIO_AMP_EN, 0);
	gpio_set_level(GPIO_AMP_WARN, 1);
}

void feedback_task(void)
{
	CPRINTF("Start feedback task ...\n");

	while (1) {
		/* wait for the next key event */
		task_wait_event(-1);
		/* time to produce a nice effect */
		produce_feedback(feedback_row, feedback_col);
	}
}
