/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "drivers/imvp/rt3645.h"
#include "emul/emul_rt3645.h"
#include "test/drivers/test_state.h"
#include "timer.h"

#include <zephyr/fff.h>
#include <zephyr/random/random.h>
#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#define RT3645_PORT 0
#define RT3645_NODE DT_NODELABEL(rt3645_emul)

extern int rt3645_get_flag(int port);

const struct emul *emul = EMUL_DT_GET(RT3645_NODE);
const struct device *dev = DEVICE_DT_GET(RT3645_NODE);

void rt3645_imvp_set_in_config_mode(void)
{
	zassert_ok(shell_execute_cmd(get_ec_shell(),
				     "imvp cfg_mode 0x24 0x25 0x26 0x27"),
		   NULL);
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

	/* Setting wrong pattern length */
	zassert_ok(shell_execute_cmd(get_ec_shell(),
				     "imvp cfg_mode 0x24 0x25 0x26"),
		   NULL);
	zassert_ok(rt3645_set_page(dev, 4));
	zassert_false(rt3645_emul_in_config_mode(emul));

	/* Setting wrong pattern */
	zassert_ok(shell_execute_cmd(get_ec_shell(),
				     "imvp cfg_mode 0x24 0x25 0x26 0x30"),
		   NULL);
	zassert_false(rt3645_emul_in_config_mode(emul));

	/* Set register to reset internal device counter */
	zassert_ok(rt3645_set_page(dev, 4));

	/* Setting correct pattern */
	rt3645_imvp_set_in_config_mode();
	zassert_true(rt3645_emul_in_config_mode(emul));
}

/**
 * @brief Write registers into RT3645 page
 *
 * @param  page Number of page to write registers
 * @param  regs Array holding registers value to be written into device
 * @param  count Number of bytes to be written
 *
 * @retval 0 if successful, otherwise if it fails
 */
int rt3645_imvp_test_fill_page_regs(uint8_t page, uint8_t *regs, uint8_t count)
{
	int ret;
	char set_page_cmd_in[32] = { 0 };
	char set_regs_cmd_in[128] = { 0 };
	char reg_val_str_in[6] = { 0 };
	const char *set_reg_cmd = "imvp set_regs 0";
	const char *set_page_cmd = "imvp set_page 0x%X";
	const char *set_reg_cat = " 0x%X";

	sprintf(set_page_cmd_in, set_page_cmd, page);
	ret = shell_execute_cmd(get_ec_shell(), set_page_cmd_in);
	if (ret) {
		return ret;
	}

	if (regs == NULL || count == 0) {
		/* Not required to write any registers, just page */
		return 0;
	}

	strcpy(set_regs_cmd_in, set_reg_cmd);
	for (int i = 0; i < count; i++) {
		reg_val_str_in[0] = '\0';
		sprintf(reg_val_str_in, set_reg_cat, regs[i]);
		strcat(set_regs_cmd_in, reg_val_str_in);
	}

	return shell_execute_cmd(get_ec_shell(), set_regs_cmd_in);
}

ZTEST(rt3645_imvp, test_page_setting)
{
	uint8_t page_in;
	uint8_t page_out;

	rt3645_imvp_set_in_config_mode();
	zassert_true(rt3645_emul_in_config_mode(emul));

	rt3645_emul_read_reg(emul, PAGE_SET_REG, &page_out);
	zassert_equal(page_out, 0);

	/* Set random page */
	page_in = (uint8_t)get_time().val % 12;
	zassert_ok(rt3645_imvp_test_fill_page_regs(page_in, NULL, 0));

	rt3645_emul_read_reg(emul, PAGE_SET_REG, &page_out);
	zassert_equal(page_in, page_out);
}

#define RT3654_IMVP_TEST_SET_REGS_COUNT 5

ZTEST(rt3645_imvp, test_setting_page_regs)
{
	uint8_t page_in;
	uint8_t regs_in[RT3654_IMVP_TEST_SET_REGS_COUNT];

	rt3645_imvp_set_in_config_mode();
	zassert_true(rt3645_emul_in_config_mode(emul));

	page_in = (uint8_t)get_time().val % 12;
	for (int i = 0; i < RT3654_IMVP_TEST_SET_REGS_COUNT; i++) {
		regs_in[i] = (uint8_t)get_time().val;
	}

	zassert_ok(rt3645_imvp_test_fill_page_regs(
		page_in, regs_in, RT3654_IMVP_TEST_SET_REGS_COUNT));

	for (int i = 0; i < RT3654_IMVP_TEST_SET_REGS_COUNT; i++) {
		uint8_t reg_out;

		rt3645_emul_read_reg(emul, i, &reg_out);
		zassert_equal(reg_out, regs_in[i]);
	}
}

void rt3645_imvp_test_reset(void *fixture)
{
	rt3645_emul_reset_regs(emul);
}

ZTEST_SUITE(rt3645_imvp, drivers_predicate_pre_main, NULL,
	    rt3645_imvp_test_reset, NULL, NULL);
