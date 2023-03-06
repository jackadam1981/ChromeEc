/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/retimer/ps8811.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"
#include "emul/retimer/emul_ps8811.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ps8811_emul, CONFIG_PS8811_EMUL_LOG_LEVEL);

#define DT_DRV_COMPAT cros_ps8811_emul

static int ps8811_emul_p1_read_byte(const struct emul *emul, int reg,
				    uint8_t *val, int byte)
{
	/* Registers are all one byte. */
	if (byte != 0)
		return -EIO;

	return (ps8811_emul_get_reg1(emul, reg, val) == 0) ? 0 : -EIO;
}

static int ps8811_emul_p1_write_byte(const struct emul *emul, int reg,
				     uint8_t val, int bytes)
{
	/* Registers are all one byte. */
	if (bytes != 1)
		return -EIO;

	return (ps8811_emul_set_reg1(emul, reg, val) == 0) ? 0 : -EIO;
}

static int ps8811_emul_p0_read_byte(const struct emul *emul, int reg,
				    uint8_t *val, int byte)
{
	return (ps8811_emul_get_reg0(emul, reg, val) == 0) ? 0 : -EIO;
}

static int ps8811_emul_p0_write_byte(const struct emul *emul, int reg,
				     uint8_t val, int bytes)
{
	return (ps8811_emul_get_reg0(emul, reg, val) == 0) ? 0 : -EIO;
}

static int i2c_ps8811_emul_transfer(const struct emul *target,
				    struct i2c_msg *msgs, int num_msgs,
				    int addr)
{
	struct ps8811_emul_data *data = target->data;
	const struct ps8811_emul_cfg *cfg = target->cfg;

	if (addr == target->bus.i2c->addr) {
		return i2c_common_emul_transfer_workhorse(target, target->data,
							  target->cfg, msgs,
							  num_msgs, addr);
	} else if (addr == data->p1_data.emul.addr) {
		return i2c_common_emul_transfer_workhorse(target,
							  &data->p1_data,
							  &cfg->p1_cfg, msgs,
							  num_msgs, addr);
	}

	LOG_ERR("Cannot map address %02x", addr);
	return -EIO;
}

struct i2c_emul_api i2c_ps8811_emul_api = {
	.transfer = i2c_ps8811_emul_transfer,
};

static int ps8811_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	struct ps8811_emul_data *data = emul->data;
	const struct ps8811_emul_cfg *cfg = emul->cfg;
	int rv = 0;

	ps8811_emul_reset(emul);

	i2c_common_emul_init(&data->p0_data);

	data->p1_data.emul.api = &i2c_ps8811_emul_api;
	data->p1_data.emul.addr = cfg->p1_cfg.addr;
	data->p1_data.emul.target = emul;
	data->p1_data.i2c = parent;
	data->p1_data.cfg = &cfg->p1_cfg;
	i2c_common_emul_init(&data->p1_data);

	rv = i2c_emul_register(parent, &data->p1_data.emul);
	if (rv) {
		LOG_ERR("Failed to register page 1 register emulator");
		return rv;
	}

	return 0;
}

/*
 * Page 0 contains hardware revision and similar info. No code currently
 * accesses these registers so just stub them out for future use.
 */
int ps8811_emul_get_reg0(const struct emul *emulator, int reg, uint8_t *val)
{
	return -EINVAL;
}

int ps8811_emul_set_reg0(const struct emul *emulator, int reg, uint8_t val)
{
	return -EINVAL;
}

