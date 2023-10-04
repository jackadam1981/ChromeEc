/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "ec_tasks.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <emul_intel_pd_controller.h>
#include <stdint.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#include <gpio.h>

#define EMUL_INTEL_ALTMODE_ARRAY_WITH_COMMA(node_id)       EMUL_DT_GET(node_id),

const struct emul* emul_pd_ctrlr[] = {
	DT_FOREACH_STATUS_OKAY(intel_pd_controller_emul, EMUL_INTEL_ALTMODE_ARRAY_WITH_COMMA)
};

__override uint8_t board_get_usb_pd_port_count(void)
{
	return 2;
}

ZTEST_USER(usbc_intel_altmode, test0)
{
	union data_status_reg status;

	k_msleep(200);
	status.data_conn = 1;
	for (int i = 0; i < ARRAY_SIZE(emul_pd_ctrlr); i++) {
		intel_pd_controller_emul_set_status(emul_pd_ctrlr[i], status);
		intel_pd_controller_emul_trigger_irq(emul_pd_ctrlr[i]);
	}
	k_msleep(200);
}

ZTEST_SUITE(usbc_intel_altmode, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
