/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "gpio.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"
#include "host_command.h"

#include <zephyr/kernel.h>

#define ECREG(x)                    (*((volatile unsigned char *)(x)))
#define IT8XXX2_GPIO_BASE           0x00F01600
#define IT8XXX2_GPIO_DATA           ECREG(IT8XXX2_GPIO_BASE + 0x01)

void test_gpa2_interrupt(enum gpio_signal signal) {
	/* set GPA1 output high */
	IT8XXX2_GPIO_DATA |= BIT(1);
	printk("GPA2 INT");
}

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
int main(void)
{
	ec_app_main();

	/* Set GPA2_INT falling edge trigger & callback test_gpa2_interrupt() */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_test_gpa2));

	/*
	 * GPA1 init output high (declare in gpio.dtsi)
	 * Here, set GPA1 output low to trigger GPA2_INT
	 */
	IT8XXX2_GPIO_DATA &= ~BIT(1);

	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		host_command_main();
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}

	return 0;
}
/* LCOV_EXCL_STOP */