int ps8811_emul_get_reg1(const struct emul *emulator, int reg, uint8_t *val)
{
	struct ps8811_emul_data *ps8811 = emulator->data;

	switch (reg) {
	case PS8811_REG1_USB_AEQ_LEVEL:
	case PS8811_REG1_USB_ADE_CONFIG:
	case PS8811_REG1_USB_BEQ_LEVEL:
	case PS8811_REG1_USB_BDE_CONFIG:
	case PS8811_REG1_USB_CHAN_A_SWING:
	case PS8811_REG1_50OHM_ADJUST_CHAN_B:
	case PS8811_REG1_USB_CHAN_B_SWING:
	case PS8811_REG1_USB_CHAN_B_DE_PS_LSB:
	case PS8811_REG1_USB_CHAN_B_DE_PS_MSB:
		*val = ps8811->p1_regs[reg];
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int ps8811_emul_set_reg1(const struct emul *emulator, int reg, uint8_t val)
{
	struct ps8811_emul_data *ps8811 = emulator->data;

	if (reg < 0 || reg >= PS811_REG1_MAX)
		return -EINVAL;

	/* Validate that reserved bits aren't being touched. */
	switch (reg) {
	case PS8811_REG1_USB_BDE_CONFIG:
		if (val & PS8811_REG1_USB_BDE_CONFIG_RESERVED_MASK)
			return -EINVAL;
		break;

	case PS8811_REG1_USB_CHAN_A_SWING:
		if (val & PS8811_REG1_USB_CHAN_A_SWING_RESERVED_MASK)
			return -EINVAL;
		break;

	case PS8811_REG1_50OHM_ADJUST_CHAN_B:
		if (val & PS8811_REG1_50OHM_ADJUST_CHAN_B_RESERVED_MASK)
			return -EINVAL;
		break;

	case PS8811_REG1_USB_CHAN_B_SWING:
		if (val & PS8811_REG1_USB_CHAN_B_SWING_RESERVED_MASK)
			return -EINVAL;
		break;

	case PS8811_REG1_USB_CHAN_B_DE_PS_LSB:
		if (val & PS8811_REG1_USB_CHAN_B_DE_PS_LSB_RESERVED_MASK)
			return -EINVAL;
		break;

	case PS8811_REG1_USB_CHAN_B_DE_PS_MSB:
		if (val & PS8811_REG1_USB_CHAN_B_DE_PS_MSB_RESERVED_MASK)
			return -EINVAL;
		break;

	default:
		break;
	}

	switch (reg) {
	case PS8811_REG1_USB_AEQ_LEVEL:
	case PS8811_REG1_USB_ADE_CONFIG:
	case PS8811_REG1_USB_BEQ_LEVEL:
	case PS8811_REG1_USB_BDE_CONFIG:
	case PS8811_REG1_USB_CHAN_A_SWING:
	case PS8811_REG1_50OHM_ADJUST_CHAN_B:
	case PS8811_REG1_USB_CHAN_B_SWING:
	case PS8811_REG1_USB_CHAN_B_DE_PS_LSB:
	case PS8811_REG1_USB_CHAN_B_DE_PS_MSB:
		ps8811->p1_regs[reg] = val;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

void ps8811_emul_reset(const struct emul *emul)
{
	struct ps8811_emul_data *data = emul->data;

	data->p1_regs[PS8811_REG1_USB_AEQ_LEVEL] =
		PS8811_REG1_USB_AEQ_LEVEL_DEFAULT;
	data->p1_regs[PS8811_REG1_USB_ADE_CONFIG] =
		PS8811_REG1_USB_ADE_CONFIG_DEFAULT;
	data->p1_regs[PS8811_REG1_USB_BEQ_LEVEL] =
		PS8811_REG1_USB_BEQ_LEVEL_DEFAULT;
	data->p1_regs[PS8811_REG1_USB_BDE_CONFIG] =
		PS8811_REG1_USB_BDE_CONFIG_DEFAULT;
	data->p1_regs[PS8811_REG1_USB_CHAN_A_SWING] =
		PS8811_REG1_USB_CHAN_A_SWING_DEFAULT;
	data->p1_regs[PS8811_REG1_50OHM_ADJUST_CHAN_B] =
		PS8811_REG1_50OHM_ADJUST_CHAN_B_DEFAULT;
	data->p1_regs[PS8811_REG1_USB_CHAN_B_SWING] =
		PS8811_REG1_USB_CHAN_B_SWING_DEFAULT;
	data->p1_regs[PS8811_REG1_USB_CHAN_B_DE_PS_LSB] =
		PS8811_REG1_USB_CHAN_B_DE_PS_LSB_DEFAULT;
	data->p1_regs[PS8811_REG1_USB_CHAN_B_DE_PS_MSB] =
		PS8811_REG1_USB_CHAN_B_DE_PS_MSB_DEFAULT;
}

#define PS8811_EMUL(n) \
	static struct ps8811_emul_data ps8811_emul_data_##n = { \
		.p0_data = { \
			.read_byte = ps8811_emul_p0_read_byte, \
			.write_byte = ps8811_emul_p0_write_byte, \
		}, \
		.p1_data = { \
			.read_byte = ps8811_emul_p1_read_byte, \
			.write_byte = ps8811_emul_p1_write_byte, \
		}, \
	}; \
	static const struct ps8811_emul_cfg ps8811_emul_cfg_##n = { \
		.p0_cfg = { \
			.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)), \
			.data = &ps8811_emul_data_##n.p0_data, \
			.addr = DT_INST_REG_ADDR(n), \
		}, \
		.p1_cfg = { \
			.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)), \
			.data = &ps8811_emul_data_##n.p1_data, \
			.addr = DT_INST_REG_ADDR(n) + 1, \
		}, \
	}; \
	EMUL_DT_INST_DEFINE(n, ps8811_emul_init, &ps8811_emul_data_##n, \
			    &ps8811_emul_cfg_##n, &i2c_ps8811_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(PS8811_EMUL);
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE)
