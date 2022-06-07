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
#include "ap_power/ap_power_interface.h"
#include "chipset.h"
#include "emul/emul_power_signals.h"
#include "test_state.h"


static struct ap_power_ev_callback test_cb;
static int power_resume_count;
static int power_start_up_count;
static int power_hard_off_count;
static int power_shutdown_count;
static int power_shutdown_complete_count;
static int power_suspend_count;

static void emul_ev_handler(struct ap_power_ev_callback *callback,
		       struct ap_power_ev_data data)
{
	switch(data.event) {
	case AP_POWER_RESUME:
		power_resume_count++;
		break;

	case AP_POWER_STARTUP:
		power_start_up_count++;
		break;

	case AP_POWER_HARD_OFF:
		power_hard_off_count++;
		break;

	case AP_POWER_SHUTDOWN:
		power_shutdown_count++;
		break;

	case AP_POWER_SHUTDOWN_COMPLETE:
		power_shutdown_complete_count++;
		break;

	case AP_POWER_SUSPEND:
		power_suspend_count++;
		break;
	default:
		break;
	};
}

ZTEST(ap_pwrseq, test_ap_pwrseq_0)
{
	power_signal_emul_load(TP_G3_TO_S0);

	k_msleep(500);

	zassert_equal(1, power_start_up_count, "AP_POWER_STARTUP event not generated");
	zassert_equal(1, power_resume_count, "AP_POWER_RESUME event not generated");
}

ZTEST(ap_pwrseq, test_ap_pwrseq_1)
{
	power_signal_emul_load(TP_S0_POWER_FAIL);

	k_msleep(500);
	zassert_equal(1, power_shutdown_count, "AP_POWER_SHUTDOWN event not generated");
	zassert_equal(1, power_shutdown_complete_count, "AP_POWER_SHUTDOWN_COMPLETE event not generated");
	zassert_equal(0, power_suspend_count, "AP_POWER_SUSPEND event generated");
}

ZTEST(ap_pwrseq, test_ap_pwrseq_2)
{
	power_signal_emul_load(TP_G3_TO_S0_POWER_DOWN);

	ap_power_exit_hardoff();
	k_msleep(1000);
	zassert_equal(2, power_shutdown_count, "AP_POWER_SHUTDOWN event not generated");
	zassert_equal(2, power_shutdown_complete_count, "AP_POWER_SHUTDOWN_COMPLETE event not generated");
	zassert_equal(1, power_suspend_count, "AP_POWER_SUSPEND event generated");
}

void ap_pwrseq_after_test(void *data)
{
	power_signal_emul_unload();
}

void *ap_pwrseq_setup_suite(void)
{
	ap_power_ev_init_callback(&test_cb, emul_ev_handler, AP_POWER_RESUME |
				  AP_POWER_STARTUP | AP_POWER_HARD_OFF |
				  AP_POWER_SUSPEND | AP_POWER_SHUTDOWN |
				  AP_POWER_SHUTDOWN_COMPLETE);
	
	ap_power_ev_add_callback(&test_cb);

	return NULL;
}

void ap_pwrseq_teardown_suite(void *data)
{
	ap_power_ev_remove_callback(&test_cb);
}

ZTEST_SUITE(ap_pwrseq, ap_power_predicate_post_main, ap_pwrseq_setup_suite,
	    NULL , ap_pwrseq_after_test , NULL);
