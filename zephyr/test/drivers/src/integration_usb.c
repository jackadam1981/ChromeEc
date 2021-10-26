/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "emul/emul_tcpci.h"
#define TCPCI_EMUL_LABEL DT_NODELABEL(tcpci_emul)
#define BATTERY_ORD	DT_DEP_ORD(DT_NODELABEL(battery))

#if 0

static void remove_emulated_devices(void)
{
	const struct emul *tcpci_emul = emul_get_binding(
		DT_LABEL(TCPCI_EMUL_LABEL));

	tcpci_emul->partner = NULL;
	/* TODO: This function should trigger gpios to signal there is nothing
	 * attached to the port.
	 */
	tcpci_emul_disconnect_partner(tcpci_emul);
}

/* Handle messages sent to charger. */
static void handle_transmit(const struct emul *emul,
			 const struct tcpci_emul_partner_ops *ops,
			 const struct tcpci_emul_msg *tx_msg,
			 enum tcpci_msg_type type,
			 int retry)
{
	/* TODO: reply to transmitted messages.
	 * I don't know if sleep is allowed here to simulate a delay before
	 * the partner device responds.
	 */
}

static void handle_control_change(const struct emul *emul,
			const struct tcpci_emul_partner_ops *ops)
{
	/* TODO: handle control changes. */
}

/* I imagine other partners here that inherit from this but only override a
 * few transmitted messages.
 */
static struct tcpci_emul_partner_ops compliant_charger
{
	.transmit = &handle_transmit,
	.control_change = &handle_control_change,
};

static void test_attach_compliant_charger(void)
{
	const struct emul *tcpci_emul = emul_get_binding(
		DT_LABEL(TCPCI_EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint16_t battery_status;

	/* Verify battery not charging. */
	i2c_emul = sbat_emul_get_ptr(BATTERY_ORD);
	battery_status = sbat_emul_read_status(i2c_emul);
	zassert_true(battery_status & STATUS_DISCHARGING,
		"Battery is not discharging: %d", battery_status);

	/* Attach emulated charger. */
	tcpci_emul->partner = &compliant_charger;
	/* TODO: This function should trigger gpios to signal there is something
	 * attached to the port.
	 */
	tcpci_emul_connect_partner(tcpci_emul);

	/* Wait for PD negotiation. */
	/* TODO: This function should wait until PD negotiation reaches a stable
	 * state. Alternatively, we could have the handle_transmit function
	 * above signal a condition or semaphore when it receives a message
	 * notifying it of the final state.
	 */
	tcpci_emul_wait_for_stable(tcpci_emul);

	/* Verify battery charging. */
	battery_status = sbat_emul_read_status(i2c_emul);
	zassert_false(battery_status & STATUS_DISCHARGING,
		"Battery is discharging: %d", battery_status);
	/* TODO: Also check voltage, current, etc. */
}

void test_suite_integration_usb(void)
{
	ztest_test_suite(integration_usb,
			 ztest_user_unit_test_setup_teardown(
				 test_attach_compliant_charger, unit_test_noop,
				remove_emulated_devices));
	ztest_run_test_suite(integration_usb);
}
#endif
