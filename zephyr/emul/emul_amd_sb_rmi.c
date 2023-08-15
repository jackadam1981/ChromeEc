/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/sb_rmi.h"
#include "emul/emul_amd_sb_rmi.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#define DT_DRV_COMPAT amd_sb_rmi

//TODO:
LOG_MODULE_REGISTER(amd_sb_rmi_emul, 4);

static const struct amd_sb_rmi_register default_reg_configs[SB_RMI_REG_MAX] = {
	/* Out-bound message registers. */
	{
		.reg = SB_RMI_OUT_BND_MSG0_REG,
	},
	{
		.reg = SB_RMI_OUT_BND_MSG1_REG,
	},
	{
		.reg = SB_RMI_OUT_BND_MSG2_REG,
	},
	{
		.reg = SB_RMI_OUT_BND_MSG3_REG,
	},
	{
		.reg = SB_RMI_OUT_BND_MSG4_REG,
	},
	{
		.reg = SB_RMI_OUT_BND_MSG5_REG,
		.reserved = SB_RMI_OUT_BND_MSG5_REG_RESERVED,
	},
	{
		.reg = SB_RMI_OUT_BND_MSG6_REG,
		.reserved = SB_RMI_OUT_BND_MSG6_REG_RESERVED,
	},
	{
		.reg = SB_RMI_OUT_BND_MSG7_REG,
	},

	/* In-bound message registers. */
	{
		.reg = SB_RMI_IN_BND_MSG0_REG,
	},
	{
		.reg = SB_RMI_IN_BND_MSG1_REG,
	},
	{
		.reg = SB_RMI_IN_BND_MSG2_REG,
	},
	{
		.reg = SB_RMI_IN_BND_MSG3_REG,
	},
	{
		.reg = SB_RMI_IN_BND_MSG4_REG,
	},
	{
		.reg = SB_RMI_IN_BND_MSG5_REG,
		.reserved = SB_RMI_IN_BND_MSG5_REG_RESERVED,
	},
	{
		.reg = SB_RMI_IN_BND_MSG6_REG,
		.reserved = SB_RMI_IN_BND_MSG6_REG_RESERVED,
	},
	{
		.reg = SB_RMI_IN_BND_MSG7_REG,
	},

	/* Status and control registers. */
	{
		.reg = SB_RMI_STATUS_REG,
		.reserved = SB_RMI_STATUS_REG_RESERVED,
	},
	{
		.reg = SB_RMI_SW_INTR_REG,
		.reserved = SB_RMI_SW_INTR_REG_RESERVED,
	},
};

static struct amd_sb_rmi_register *get_register_mut(const struct emul *emul,
						    int reg)
{
	struct amd_sb_rmi_emul_data *sb_rmi = emul->data;

	for (size_t i = 0; i < SB_RMI_REG_MAX; i++) {
		if (sb_rmi->regs[i].reg == reg)
			return &sb_rmi->regs[i];
	}

	return NULL;
}

static const struct amd_sb_rmi_register *
get_register_const(const struct emul *emul, int reg)
{
	return get_register_mut(emul, reg);
}

int amd_sb_rmi_emul_get_reg(const struct emul *emul, int r, uint8_t *val)
{
	const struct amd_sb_rmi_register *reg = get_register_const(emul, r);

	if (!reg) {
		LOG_DBG("Unknown register %x", r);
		return -EINVAL;
	}

	*val = reg->value;
	return 0;
}

int amd_sb_rmi_emul_set_reg(const struct emul *emul, int r, uint8_t val)
{
	struct amd_sb_rmi_register *reg = get_register_mut(emul, r);

	if (!reg) {
		LOG_DBG("Unknown register %x", r);
		return -EINVAL;
	}

	if ((val & reg->reserved) != (reg->def & reg->reserved)) {
		LOG_DBG("Reserved bits modified for reg %02x, val: %02x, \
			default: %02x, reserved: %02x",
			r, val, reg->def, reg->reserved);
		return -EINVAL;
	}

	reg->value = val;
	return 0;
}

void amd_sb_rmi_emul_reset(const struct emul *emul)
{
	struct amd_sb_rmi_emul_data *sb_rmi = emul->data;

	/* Initialize our default register config. */
	memcpy(&sb_rmi->regs, default_reg_configs,
	       SB_RMI_REG_MAX * sizeof(struct amd_sb_rmi_register));

	/* Using the setter helps catch any default misconfigs. */
	for (size_t i = 0; i < SB_RMI_REG_MAX; i++)
		amd_sb_rmi_emul_set_reg(emul, sb_rmi->regs[i].reg,
					sb_rmi->regs[i].def);
}

static int amd_sb_rmi_emul_init(const struct emul *emul,
			     const struct device *parent)
{
	struct amd_sb_rmi_emul_data *sb_rmi = emul->data;

	amd_sb_rmi_emul_reset(emul);
	i2c_common_emul_init(&sb_rmi->common);

	return 0;
}

static int amd_sb_rmi_emul_read_byte(const struct emul *emul, int reg,
				     uint8_t *val, int byte)
{
	/* Registers are only one byte. */
	if (byte != 0)
		return -EIO;

	return (amd_sb_rmi_emul_get_reg(emul, reg, val) == 0) ? 0 : -EIO;
}

static int amd_sb_rmi_emul_write_byte(const struct emul *emul, int reg,
				      uint8_t val, int bytes)
{
	/* Registers are only one byte. */
	if (bytes != 1)
		return -EIO;
	return (amd_sb_rmi_emul_set_reg(emul, reg, val) == 0) ? 0 : -EIO;
}


#define AMD_SB_RMI_EMUL_RESET_RULE_AFTER(n) \
	amd_sb_rmi_emul_reset(EMUL_DT_GET(DT_DRV_INST(n)));

static void amd_sb_rmi_emul_test_reset(const struct ztest_unit_test *test,
				    void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	DT_INST_FOREACH_STATUS_OKAY(AMD_SB_RMI_EMUL_RESET_RULE_AFTER)
}

ZTEST_RULE(emul_amd_sb_rmi_reset, NULL, amd_sb_rmi_emul_test_reset);

#define AMD_SB_RMI_EMUL(n)                                                   \
	static struct amd_sb_rmi_emul_data amd_sb_rmi_emul_data_##n = { \
		.common = { \
			.read_byte = amd_sb_rmi_emul_read_byte, \
			.write_byte = amd_sb_rmi_emul_write_byte, \
		}, \
	};       \
	static const struct amd_sb_rmi_emul_cfg amd_sb_rmi_emul_cfg_##n = { \
		.common = { \
			.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)), \
			.data = &amd_sb_rmi_emul_data_##n.common, \
			.addr = DT_INST_REG_ADDR(n), \
		}, \
	};   \
	EMUL_DT_INST_DEFINE(n, amd_sb_rmi_emul_init, &amd_sb_rmi_emul_data_##n, \
			    &amd_sb_rmi_emul_cfg_##n, &i2c_common_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(AMD_SB_RMI_EMUL)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE)