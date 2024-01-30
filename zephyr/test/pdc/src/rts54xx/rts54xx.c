/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "common.h"
#include "console.h"
#include "drivers/pdc.h"
#include "drivers/ucsi_v3.h"
#include "emul/emul_pdc.h"
#include "emul/emul_realtek_rts54xx.h"
#include "i2c.h"
#include "zephyr/sys/util.h"
#include "zephyr/sys/util_macro.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(test_rts54xx, LOG_LEVEL_INF);

#define RTS5453P_NODE DT_NODELABEL(rts5453p_emul)

static const struct emul *emul = EMUL_DT_GET(RTS5453P_NODE);
static const struct device *dev = DEVICE_DT_GET(RTS5453P_NODE);

static const uint32_t epr_pdos4[] = {

};
static const uint32_t epr_pdos5[] = {

};

static void rts54xx_before_test(void *data)
{
	emul_pdc_set_response_delay(emul, 0);
}

ZTEST_SUITE(rts54xx, NULL, NULL, rts54xx_before_test, NULL, NULL);

ZTEST_USER(rts54xx, test_emul_reset)
{
	uint32_t pdos[PDO_OFFSET_END];

	/* Test source PDO reset values. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(pdc_get_pdos(dev, SOURCE_PDO, PDO_OFFSET_0, 8, false, &pdos[0]));

	k_sleep(K_MSEC(100));
	zassert_equal(pdos[0], RTS5453P_FIXED_SRC);

	for (int i = 0; i < 7; i++) {
		zassert_equal(pdos[i + 1], 0xFFFFFFFF);
	}

	/* Test sink PDO reset values. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(pdc_get_pdos(dev, SINK_PDO, PDO_OFFSET_0, 8, false, &pdos[0]));

	k_sleep(K_MSEC(100));
	zassert_equal(pdos[0], RTS5453P_FIXED_SNK);

	for (int i = 0; i < 7; i++) {
		zassert_equal(pdos[i + 1], 0xFFFFFFFF);
	}
}

ZTEST_USER(rts54xx, test_emul_set_pdos)
{
	uint32_t pdos[PDO_OFFSET_END];

	/* Validate that offset zero is invalid. */
	zassert_not_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1, false, &pdos[0]));
	zassert_not_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_0, 1, false, &pdos[0]));

	/* Validate that only PDOs 1-4 support EPR. */
	/* Validate source PDOs. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, false, epr_pdos4));
	zassert_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, false, pdos));
	for (int i = 0; i < ARRAY_SIZE(pdos); i++) {
		LOG_INF("RPZ a %x", pdos[i]);
	}
	zassert_ok(memcmp(pdos, epr_pdos4, sizeof(epr_pdos4)));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET + 1, false, epr_pdos5));
	zassert_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET + 1, false, pdos));
	for (int i = 0; i < ARRAY_SIZE(pdos); i++) {
		LOG_INF("RPZ a %x", pdos[i]);
	}
	zassert_not_ok(memcmp(pdos, epr_pdos5, sizeof(epr_pdos5)));

	/* Validate sink PDOs. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, false, epr_pdos4));
	zassert_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, false, pdos));
	zassert_ok(memcmp(pdos, epr_pdos4, sizeof(epr_pdos4)));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET + 1, false, epr_pdos5));
	zassert_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET + 1, false, pdos));
	zassert_not_ok(memcmp(pdos, epr_pdos5, sizeof(epr_pdos5)));

}