/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power_interface.h"
#include "rt3645_regs.h"

#include <ctype.h>
#include <stdlib.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/shell/shell.h>

#define DT_DRV_COMPAT richtek_rt3645

#define RT3645_PAGE_INVALID 0xFF
#define RT3645_LAST_PAGE 0x0D

#define RT3645_LAST_PAGE_REGISTER 0x12

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
	"only one 'richtek,rt3645' compatible node may be present");

static const struct i2c_dt_spec ifc = I2C_DT_SPEC_GET(DT_DRV_INST(0));

static uint8_t cur_page = RT3645_PAGE_INVALID;

static int rt3645_read(uint8_t reg, uint8_t *val)
{
	struct i2c_msg msg[2];

	msg[0].buf = &reg;
	msg[0].len = 1;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = val;
	msg[1].len = 1;
	msg[1].flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP;

	return i2c_transfer_dt(&ifc, msg, 2);
}

static int rt3645_write(uint8_t reg, uint8_t val)
{
	struct i2c_msg msg;
	uint8_t data[2];

	data[0] = reg;
	data[1] = val;

	msg.buf = data;
	msg.len = 2;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(&ifc, &msg, 1);
}

static int rt3645_set_cfg_mode(const uint8_t *seq, uint8_t seq_len)
{
	int rv = 0;

	for (int i = 0; (i < seq_len) && !rv; i++)
		rv = rt3645_write(RT3645_ENTER_CONFIG_MODE, seq[i]);

	return rv;
}

static int rt3645_set_page(const uint8_t page)
{
	int rv;

	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		return -EINVAL;
	}

	rv = rt3645_write(RT3645_PAGE, page);
	if (rv) {
		return rv;
	}

	cur_page = page;

	return rv;
}

static int rt3645_load_config(void)
{
	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		return -EINVAL;
	}
	return rt3645_write(RT3645_NVM_PROGRAM_CTRL,
			    RT3645_NVM_PROGRAM_CTRL_RELOAD);
}

static int rt3645_store_config(void)
{
	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		return -EINVAL;
	}
	return rt3645_write(RT3645_NVM_PROGRAM_CTRL,
			    RT3645_NVM_PROGRAM_CTRL_PROGRAM);
}

static int cmd_rt3645_enter_cfg_mode(const struct shell *sh, size_t argc,
				     char **argv)
{
	uint8_t unlock_seq[4];
	uint8_t seq_len;

	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5\n");
		return -EINVAL;
	}

	seq_len = 0;
	for (int i = 1; i < argc; i++, seq_len++) {
		unlock_seq[i - 1] = strtol(argv[i], NULL, 0);
	}
	return rt3645_set_cfg_mode(unlock_seq, seq_len);
}

static int cmd_rt3645_load(const struct shell *sh, size_t argc, char **argv)
{
	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}
	return rt3645_load_config();
}

static int cmd_rt3645_store(const struct shell *sh, size_t argc, char **argv)
{
	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}
	return rt3645_store_config();
}

static int cmd_rt3645_set_page(const struct shell *sh, size_t argc, char **argv)
{
	unsigned int arg;

	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}
	arg = (unsigned int)strtol(argv[1], NULL, 0);

	if (arg > RT3645_LAST_PAGE) {
		shell_error(sh, "Page number out of range");
		return -EINVAL;
	}

	return rt3645_set_page(arg);
}

static void dump_reg_range(const struct shell *sh, int low, int high)
{
	uint8_t reg;
	uint8_t regval;
	int rv;

	for (reg = low; reg <= high; reg++) {
		rv = rt3645_read(reg, &regval);
		if (!rv)
			shell_fprintf(sh, SHELL_INFO, "[%02Xh] = 0x%02X\n", reg,
				      regval);
		else
			shell_fprintf(sh, SHELL_INFO, "ERROR [%Xh]\n", reg);
	}
}

static int cmd_rt3645_dump_regs(const struct shell *sh, size_t argc,
				char **argv)
{
	dump_reg_range(sh, 0xEC, 0xEC);
	dump_reg_range(sh, 0xEF, 0xEF);
	dump_reg_range(sh, 0xF9, 0xFA);
	dump_reg_range(sh, 0xFE, 0xFE);

	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_print(sh, "To access paged registers put system on S5");
		return -EINVAL;
	}

	if (cur_page == RT3645_PAGE_INVALID) {
		shell_print(sh, "No active page set");
		return 0;
	}

	shell_fprintf(sh, SHELL_INFO, "Page: 0x%X\n", (int)cur_page);
	dump_reg_range(sh, 0x00, RT3645_LAST_PAGE_REGISTER);
	if (cur_page == RT3645_LAST_PAGE) {
		dump_reg_range(sh, (RT3645_LAST_PAGE_REGISTER + 1),
			       (RT3645_LAST_PAGE_REGISTER + 1));
	}

	return 0;
}

static int cmd_rt3645_set_regs(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t reg, data, last_page_reg;
	int rv = 0;

	if (!ap_power_in_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}

	if (cur_page == RT3645_PAGE_INVALID) {
		shell_error(sh, "Not active page set");
		return -EINVAL;
	}

	reg = strtol(argv[1], NULL, 0);

	/* Last register page has an extra register */
	last_page_reg = cur_page < RT3645_LAST_PAGE ?
				RT3645_LAST_PAGE_REGISTER :
				RT3645_LAST_PAGE_REGISTER + 1;
	for (int i = 2; (i < argc) && (reg <= last_page_reg); i++, reg++) {
		if (*argv[i] == '-') {
			continue;
		}

		data = strtol(argv[i], NULL, 0);
		rv = rt3645_write(reg, data);
		if (rv)
			break;
	}

	return rv;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_imvp_cmds,
	SHELL_CMD_ARG(cfg_mode, NULL,
		      "Enter password to set IMVP chip in config mode\n"
		      "Usage: imvp cfg_mode <Password sequence>\n",
		      cmd_rt3645_enter_cfg_mode, 1, 4),
	SHELL_CMD(load_cfg, NULL, "Load IMVP config from NVM\n",
		  cmd_rt3645_load),
	SHELL_CMD(store_cfg, NULL, "Store IMVP config into NVM\n",
		  cmd_rt3645_store),
	SHELL_CMD_ARG(set_page, NULL,
		      "Sets active page for R/W\n"
		      "Usage: imvp set_page <page number in hex>\n",
		      cmd_rt3645_set_page, 2, 0),
	SHELL_CMD_ARG(set_regs, NULL,
		      "Sets chip paged registers\n"
		      "Usage: imvp set_regs <start_reg> [value | - ] \n",
		      cmd_rt3645_set_regs, 2, 13),
	SHELL_CMD(dump_regs, NULL,
		  "Dump registers, content depends on current page\n",
		  cmd_rt3645_dump_regs),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(imvp, &sub_imvp_cmds, "IMVP commands", NULL);
