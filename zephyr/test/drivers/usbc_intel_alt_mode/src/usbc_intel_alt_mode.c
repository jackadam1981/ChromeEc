/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_mux.h"

#include <zephyr/ztest.h>

#include <emul_intel_pd_controller.h>
#include <host_command.h>

#define DT_DRV_COMPAT  intel_pd_altmode

/* Delay longer than driver ISR latency */
#define USBC_INTEL_ALTMODE_DELAY (500)  /* 250 ms */
#define USBC_INTEL_ALTMODE_WAIT() k_msleep(USBC_INTEL_ALTMODE_DELAY)

#define USBC_INTEL_ALTMODE_EMUL_ARRAY_WITH_COMMA(node_id) EMUL_DT_GET(node_id),

#define CHECK_MUX_FLAGS(port, flags)  ((usb_mux_get(port) & (flags)) == (flags))

const struct emul *emul_pd_ctrlr[] = { DT_FOREACH_STATUS_OKAY(
	DT_DRV_COMPAT, USBC_INTEL_ALTMODE_EMUL_ARRAY_WITH_COMMA) };

#define USBC_INTEL_ALTMODE_PORT_ENUM_WITH_COMMA(n)                            \
	USBC_PORT##n,

enum usbc_port {
	DT_INST_FOREACH_STATUS_OKAY(USBC_INTEL_ALTMODE_PORT_ENUM_WITH_COMMA)
	USBC_PORT_COUNT,
};

BUILD_ASSERT(USBC_PORT_COUNT == CONFIG_USB_PD_PORT_MAX_COUNT);

/*
 * These functions are required to satisfy build since
 * CONFIG_PLATFORM_EC_CHARGE_MANAGER is not set.
 */
__override uint8_t board_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}

__override int charge_get_display_charge(void)
{
	return 0;
}

int charge_manager_get_active_charge_port(void)
{
	return 0;
}

/* Test functions */
void usbc_intel_altmode_before(void *fixture)
{
	union data_status_reg status = { 0 };

	/*Reset resgister value */
	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();
}

ZTEST_USER(usbc_intel_altmode, test_conn)
{
	union data_status_reg status;

	status.data_conn = 1;
	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();
	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i), "Port %d Failed", i);
	}
}

ZTEST_USER(usbc_intel_altmode, test_dp)
{
	union data_status_reg status;

	status.data_conn = 1;
	status.dp = 1;
	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));
	intel_pd_controller_emul_set_status(emul_pd_ctrlr[USBC_PORT0], status);
	intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[USBC_PORT0]);
	USBC_INTEL_ALTMODE_WAIT();
	zassert_true(pd_is_connected(USBC_PORT0));
	zassert_true(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_DP_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_DP_ENABLED));

	status.dp_irq = 1;
	intel_pd_controller_emul_set_status(emul_pd_ctrlr[USBC_PORT1], status);
	intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[USBC_PORT1]);
	USBC_INTEL_ALTMODE_WAIT();
	zassert_true(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_DP_ENABLED));
	zassert_true(pd_is_connected(USBC_PORT1));
	zassert_true(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_DP_ENABLED));
}

ZTEST_USER(usbc_intel_altmode, test_mux_usb2)
{
	union data_status_reg status;

	status.data_conn = 1;
	status.usb2 = 1;

	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_USB_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i));
		zassert_true(CHECK_MUX_FLAGS(i, USB_PD_MUX_USB_ENABLED),
			"Port %d Failed", i);
	}
}

ZTEST_USER(usbc_intel_altmode, test_mux_usb3_2)
{
	union data_status_reg status;

	status.data_conn = 1;
	status.usb3_2 = 1;

	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_USB_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i));
		zassert_true(CHECK_MUX_FLAGS(i, USB_PD_MUX_USB_ENABLED),
			"Port %d Failed", i);
	}
}

ZTEST_USER(usbc_intel_altmode, test_mux_hpd_lvl)
{
	union data_status_reg status;

	status.data_conn = 1;
	status.hpd_lvl = 1;

	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_HPD_LVL));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_HPD_LVL));

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i));
		zassert_true(CHECK_MUX_FLAGS(i, USB_PD_MUX_HPD_LVL),
			"Port %d Failed", i);
	}
}

ZTEST_SUITE(usbc_intel_altmode, drivers_predicate_post_main, NULL,
	    usbc_intel_altmode_before, NULL, NULL);
