/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include "common.h"
#include "ec_tasks.h"
#include "emul/emul_charger.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_tcpci.h"
#include "hooks.h"
#include "i2c.h"
#include "stubs.h"
#include "tcpci_test_common.h"
#include "test/usb_pe.h"

#include "tcpm/tcpci.h"

#define EMUL_LABEL DT_NODELABEL(tcpci_emul)

void alert_state_func(const struct emul *emul, bool alert, void *data)
{
	if (alert) {
		stub_set_alert_status(PD_STATUS_TCPC_ALERT_0);
		schedule_deferred_pd_interrupt(USBC_PORT_C0);
	} else {
		stub_clear_alert_status(PD_STATUS_TCPC_ALERT_0);
	}
}

struct charger_emul_data charger_emul;

/** Test TCPCI init and vbus level */
static void test_emul_charger(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));

	tcpci_emul_set_alert_callback(emul, alert_state_func, NULL);

	/* Init and connect charger emulator */
	charger_emul_init(&charger_emul);
	charger_emul_connect_to_tcpci(&charger_emul, emul);

	/* Wait for PD negotiation to end */
	k_msleep(8000);

	/* Disable alert interrupt */
	tcpci_emul_set_alert_callback(emul, NULL, NULL);

	/* Test that SNK ready is achived */
	zassert_equal(PE_SNK_READY, get_state_pe(USBC_PORT_C0), NULL);
}

void test_suite_emul_charger(void)
{
	ztest_test_suite(emul_charger,
			 ztest_unit_test(test_emul_charger));
	ztest_run_test_suite(emul_charger);
}
