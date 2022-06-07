/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/espi_emul.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/zephyr.h>
#include <ztest.h>

#include "ap_power/ap_power.h"
#include "ap_power/ap_power_emul.h"
#include "ap_power/ap_power_events.h"
#include "chipset.h"
#include "power_signals.h"
#include "test_state.h"

bool flag;

static void emul_ev_handler(struct ap_power_ev_callback *callback,
		       struct ap_power_ev_data data)
{
	flag = 1;
}

ZTEST(ap_pwrseq, test_power_up)
{
	struct ap_power_ev_callback cb;

	ap_power_ev_init_callback(&cb, emul_ev_handler, AP_POWER_RESUME);
	ap_power_ev_add_callback(&cb);

	chipset_exit_hard_off();

	k_msleep(1000);
	ap_power_ev_remove_callback(&cb);

}

static void *set_initial_signals_state(void)
{
	power_signal_emul_load(TEST_S0);

	return NULL;
}

ZTEST_SUITE(ap_pwrseq, ap_power_predicate_post_main,
	    set_initial_signals_state, NULL , NULL, NULL);
