/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "drivers/imvp/rt3645.h"
#include "emul/emul_rt3645.h"
#include "test/drivers/test_state.h"

#include <zephyr/fff.h>
#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#define RT3645_PORT 0
#define RT3645_NODE DT_NODELABEL(rt3645_emul)

extern int rt3645_get_flag(int port);

const struct emul *emul = EMUL_DT_GET(RT3645_NODE);
const struct device *dev = DEVICE_DT_GET(RT3645_NODE);

void rt3645_imvp_set_in_config_mode(void)
{
	rt3645_write_reg(dev, CONFIG_MODE_REG, 0x24);
	rt3645_write_reg(dev, CONFIG_MODE_REG, 0x25);
	rt3645_write_reg(dev, CONFIG_MODE_REG, 0x26);
	rt3645_write_reg(dev, CONFIG_MODE_REG, 0x27);
}

ZTEST(rt3645_imvp, test_product_id)
{
	uint8_t reg_val;

	rt3645_emul_read_reg(emul, PRODUCT_ID_REG, &reg_val);
	zassert_equal(reg_val, 0x45);
}

ZTEST(rt3645_imvp, test_config_mode)
{
	zassert_false(rt3645_emul_in_config_mode(emul));
	rt3645_write_reg(dev, CONFIG_MODE_REG, 0x24);
	rt3645_write_reg(dev, CONFIG_MODE_REG, 0x26);
	rt3645_write_reg(dev, CONFIG_MODE_REG, 0x27);
	zassert_false(rt3645_emul_in_config_mode(emul));

	rt3645_set_page(dev, 9);

	rt3645_imvp_set_in_config_mode();
	zassert_true(rt3645_emul_in_config_mode(emul));
}

ZTEST(rt3645_imvp, test_page_setting)
{
	uint8_t reg_val;

	rt3645_imvp_set_in_config_mode();
	zassert_true(rt3645_emul_in_config_mode(emul));
	rt3645_emul_read_reg(emul, PAGE_SET_REG, &reg_val);
	zassert_equal(reg_val, 0);
	rt3645_set_page(dev, 9);
	rt3645_emul_read_reg(emul, PAGE_SET_REG, &reg_val);
	zassert_equal(reg_val, 9);
}

ZTEST(rt3645_imvp, test_page_regs_store)
{
	zassert_ok(shell_execute_cmd(get_ec_shell(), "imvp"), NULL);
}

void rt3645_imvp_test_reset(void *fixture)
{
	rt3645_emul_reset_regs(emul);
}

ZTEST_SUITE(rt3645_imvp, drivers_predicate_pre_main, NULL,
	    rt3645_imvp_test_reset, NULL, NULL);
