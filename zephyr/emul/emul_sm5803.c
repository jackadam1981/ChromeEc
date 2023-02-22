/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "driver/charger/sm5803.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_sm5803.h"
#include "emul/emul_stub_device.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#define DT_DRV_COMPAT cros_sm5803_emul

LOG_MODULE_REGISTER(sm5803_emul, CONFIG_SM5803_EMUL_LOG_LEVEL);

struct sm5803_emul_data {
};

struct sm5803_emul_cfg {
	const struct emul *chg_page;
};

static int sm5803_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	// TODO don't seem to need this function at all?
	return 0;
}

#define INIT_SM5803(n)                                                  \
	static struct sm5803_emul_data sm5803_emul_data_##n = {         \
	};                                                              \
	static struct sm5803_emul_cfg sm5803_emul_cfg_##n = {                  \
                .chg_page = EMUL_DT_GET(DT_INST_PHANDLE(n, chg_page)), \
	};                                                              \
	DEVICE_DT_INST_DEFINE(n, sm5803_emul_init, NULL, &sm5803_emul_data_##n, \
			    &sm5803_emul_cfg_##n, POST_KERNEL, 10, NULL);

DT_INST_FOREACH_STATUS_OKAY(INIT_SM5803)

static void sm5803_emul_reset(const struct emul *emul)
{
	const struct sm5803_emul_cfg *cfg = emul->cfg;
	sm5803_chg_emul_reset(cfg->chg_page);
}

static void sm5803_emul_reset_before(const struct ztest_unit_test *test,
				     void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

#define SM5803_EMUL_RESET_RULE_AFTER(n) \
	sm5803_emul_reset(EMUL_DT_GET(DT_DRV_INST(n)));

	DT_INST_FOREACH_STATUS_OKAY(SM5803_EMUL_RESET_RULE_AFTER);
}
ZTEST_RULE(sm5803_emul_reset, sm5803_emul_reset_before, NULL);
