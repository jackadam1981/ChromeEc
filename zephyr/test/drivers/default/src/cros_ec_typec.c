/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for the interactions for EC and kernel cros_ec_typec module
 */

#include <zephyr/ztest.h>

#include "emul/emul_isl923x.h"
#include "emul/tcpc/emul_tcpci_partner_drp.h"
#include "hooks.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_pd.h"

#define TEST_PORT 0

struct cros_ec_typec_fixture {
	struct tcpci_partner_data source_5v_3a;
	struct tcpci_src_emul_data src_ext;
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
};

static void *cros_ec_typec_setup(void)
{
	static struct cros_ec_typec_fixture fixture;

	/* Initialized the source to supply 5V and 3A */
	tcpci_partner_init(&fixture.source_5v_3a, PD_REV20);
	fixture.source_5v_3a.extensions = tcpci_src_emul_init(
		&fixture.src_ext, &fixture.source_5v_3a, NULL);
	fixture.src_ext.pdo[1] = PDO_FIXED(5000, 3000, PDO_FIXED_UNCONSTRAINED);

	fixture.tcpci_emul = EMUL_GET_USBC_BINDING(TEST_PORT, tcpc);
	fixture.charger_emul = EMUL_GET_USBC_BINDING(TEST_PORT, chg);

	return &fixture;
}

static void cros_ec_typec_before(void *data)
{
	struct cros_ec_typec_fixture *fixture = data;

	zassume_true(board_get_usb_pd_port_count() > TEST_PORT &&
			     TEST_PORT >= 0,
		     "TEST_PORT is out-of-range.");

	connect_source_to_port(&fixture->source_5v_3a, &fixture->src_ext, 1,
			       fixture->tcpci_emul, fixture->charger_emul);
	test_set_chipset_to_s0();
}

static void cros_ec_typec_after(void *data)
{
	struct cros_ec_typec_fixture *fixture = data;

	pd_clear_events(TEST_PORT, GENMASK(31, 0));
	disconnect_source_from_port(fixture->tcpci_emul, fixture->charger_emul);
	test_set_chipset_to_s0();
}

ZTEST_F(cros_ec_typec, verify_hard_reset_event_cleared_at_startup)
{
	struct ec_response_typec_status status;

	test_set_chipset_to_g3();

	k_sleep(K_SECONDS(1));
	zassert_true(chipset_in_state(CHIPSET_STATE_ANY_OFF),
		     "Chipset is not off.");

	status = host_cmd_typec_status(TEST_PORT);
	zassert_false(status.events & PD_STATUS_EVENT_HARD_RESET, NULL);

	/* issue hard reset and ensure the event is recorded */
	tcpci_partner_common_send_hard_reset(&fixture->source_5v_3a);
	k_sleep(K_SECONDS(1));
	status = host_cmd_typec_status(TEST_PORT);
	zassert_true(status.events & PD_STATUS_EVENT_HARD_RESET, NULL);

	/*
	 * chipset starts up, the past irrelevant hard reset events should be
	 * cleared
	 */
	hook_notify(HOOK_CHIPSET_STARTUP);
	k_sleep(K_SECONDS(1));
	status = host_cmd_typec_status(TEST_PORT);
	zassert_false(status.events & PD_STATUS_EVENT_HARD_RESET,
		      "PD_STATUS_EVENT_HARD_RESET should be cleared when "
		      "chipset starting up.");
}

ZTEST_SUITE(cros_ec_typec, drivers_predicate_post_main, cros_ec_typec_setup,
	    cros_ec_typec_before, cros_ec_typec_after, NULL);
