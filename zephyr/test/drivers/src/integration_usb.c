/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "emul/emul_tcpci.h"
#include "emul/emul_smart_battery.h"
#include "battery_smart.h"
#include "tcpm/tcpci.h"

#define TCPCI_EMUL_LABEL DT_NODELABEL(tcpci_emul)
#define BATTERY_ORD	DT_DEP_ORD(DT_NODELABEL(battery))

static void init_tcpm(void) {
	const struct emul *tcpci_emul = emul_get_binding(
		DT_LABEL(TCPCI_EMUL_LABEL));

	zassert_ok(tcpci_tcpm_init(0), 0);
	pd_set_suspend(0, 0);
	/* Reset to disconnected state. */
	zassert_ok(tcpci_emul_disconnect_partner(tcpci_emul), NULL);
}

static void remove_emulated_devices(void)
{
	const struct emul *tcpci_emul = emul_get_binding(
		DT_LABEL(TCPCI_EMUL_LABEL));
	/* TODO: This function should trigger gpios to signal there is nothing
	 * attached to the port.
	 */
	zassert_ok(tcpci_emul_disconnect_partner(tcpci_emul), NULL);
}

/* I imagine other partners here that inherit from this but only override a
 * few transmitted messages.
 */
struct compliant_charger_emul {
	struct tcpci_emul_partner_ops partner_ops;
	int state;
};

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
	printf("START handle_transmit type=%d retry=%d\n", type, retry);
}

static void handle_control_change(const struct emul *emul,
			const struct tcpci_emul_partner_ops *ops)
{
	struct compliant_charger_emul *my_state = (struct compliant_charger_emul *)ops;

	/* TODO: handle control changes. */
	printf("START handle_control_change state = %d\n", my_state->state);
	switch (my_state->state) {
		case 0:
		break;
	default:
		zassert_unreachable("Invalid state: %d", my_state->state);
	}
	printf("END handle_control_change state = %d\n", my_state->state);
}

static void handle_rx_consumed(const struct emul *emul,
			    const struct tcpci_emul_partner_ops *ops,
			    const struct tcpci_emul_msg *rx_msg) {
	printf("START handle_rx_consumed\n");
			    }

static void test_attach_compliant_charger(void)
{
	const struct emul *tcpci_emul = emul_get_binding(
		DT_LABEL(TCPCI_EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint16_t battery_status;
	struct compliant_charger_emul compliant_charger =
	{
		.partner_ops.transmit = &handle_transmit,
		.partner_ops.control_change = &handle_control_change,
		.partner_ops.rx_consumed = &handle_rx_consumed,
		.state = 0,
	};

	/* Verify battery not charging. */
	i2c_emul = sbat_emul_get_ptr(BATTERY_ORD);
	zassert_ok(sbat_emul_get_word_val(i2c_emul, SB_BATTERY_STATUS,
		&battery_status), NULL);
	zassert_not_equal(battery_status & STATUS_DISCHARGING, 0,
		"Battery is not discharging: %d", battery_status);

	/* TODO? Send host command to verify PD_ROLE_DISCONNECTED. */ 

	/* Attach emulated charger. */
	tcpci_emul_set_partner_ops(
		tcpci_emul,
		(struct tcpci_emul_partner_ops *)&compliant_charger);
	tcpci_emul_connect_partner(tcpci_emul, /*src=*/true, /*cc1=*/true,
				   TYPEC_CC_VOLT_RD, true);

	k_sleep(K_SECONDS(5));
#if 0
	/* Wait for PD negotiation. */
	/* TODO: This function should wait until PD negotiation reaches a stable
	 * state. Alternatively, we could have the handle_transmit function
	 * above signal a condition or semaphore when it receives a message
	 * notifying it of the final state.
	 */
	tcpci_emul_wait_for_stable(tcpci_emul);

	/* Verify battery charging. */
	battery_status = sbat_emul_read_status(i2c_emul);
	zassert_equal(battery_status & STATUS_DISCHARGING, 0,
		"Battery is discharging: %d", battery_status);
	/* TODO: Also check voltage, current, etc. */
#endif
}

void test_suite_integration_usb(void)
{
	ztest_test_suite(integration_usb,
			 ztest_user_unit_test_setup_teardown(
				 test_attach_compliant_charger, init_tcpm,
				remove_emulated_devices));
	ztest_run_test_suite(integration_usb);
}
