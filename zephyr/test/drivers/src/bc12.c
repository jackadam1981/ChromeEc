/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include "emul/emul_pi3usb9201.h"

#include "timer.h"
#include "usb_charge.h"

#define EMUL_LABEL DT_NODELABEL(pi3usb9201_emul)

#define PI3USB9201_ORD DT_DEP_ORD(EMUL_LABEL)

enum pi3usb9201_client_sts {
	CHG_OTHER = 0,
	CHG_2_4A,
	CHG_2_0A,
	CHG_1_0A,
	CHG_RESERVED,
	CHG_CDP,
	CHG_SDP,
	CHG_DCP,
};

static void test_bc12_pi3usb9201(void)
{
	struct i2c_emul *emul;

	emul = pi3usb9201_emul_get(PI3USB9201_ORD);

	// todo:
	// task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_CC_OPEN);
	// expect bc12_power_down
	// task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_DR_UFP);
	// expect bc12_detect_start

	pi3usb9201_emul_set_reg(emul, PI3USB9201_REG_CLIENT_STS, 1 << CHG_CDP);
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12);
	// expect charge_manager_update_charge(CHARGE_SUPPLIER_BC12_CDP, USB_CHARGER_MAX_CURR_MA)

	msleep(1000);
	zassert_equal(0, 1, NULL);
}

void test_suite_bc12(void)
{
	ztest_test_suite(bc12,
			 ztest_user_unit_test(test_bc12_pi3usb9201));
	ztest_run_test_suite(bc12);
}
