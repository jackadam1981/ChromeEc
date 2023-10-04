/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <emul_intel_pd_controller.h>
#include <host_command.h>
#include <zephyr/ztest.h>

#define USBC_INTEL_ALTMODE_EMUL_ARRAY_WITH_COMMA(node_id)  EMUL_DT_GET(node_id),

/* Delay longer than driver ISR latency */
#define USBC_INTEL_ALTMODE_DELAY        (100)
#define USBC_INTEL_ALTMODE_WAIT()       do{k_msleep(USBC_INTEL_ALTMODE_DELAY); \
					}while(false)                          \

const struct emul* emul_pd_ctrlr[] = {
	DT_FOREACH_STATUS_OKAY(intel_pd_controller_emul,
			USBC_INTEL_ALTMODE_EMUL_ARRAY_WITH_COMMA)
};

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
	union data_status_reg status;

	/*Reset resgister value */
	memset(&status, 0, sizeof(status));
	for (int i = 0; i < ARRAY_SIZE(emul_pd_ctrlr); i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();
}

ZTEST_USER(usbc_intel_altmode, is_connected)
{
	union data_status_reg status;

	status.data_conn = 1;
	for (int i = 0; i < ARRAY_SIZE(emul_pd_ctrlr); i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();
	for (int i = 0; i < ARRAY_SIZE(emul_pd_ctrlr); i++) {
		zassert_true(pd_is_connected(i), "Port %d Failed", i);
	}
}

ZTEST_SUITE(usbc_intel_altmode, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
