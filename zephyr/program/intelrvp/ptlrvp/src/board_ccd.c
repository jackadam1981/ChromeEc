/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"

#include <zephyr/init.h>

LOG_MODULE_REGISTER(board_ccd, LOG_LEVEL_INF);

#define BOARD_CCD_DELAY	 K_MSEC(800)

static struct k_work_delayable ccd_cntrl_work;

static void board_ccd_cntrl_handler(struct k_work *ccd_work)
{
	const struct gpio_dt_spec *spec = GPIO_DT_FROM_NODELABEL(ccd_mode_odl);
	static bool prev;
	bool cur = !!gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(ccd_cntl));

	if (cur != prev) {
		if (cur) {
			LOG_INF("CCD_CNTRL_HANDLER -- Input");
			gpio_pin_configure(spec->port, spec->pin, GPIO_INPUT);
		} else  {
			LOG_INF("CCD_CNTRL_HANDLER -- High");
			gpio_pin_configure(spec->port, spec->pin, GPIO_OUTPUT_HIGH);
		}
		prev = cur;
	}

	k_work_schedule(&ccd_cntrl_work, BOARD_CCD_DELAY);
}

static int board_ccd_cntrl(void)
{
	k_work_init_delayable(&ccd_cntrl_work, board_ccd_cntrl_handler);

	board_ccd_cntrl_handler((struct k_work *)&ccd_cntrl_work);

	return 0;
}
SYS_INIT(board_ccd_cntrl, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
