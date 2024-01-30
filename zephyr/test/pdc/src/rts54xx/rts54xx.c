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
#include "emul/emul_realtek_rts54xx_public.h"
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

static const uint32_t epr_pdos[] = {
	PDO_AUG_EPR(5000, 20000, 140, 0),
	PDO_AUG_EPR(5000, 20000, 140, 0),
	PDO_AUG_EPR(5000, 20000, 140, 0),
	PDO_AUG_EPR(5000, 20000, 140, 0),
	PDO_AUG_EPR(5000, 20000, 140, 0),
};
static const uint32_t spr_pdos[] = {
	PDO_AUG(1000, 5000, 3000),
	PDO_FIXED(5000, 3000, 0),
	PDO_AUG(1000, 5000, 3000),
	PDO_FIXED(9000, 3000, 0),
	PDO_AUG(1000, 5000, 3000),
	PDO_FIXED(15000, 3000, 0),
	PDO_AUG(1000, 5000, 3000),
	PDO_FIXED(20000, 3000, 0),
};
static const uint32_t mixed_pdos_success[] = {
	PDO_AUG_EPR(5000, 20000, 140, 0),
	PDO_FIXED(5000, 3000, PDO_FIXED_EPR_MODE_CAPABLE),
	PDO_AUG(1000, 5000, 3000),
	PDO_FIXED(5000, 3000, 0),
	PDO_FIXED(9000, 3000, 0),
	PDO_FIXED(20000, 3000, 0),
};
static const uint32_t mixed_pdos_failure[] = {
	PDO_AUG(1000, 5000, 3000),
	PDO_FIXED(5000, 3000, 0),
	PDO_FIXED(9000, 3000, 0),
	PDO_FIXED(20000, 3000, 0),
	PDO_FIXED(5000, 3000, PDO_FIXED_EPR_MODE_CAPABLE),
	PDO_AUG_EPR(5000, 20000, 140, 0),
};

static const struct emul *emul = EMUL_DT_GET(RTS5453P_NODE);
static const struct device *dev = DEVICE_DT_GET(RTS5453P_NODE);

static void rts54xx_before_test(void *data)
{
	emul_pdc_reset(emul);
	emul_pdc_set_response_delay(emul, 0);
}

ZTEST_SUITE(rts54xx, NULL, NULL, rts54xx_before_test, NULL, NULL);

ZTEST_USER(rts54xx, test_emul_reset)
{
	uint32_t pdos[PDO_OFFSET_END];

	/* Test source PDO reset values. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 8, false, pdos));
	zassert_equal(pdos[0], RTS5453P_FIXED_SRC);

	for (int i = 0; i < 7; i++) {
		zassert_equal(pdos[i + 1], 0xFFFFFFFF);
	}

	/* Test sink PDO reset values. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_0, 8, false, pdos));
	zassert_equal(pdos[0], RTS5453P_FIXED_SNK);

	for (int i = 0; i < 7; i++) {
		zassert_equal(pdos[i + 1], 0xFFFFFFFF);
	}
}

ZTEST_USER(rts54xx, test_emul_pdos)
{
	uint32_t pdos[PDO_OFFSET_END];

	/* Port partner PDOs aren't currently supported. */
	/* TODO b/317065172: Update when port partner functionality is in. */
	zassert_not_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1, true, pdos));
	zassert_not_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_0, 1, true, pdos));

	/* Test that offset zero is invalid for setting. */
	zassert_not_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_0, 1, pdos));
	zassert_not_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_0, 1, pdos));

	/* Test PDO overflow. */
	zassert_not_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 8, spr_pdos));
	zassert_not_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, 8, spr_pdos));

	zassert_not_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_5, 8, false, pdos));
	zassert_not_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_5, 8, false, pdos));

	/* Test that only PDOs 1-4 support EPR. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, epr_pdos));
	zassert_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, false, pdos));
	zassert_ok(memcmp(pdos, epr_pdos, sizeof(uint32_t) * 4));
	zassert_not_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET + 1, epr_pdos));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, epr_pdos));
	zassert_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET, false, pdos));
	zassert_ok(memcmp(pdos, epr_pdos, sizeof(uint32_t) * 4));
	zassert_not_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, RTS5453P_MAX_EPR_PDO_OFFSET + 1, epr_pdos));

	/* Test that SPR PDOs can be placed in any offset. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 7, spr_pdos));
	zassert_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 7, false, pdos));
	zassert_ok(memcmp(pdos, spr_pdos, sizeof(uint32_t) * 7));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, 7, spr_pdos));
	zassert_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_1, 7, false, pdos));
	zassert_ok(memcmp(pdos, spr_pdos, sizeof(uint32_t) * 7));

	/* Test mixtures of PDOS. */
	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 6, mixed_pdos_success));
	zassert_ok(emul_pdc_get_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 6, false, pdos));
	zassert_ok(memcmp(pdos, mixed_pdos_success, sizeof(mixed_pdos_success)));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, 6, mixed_pdos_success));
	zassert_ok(emul_pdc_get_pdos(emul, SINK_PDO, PDO_OFFSET_1, 6, false, pdos));
	zassert_ok(memcmp(pdos, mixed_pdos_success, sizeof(mixed_pdos_success)));

	zassert_not_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 6, mixed_pdos_failure));
	zassert_not_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, 6, mixed_pdos_failure));
}

ZTEST_USER(rts54xx, test_pdos)
{
	uint32_t pdos[PDO_OFFSET_END];

	zassume_ok(emul_pdc_set_pdos(emul, SOURCE_PDO, PDO_OFFSET_1, 6, mixed_pdos_success));
	zassume_ok(emul_pdc_set_pdos(emul, SINK_PDO, PDO_OFFSET_1, 6, mixed_pdos_success));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(pdc_get_pdos(dev, SOURCE_PDO, PDO_OFFSET_1, 6, false, pdos));
	k_sleep(K_MSEC(100));
	zassert_ok(memcmp(pdos, mixed_pdos_success, sizeof(mixed_pdos_success)));

	memset(pdos, 0, sizeof(pdos));
	zassert_ok(pdc_get_pdos(dev, SINK_PDO, PDO_OFFSET_1, 6, false, pdos));
	k_sleep(K_MSEC(100));
	zassert_ok(memcmp(pdos, mixed_pdos_success, sizeof(mixed_pdos_success)));

	/* Test overflow. */
	zassert_ok(pdc_get_pdos(dev, SOURCE_PDO, PDO_OFFSET_5, 6, false, pdos));
	k_sleep(K_MSEC(100));
	zassert_not_ok(pdc_get_error_status(dev, NULL));

	zassert_ok(pdc_get_pdos(dev, SINK_PDO, PDO_OFFSET_5, 6, false, pdos));
	k_sleep(K_MSEC(100));
	zassert_not_ok(pdc_get_error_status(dev, NULL));
}