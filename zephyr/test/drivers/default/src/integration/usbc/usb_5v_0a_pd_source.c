/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/tcpc/emul_tcpci_partner_src.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/shell/shell_dummy.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/slist.h>
#include <zephyr/ztest.h>

#define TEST_PORT 0

struct usb_attach_5v_0a_pd_source_fixture {
	struct tcpci_partner_data source_5v_0a;
	struct tcpci_src_emul_data src_ext;
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
};

static void *usb_attach_5v_0a_pd_source_setup(void)
{
	static struct usb_attach_5v_0a_pd_source_fixture test_fixture;

	/* Get references for the emulators */
	test_fixture.tcpci_emul = EMUL_GET_USBC_BINDING(TEST_PORT, tcpc);
	test_fixture.charger_emul = EMUL_GET_USBC_BINDING(TEST_PORT, chg);

	/* Initialized the charger to supply 5V and 0A */
	tcpci_partner_init(&test_fixture.source_5v_0a, PD_REV20);
	test_fixture.source_5v_0a.extensions = tcpci_src_emul_init(
		&test_fixture.src_ext, &test_fixture.source_5v_0a, NULL);
	test_fixture.src_ext.pdo[0] = PDO_FIXED(5000, 0, 0);

	return &test_fixture;
}

static void usb_attach_5v_0a_pd_source_before(void *data)
{
	struct usb_attach_5v_0a_pd_source_fixture *fixture = data;

	connect_source_to_port(&fixture->source_5v_0a, &fixture->src_ext, 1,
			       fixture->tcpci_emul, fixture->charger_emul);
}

static void usb_attach_5v_0a_pd_source_after(void *data)
{
	struct usb_attach_5v_0a_pd_source_fixture *fixture = data;

	disconnect_source_from_port(fixture->tcpci_emul, fixture->charger_emul);
}

ZTEST_SUITE(usb_attach_5v_0a_pd_source, drivers_predicate_post_main,
	    usb_attach_5v_0a_pd_source_setup, usb_attach_5v_0a_pd_source_before,
	    usb_attach_5v_0a_pd_source_after, NULL);

ZTEST(usb_attach_5v_0a_pd_source, test_typec_status)
{
	struct ec_response_typec_status status = host_cmd_typec_status(0);

	zassert_true(status.pd_enabled, "PD is disabled");
	zassert_true(status.dev_connected, "Device disconnected");
	zassert_true(status.sop_connected, "Charger is not SOP capable");
	zassert_equal(status.source_cap_count, 1,
		      "Expected 1 source PDO, but got %d",
		      status.source_cap_count);
	zassert_equal(status.power_role, PD_ROLE_SINK,
		      "Expected power role to be %d, but got %d", PD_ROLE_SINK,
		      status.power_role);
}

ZTEST(usb_attach_5v_0a_pd_source, test_power_info)
{
	struct ec_response_usb_pd_power_info info = host_cmd_power_info(0);

	zassert_equal(info.role, USB_PD_PORT_POWER_DISCONNECTED,
		      "Expected role to be %d, but got %d",
		      USB_PD_PORT_POWER_DISCONNECTED, info.role);
	zassert_equal(info.type, USB_CHG_TYPE_NONE,
		      "Expected type to be %d, but got %d", USB_CHG_TYPE_NONE,
		      info.type);
	zassert_equal(info.max_power, 0,
		      "Expected the maximum power to be 0uW, but got %duW",
		      info.max_power);
	zassert_equal(info.meas.voltage_max, 0,
		      "Expected charge voltage max of 0mV, but got %dmV",
		      info.meas.voltage_max);
	zassert_within(info.meas.voltage_now, 5, 5,
		       "Charging voltage expected to be near 0mV, but got %dmV",
		       info.meas.voltage_now);
	zassert_equal(info.meas.current_max, 0,
		      "Current max expected to be 0mA, but was %dmA",
		      info.meas.current_max);
	zassert_true(info.meas.current_lim >= 0,
		     "Expected the PD current limit to be >= 0, but got %dmA",
		     info.meas.current_lim);
}

ZTEST_F(usb_attach_5v_0a_pd_source, test_disconnect_typec_status)
{
	struct ec_response_typec_status typec_status;

	disconnect_source_from_port(fixture->tcpci_emul, fixture->charger_emul);
	typec_status = host_cmd_typec_status(0);

	zassert_false(typec_status.pd_enabled);
	zassert_false(typec_status.dev_connected);
	zassert_false(typec_status.sop_connected);
	zassert_equal(typec_status.source_cap_count, 0,
		      "Expected 0 source caps, but got %d",
		      typec_status.source_cap_count);
	zassert_equal(typec_status.power_role, USB_CHG_TYPE_NONE,
		      "Expected power role to be %d, but got %d",
		      USB_CHG_TYPE_NONE, typec_status.power_role);
}

ZTEST_F(usb_attach_5v_0a_pd_source, test_disconnect_power_info)
{
	struct ec_response_usb_pd_power_info power_info;

	disconnect_source_from_port(fixture->tcpci_emul, fixture->charger_emul);
	power_info = host_cmd_power_info(0);

	zassert_equal(power_info.role, USB_PD_PORT_POWER_DISCONNECTED,
		      "Expected power role to be %d, but got %d",
		      USB_PD_PORT_POWER_DISCONNECTED, power_info.role);
	zassert_equal(power_info.type, USB_CHG_TYPE_NONE,
		      "Expected charger type to be %d, but got %s",
		      USB_CHG_TYPE_NONE, power_info.type);
	zassert_equal(power_info.max_power, 0,
		      "Expected the maximum power to be 0uW, but got %duW",
		      power_info.max_power);
	zassert_equal(power_info.meas.voltage_max, 0,
		      "Expected maximum voltage of 0mV, but got %dmV",
		      power_info.meas.voltage_max);
	zassert_within(power_info.meas.voltage_now, 5, 5,
		       "Expected present voltage near 0mV, but got %dmV",
		       power_info.meas.voltage_now);
	zassert_equal(power_info.meas.current_max, 0,
		      "Expected maximum current of 0mA, but got %dmA",
		      power_info.meas.current_max);
	zassert_true(power_info.meas.current_lim >= 0,
		     "Expected the PD current limit to be >= 0, but got %dmA",
		     power_info.meas.current_lim);
}
