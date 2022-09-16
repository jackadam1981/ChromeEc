/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>
#include <zephyr/fff.h>

#include "charger.h"
#include "charge_manager.h"
#include "driver/charger/rt9490.h"
#include "emul/emul_rt9490.h"
#include "i2c.h"
#include "test/drivers/test_state.h"
#include "timer.h"
#include "usb_charge.h"

static const struct emul *emul = EMUL_DT_GET(DT_NODELABEL(rt9490));
static const int chgnum = CHARGER_SOLO;

/* TODO: Disable port 1 until we figure out why it crashes */
int board_get_usb_pd_port_count(void)
{
	return 1;
}

FAKE_VALUE_FUNC(bool, rt9490_is_non_pd_sink, int);

void run_bc12_test(int reg_value, enum charge_supplier expected_result)
{
	/* simulate plug, expect bc12 detection starting. */
	rt9490_is_non_pd_sink_fake.return_val = true;
	usb_charger_task_set_event(0, USB_CHG_EVENT_VBUS);
	msleep(1);
	zassert_true(rt9490_emul_peek_reg(emul, RT9490_REG_CHG_CTRL2) &
			     RT9490_BC12_EN,
		     NULL);

	/*
	 * simulate triggering interrupt on bc12 detection done, and verify the
	 * result.
	 */
	zassert_ok(rt9490_emul_write_reg(emul, RT9490_REG_CHG_IRQ_FLAG1,
					 RT9490_BC12_DONE_FLAG));
	zassert_ok(
		rt9490_emul_write_reg(emul, RT9490_REG_CHG_STATUS1, reg_value));
	rt9490_interrupt(0);
	/* wait for deferred task scheduled, this takes longer. */
	msleep(500);
	zassert_false(rt9490_emul_peek_reg(emul, RT9490_REG_CHG_CTRL2) &
			      RT9490_BC12_EN,
		      NULL);
	zassert_equal(charge_manager_get_supplier(), expected_result, NULL);

	/* simulate unplug */
	rt9490_is_non_pd_sink_fake.return_val = false;
	usb_charger_task_set_event(0, USB_CHG_EVENT_VBUS);
	msleep(1);
	zassert_equal(charge_manager_get_supplier(), CHARGE_SUPPLIER_NONE,
		      NULL);
}

ZTEST(rt9490_bc12, test_detection_flow)
{
	/* make charge manager thinks port 0 is chargable */
	msleep(500);
	usb_charger_task_set_event(0, USB_CHG_EVENT_DR_UFP);
	charge_manager_update_dualrole(0, CAP_DEDICATED);
	zassert_equal(charge_manager_get_supplier(), CHARGE_SUPPLIER_NONE,
		      NULL);
	msleep(1);

	run_bc12_test(RT9490_DCP << RT9490_VBUS_STAT_SHIFT,
		      CHARGE_SUPPLIER_BC12_DCP);
	run_bc12_test(RT9490_CDP << RT9490_VBUS_STAT_SHIFT,
		      CHARGE_SUPPLIER_BC12_CDP);
	run_bc12_test(RT9490_SDP << RT9490_VBUS_STAT_SHIFT,
		      CHARGE_SUPPLIER_BC12_SDP);
	run_bc12_test(0xA, CHARGE_SUPPLIER_NONE); /* unknown type */
}

static void reset_emul(void *fixture)
{
	rt9490_emul_reset_regs(emul);
	rt9490_drv.init(chgnum);
	RESET_FAKE(rt9490_is_non_pd_sink);
}

ZTEST_SUITE(rt9490_bc12, drivers_predicate_post_main, NULL, reset_emul, NULL,
	    NULL);
