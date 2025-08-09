/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power_interface.h"

#include <ctype.h>
#include <stdlib.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/shell/shell.h>

static const struct i2c_dt_spec ifc = I2C_DT_SPEC_GET(DT_NODELABEL(cros_imvp));

uint8_t cur_page;

static int imvp_read(uint8_t reg, uint8_t *val)
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

static int imvp_write(uint8_t reg, uint8_t val)
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

static int imvp_chip_unlock(uint8_t *seq, uint8_t seq_len)
{
	int rv = 0;

	for (int i = 0; (i < seq_len) && !rv; i++)
		rv = imvp_write(0xF1, seq[i]);

	return rv;
}

static int imvp_chip_set_page(const uint8_t page)
{
	int rv;

	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		return -EINVAL;
	}

	rv = imvp_write(0xEF, page);
	if (rv) {
		return rv;
	}

	cur_page = page;

	return rv;
}

static int imvp_chip_load_config(void)
{
	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		return -EINVAL;
	}
	return imvp_write(0xED, 0x66);
}

static int imvp_chip_store_config(void)
{
	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		return -EINVAL;
	}
	return imvp_write(0xED, 0xA8);
}

static int cmd_imvp_unlock(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t unlock_seq[4];
	uint8_t seq_len;

	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5\n");
		return -EINVAL;
	}

	seq_len = 0;
	for (int i = 1; i < argc; i++, seq_len++) {
		unlock_seq[i - 1] = strtol(argv[i], NULL, 0);
	}
	return imvp_chip_unlock(unlock_seq, seq_len);
}

static int cmd_imvp_load(const struct shell *sh, size_t argc, char **argv)
{
	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}
	return imvp_chip_load_config();
}

static int cmd_imvp_store(const struct shell *sh, size_t argc, char **argv)
{
	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}
	return imvp_chip_store_config();
}

static int cmd_imvp_set_page(const struct shell *sh, size_t argc, char **argv)
{
	int arg;

	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}
	arg = strtol(argv[1], NULL, 0);

	return imvp_chip_set_page(arg);
}

static void dump_reg_range(const struct shell *sh, int low, int high)
{
	uint8_t reg;
	uint8_t regval;
	int rv;

	for (reg = low; reg <= high; reg++) {
		rv = imvp_read(reg, &regval);
		if (!rv)
			shell_fprintf(sh, SHELL_INFO, "[%02Xh] = 0x%02X\n", reg,
				      regval);
		else
			shell_fprintf(sh, SHELL_INFO, "ERROR [%Xh]\n", reg);
	}
}

static int cmd_imvp_dump_regs(const struct shell *sh, size_t argc, char **argv)
{
	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}

	dump_reg_range(sh, 0xEC, 0xEC);
	dump_reg_range(sh, 0xEF, 0xEF);
	dump_reg_range(sh, 0xF9, 0xFA);
	dump_reg_range(sh, 0xFE, 0xFE);

	shell_fprintf(sh, SHELL_INFO, "Page: 0x%X\n", (int)cur_page);
	dump_reg_range(sh, 0x00, 0x12);
	if (cur_page == 0x0D) {
		dump_reg_range(sh, 0x13, 0x13);
	}

	return 0;
}

static int cmd_imvp_set_regs(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t reg, data;
	int rv = 0;

	if (!ap_power_in_or_transitioning_to_state(AP_POWER_STATE_SOFT_OFF)) {
		shell_error(sh, "Can not change regsiters while out of S5");
		return -EINVAL;
	}

	reg = strtol(argv[1], NULL, 0);

	for (int i = 2; i < argc; i++, reg++) {
		if (cur_page == 0x0D) {
			if (reg >= 0x13) {
				break;
			}
		} else if (reg >= 0x12) {
			break;
		}

		if (*argv[i] == '-') {
			continue;
		}

		data = strtol(argv[i], NULL, 0);
		rv = imvp_write(reg, data);
		if (rv)
			break;
	}

	return rv;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_imvp_cmds,
	SHELL_CMD_ARG(unlock, NULL,
		      "Unlock IMVP devices\n"
		      "Usage: imvp unlock <Sequence of bytes to unlock chip>\n",
		      cmd_imvp_unlock, 1, 4),
	SHELL_CMD(load_cfg, NULL, "Load IMVP config from NVM\n", cmd_imvp_load),
	SHELL_CMD(store_cfg, NULL, "Store IMVP config from NVM\n",
		  cmd_imvp_store),
	SHELL_CMD_ARG(set_page, NULL,
		      "Sets active page for R/W\n"
		      "Usage: imvp set_page <page number in hex>\n",
		      cmd_imvp_set_page, 2, 0),
	SHELL_CMD_ARG(set_regs, NULL,
		      "Sets chip page register\n"
		      "Usage: imvp set_regs <start_reg> [value | - ] \n",
		      cmd_imvp_set_regs, 2, 13),
	SHELL_CMD(dump_regs, NULL,
		  "Dump registers, content depends on current page\n",
		  cmd_imvp_dump_regs),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(imvp, &sub_imvp_cmds, "IMVP commands", NULL);
