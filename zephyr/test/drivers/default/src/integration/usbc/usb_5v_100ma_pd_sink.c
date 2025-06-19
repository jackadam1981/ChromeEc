/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_pd.h"

#include <zephyr/ztest.h>

/* Chromebooks only charge PD partners at 5v */
#define TEST_SRC_PORT_VBUS_MV 5000
#define TEST_SRC_PORT_TARGET_MA 3000
#define TEST_SNK_PORT_INITIAL_MA 100

#define TEST_SINK_CAP_5V_100MA \
	PDO_FIXED(TEST_SRC_PORT_VBUS_MV, TEST_SNK_PORT_INITIAL_MA, 0)

#define TEST_SINK_CAP_5V_3000MA \
	PDO_FIXED(TEST_SRC_PORT_VBUS_MV, TEST_SRC_PORT_TARGET_MA, 0)

struct usb_attach_5v_100ma_pd_sink_fixture {
	struct tcpci_partner_data sink_5v_100ma;
	struct tcpci_snk_emul_data snk_ext;
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
};

static void *usb_attach_5v_100ma_pd_sink_setup(void)
{
	static struct usb_attach_5v_100ma_pd_sink_fixture test_fixture;

	/* Get references for the emulators */
	test_fixture.tcpci_emul = EMUL_GET_USBC_BINDING(0, tcpc);
	test_fixture.charger_emul = EMUL_GET_USBC_BINDING(0, chg);

	return &test_fixture;
}

static void usb_attach_5v_100ma_pd_sink_before(void *data)
{
	struct usb_attach_5v_100ma_pd_sink_fixture *test_fixture = data;

	/* Set chipset to ON, this will set TCPM to DRP */
	test_set_chipset_to_s0();

	/* TODO(b/214401892): Check why need to give time TCPM to spin */
	k_sleep(K_SECONDS(1));

	/* connect the partner emulator with sink caps 5V and 100mA */
	tcpci_partner_init(&test_fixture->sink_5v_100ma, PD_REV20);
	test_fixture->sink_5v_100ma.extensions = tcpci_snk_emul_init(
		&test_fixture->snk_ext, &test_fixture->sink_5v_100ma, NULL);
	test_fixture->snk_ext.pdo[0] = TEST_SINK_CAP_5V_100MA;
	connect_sink_to_port(&test_fixture->sink_5v_100ma,
			     test_fixture->tcpci_emul,
			     test_fixture->charger_emul);
}

static void usb_attach_5v_100ma_pd_sink_after(void *data)
{
	struct usb_attach_5v_100ma_pd_sink_fixture *test_fixture = data;

	disconnect_sink_from_port(test_fixture->tcpci_emul);
}

ZTEST_SUITE(usb_attach_5v_100ma_pd_sink, drivers_predicate_post_main,
	    usb_attach_5v_100ma_pd_sink_setup,
	    usb_attach_5v_100ma_pd_sink_before,
	    usb_attach_5v_100ma_pd_sink_after, NULL);

ZTEST_F(usb_attach_5v_100ma_pd_sink, test_sink_caps_pdos)
{
	struct ec_response_typec_status status = host_cmd_typec_status(0);

	/* check the received sink caps with the initial sink caps */
	zassert_equal(status.sink_cap_count, 1,
		      "Expected 1 sink PDOs, but got %d",
		      status.sink_cap_count);

	/* Change sink PDO to 5V/3A */
	fixture->snk_ext.pdo[0] = TEST_SINK_CAP_5V_3000MA;

	/* Send Request with Cap Mismatch */
	tcpci_snk_emul_send_request_msg(&fixture->snk_ext,
					&fixture->sink_5v_100ma, 3000, true);

	k_sleep(K_SECONDS(1));

	struct ec_response_usb_pd_power_info info = host_cmd_power_info(0);

	/* Checking the received sink caps with the new ones we set */
	zassert_within(
		info.meas.voltage_now, TEST_SRC_PORT_VBUS_MV, 500,
		"Charging voltage expected to be near 5000mV, but was %dmV",
		info.meas.voltage_now);
	zassert_equal(info.meas.current_max, 3000,
		      "Current max expected to be 3000mA, but was %dmA",
		      info.meas.current_max);
}
