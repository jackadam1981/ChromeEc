/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "drivers/pdc.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_realtek_rts5453p.h"
#include "i2c.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_mocks.h"
#include "test/drivers/test_state.h"
#include "timer.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define RTS5453P_NODE DT_NODELABEL(rts5453p_emul)
#define EMUL EMUL_DT_GET(RTS5453P_NODE)
#define EMUL_DATA rts5453p_emul_get_i2c_common_data(EMUL)

LOG_MODULE_REGISTER(test_pdc_api, LOG_LEVEL_INF);

ZTEST_SUITE(pdc_api, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST_USER(pdc_api, test_reset)
{
	/* TODO
	 * struct pdc_info_t *test = EMUL_DATA;
	 * Basic initialization works.
	 * zassert_ok(pdc_reset(PDC_DEV), "Failed to reset PDC");
	 */
	zassert_true(true);
}
