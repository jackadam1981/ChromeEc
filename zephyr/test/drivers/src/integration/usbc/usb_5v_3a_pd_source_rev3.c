/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ztest.h>

#include "battery.h"
#include "battery_smart.h"
#include "emul/emul_isl923x.h"
#include "emul/emul_smart_battery.h"
#include "emul/tcpc/emul_tcpci_partner_src.h"
#include "hooks.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_pd.h"
#include "usb_prl_sm.h"
#include "util.h"

#define BATTERY_ORD DT_DEP_ORD(DT_NODELABEL(battery))

#define TEST_USB_PORT USBC_PORT_C0

struct usb_attach_5v_3a_pd_source_rev3_fixture {
	struct tcpci_partner_data source_5v_3a;
	struct tcpci_src_emul_data src_ext;
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
};

static void *usb_attach_5v_3a_pd_source_setup(void)
{
	static struct usb_attach_5v_3a_pd_source_rev3_fixture test_fixture;

	/* Get references for the emulators */
	test_fixture.tcpci_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(tcpci_emul)));
	test_fixture.charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	/* Configure TCPCI revision in board config and emulator */
	tcpc_config[0].flags |= TCPC_FLAGS_TCPCI_REV2_0;
	tcpci_emul_set_rev(test_fixture.tcpci_emul, TCPCI_EMUL_REV2_0_VER1_1);

	/* Initialized the charger to supply 5V and 3A */
	tcpci_partner_init(&test_fixture.source_5v_3a, PD_REV30);
	test_fixture.source_5v_3a.extensions = tcpci_src_emul_init(
		&test_fixture.src_ext, &test_fixture.source_5v_3a, NULL);
	test_fixture.src_ext.pdo[1] =
		PDO_FIXED(5000, 3000, PDO_FIXED_UNCONSTRAINED);

	return &test_fixture;
}

static void usb_attach_5v_3a_pd_source_before(void *data)
{
	struct usb_attach_5v_3a_pd_source_rev3_fixture *fixture = data;

	connect_source_to_port(&fixture->source_5v_3a, &fixture->src_ext, 1,
			       fixture->tcpci_emul, fixture->charger_emul);
}

static void usb_attach_5v_3a_pd_source_after(void *data)
{
	struct usb_attach_5v_3a_pd_source_rev3_fixture *fixture = data;

	disconnect_source_from_port(fixture->tcpci_emul, fixture->charger_emul);
}

ZTEST_SUITE(usb_attach_5v_3a_pd_source_rev3, drivers_predicate_post_main,
	    usb_attach_5v_3a_pd_source_setup, usb_attach_5v_3a_pd_source_before,
	    usb_attach_5v_3a_pd_source_after, NULL);

ZTEST_F(usb_attach_5v_3a_pd_source_rev3, test_batt_cap)
{
	int battery_index = 0;

	tcpci_partner_common_send_get_battery_capabilities(
		&fixture->source_5v_3a, battery_index);

	/* Allow some time for TCPC to process and respond */
	k_sleep(K_SECONDS(1));

	zassert_true(fixture->source_5v_3a.battery_capabilities
			     .have_response[battery_index],
		     "No battery capabilities response stored.");

	/* The response */
	struct pd_bcdb *bcdb =
		&fixture->source_5v_3a.battery_capabilities.bcdb[battery_index];

	zassert_equal(USB_VID_GOOGLE, bcdb->vid, "Incorrect battery VID");
	zassert_equal(CONFIG_USB_PID, bcdb->pid, "Incorrect battery PID");
	zassert_false((bcdb->battery_type) & (1 << 0),
		      "Invalid battery ref bit should not be set");

	/* Verify the battery capacity and last full charge capacity. These
	 * fields require that the battery is present and that we can
	 * access information about the nominal voltage and capacity.
	 */

	int expected_design_cap;
	int expected_last_charge_cap;

	/* See pe_give_battery_cap_entry() in common/usbc/usb_pe_drp_sm.c */

	if (battery_is_present()) {
		/* Default to 'capacity unknown' */
		expected_design_cap = 0xFFFF;
		expected_last_charge_cap = 0xFFFF;

		/* If the host memmap is available, get data from there */
		if (IS_ENABLED(HAS_TASK_HOSTCMD) &&
		    *host_get_memmap(EC_MEMMAP_BATTERY_VERSION) != 0) {
			int design_volt, design_cap, full_cap;

			design_volt =
				*(int *)host_get_memmap(EC_MEMMAP_BATT_DVLT);
			design_cap =
				*(int *)host_get_memmap(EC_MEMMAP_BATT_DCAP);
			full_cap = *(int *)host_get_memmap(EC_MEMMAP_BATT_LFCC);

			expected_design_cap = DIV_ROUND_NEAREST(
				(design_cap * design_volt), 100000);

			expected_last_charge_cap = DIV_ROUND_NEAREST(
				(design_cap * full_cap), 100000);
		} else {
			/* Otherwise, use legacy API */

			int v;
			int c;

			if (battery_design_voltage(&v) == 0) {
				if (battery_design_capacity(&c) == 0) {
					expected_design_cap = DIV_ROUND_NEAREST(
						(v * c), 100000);
				}

				if (battery_full_charge_capacity(&c) == 0) {
					expected_last_charge_cap =
						DIV_ROUND_NEAREST((v * c),
								  100000);
				}
			}
		}
	} else {
		/* Fields should be 0 to indicate no battery present. */
		expected_design_cap = 0;
		expected_last_charge_cap = 0;
	}

	zassert_equal(expected_design_cap, bcdb->design_cap,
		      "Design capacity not correct. Expected %d but got %d",
		      expected_design_cap, bcdb->design_cap);
	zassert_equal(
		expected_last_charge_cap, bcdb->last_full_charge_cap,
		"Last full charge capacity not correct. Expected %d but got %d",
		expected_last_charge_cap, bcdb->last_full_charge_cap);
}

ZTEST_F(usb_attach_5v_3a_pd_source_rev3, test_batt_cap_invalid)
{
	/* Request data on a battery that does not exist. The PD stack only
	 * supports battery 0.
	 */

	int battery_index = 5;

	tcpci_partner_common_send_get_battery_capabilities(
		&fixture->source_5v_3a, battery_index);

	/* Allow some time for TCPC to process and respond */
	k_sleep(K_SECONDS(1));

	/* Ensure we get a response that says our battery index was invalid */

	zassert_true(fixture->source_5v_3a.battery_capabilities
			     .have_response[battery_index],
		     "No battery capabilities response stored.");
	zassert_true(
		(fixture->source_5v_3a.battery_capabilities.bcdb[battery_index]
			 .battery_type) &
			(1 << 0),
		"Invalid battery ref bit should be set");
}
