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

static struct amd_sb_rmi_mailbox_emul *amd_sb_rmi_mailbox_find(const struct emul *emul, uint8_t cmd)
{
	struct amd_sb_rmi_emul_data *data = emul->data;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&data->mailboxes, node) {
		struct amd_sb_rmi_mailbox_emul *mailbox;
		const struct amd_sb_rmi_mailbox_emul_cfg *cfg;

		mailbox = CONTAINER_OF(node, struct amd_sb_rmi_mailbox_emul, node);
		if (!emul)
			continue;

		cfg = mailbox->api->get_config(mailbox->target);
		for (size_t i = 0; i < cfg->num_commands; i++) {
			if (cmd == cfg->commands[i])
				return mailbox;
		}
	}

	return NULL;
}

static void amd_sb_rmi_handle_mailbox_command(const struct emul *emul)
{
	const struct amd_sb_rmi_mailbox_emul *mailbox; 
	const struct amd_sb_rmi_mailbox_emul_api *api;
	uint32_t in_data = 0;
	uint32_t out_data;
	uint8_t cmd;
	uint8_t tmp;
	uint8_t err;
	int rv;

	const int in_regs[] = {
		SB_RMI_IN_BND_MSG4_REG,
		SB_RMI_IN_BND_MSG3_REG,
		SB_RMI_IN_BND_MSG2_REG,
		SB_RMI_IN_BND_MSG1_REG,
	};

	const int out_regs[] = {
		SB_RMI_OUT_BND_MSG4_REG,
		SB_RMI_OUT_BND_MSG3_REG,
		SB_RMI_OUT_BND_MSG2_REG,
		SB_RMI_OUT_BND_MSG1_REG,
	};

	/* Command is in in[0]. */
	rv = amd_sb_rmi_emul_get_reg(emul, SB_RMI_IN_BND_MSG0_REG, &cmd);
	if (!rv) {
		LOG_ERR("Failed to read mailbox command, rv %d", rv);
		amd_sb_rmi_emul_mailbox_finish(emul, cmd, SB_RMI_MAILBOX_ERROR_ABORTED);
		return;
	}

	mailbox = amd_sb_rmi_mailbox_find(emul, cmd);
	if (!mailbox || !mailbox->api) {
		LOG_ERR("Cannot find mailbox for command %02x", cmd);
		amd_sb_rmi_emul_mailbox_finish(emul, cmd, SB_RMI_MAILBOX_ERROR_UNKNOWN_CMD);
		return;
	}

	/* Data is split across in[1:4]. */
	for (size_t i = 0; i < ARRAY_SIZE(in_regs); i++) {
		in_data <<= 8;
		rv = amd_sb_rmi_emul_get_reg(emul, in_regs[i], &tmp);
		if (rv) {
			LOG_ERR("Failed to read mailbox data for command %02x, rv %d", cmd, rv);
			amd_sb_rmi_emul_mailbox_finish(emul, cmd, SB_RMI_MAILBOX_ERROR_ABORTED);
			return;
		}
		in_data |= tmp;
	}

	api = mailbox->api;
	if (!api->handle_command) {
		LOG_ERR("Mailbox '%s' cannot handle commands", emul->dev->name);
		amd_sb_rmi_emul_mailbox_finish(emul, cmd, SB_RMI_MAILBOX_ERROR_UNKNOWN_CMD);
		return;
	}

	err = api->handle_command(mailbox->target, cmd, in_data, &out_data);

	/* Copy output data to out[1:4]. */
	for (size_t i = 0; i < ARRAY_SIZE(out_regs); i++) {
		tmp = (out_data >> (i * 8)) & 0xff;
		rv = amd_sb_rmi_emul_set_reg(emul, out_regs[i], tmp);
		if (rv) {
			LOG_ERR("Failed to set mailbox data for command %02x, rv %d", cmd, rv);
			amd_sb_rmi_emul_mailbox_finish(emul, cmd, SB_RMI_MAILBOX_ERROR_ABORTED);
			return;
		}
	}

	amd_sb_rmi_emul_mailbox_finish(emul, cmd, err);
}

void amd_sb_rmi_emul_mailbox_finish(const struct emul *emul, uint8_t cmd, uint8_t err)
{
	int rv;

	/* Copy command to out[0]. */
	rv = amd_sb_rmi_emul_set_reg(emul, SB_RMI_OUT_BND_MSG0_REG, cmd);
	if (rv) {
		LOG_ERR("Failed to set mailbox out command %02x, rv %d\n", cmd, rv);
		return;
	}

	/* Error code is put in out[7]. */
	rv = amd_sb_rmi_emul_set_reg(emul, SB_RMI_OUT_BND_MSG7_REG, err);
	if (rv) {
		LOG_ERR("Failed to copy mailbox command %02x, rv %d", cmd, rv);
		return;
	}

	/* Set done bit. */
	//TODO
}

int amd_sb_rmi_emul_mailbox_register(const struct device *dev, struct amd_sb_rmi_mailbox_emul *emul)
{
	struct amd_sb_rmi_emul_data *data = NULL;
	const char *name;

	if (!dev || !dev->data || !emul->target || !emul->target->dev) {
		LOG_ERR("Failed to register mailbox emulator, invalid arguments");
		return -EINVAL;
	}

	data = dev->data;
	name = emul->target->dev->name;

	sys_slist_append(&data->mailboxes, &emul->node);
	LOG_INF("Register mailbox emulator '%s'", name);

	return 0;
}

static int amd_sb_rmi_emul_init(const struct emul *emul,
			     const struct device *parent)
{
	struct amd_sb_rmi_emul_data *data = emul->data;
	const struct amd_sb_rmi_emul_cfg *cfg = emul->cfg;

	//sys_slist_init(&data->mailboxes);
	amd_sb_rmi_emul_reset(emul);
	i2c_common_emul_init(&data->common);

#if 0
	/* Register our mailbox emulators. */
	for (size_t i = 0; i < cfg->num_mailboxes; i++) {
		const struct emul *mailbox = emul_get_binding(cfg->mailboxes[i]->name);
		const struct amd_sb_rmi_mailbox_emul_api *api;
		
		if (!emul || !emul->backend_api) {
			LOG_ERR("Failed to find emulator for mailbox device '%s'", cfg->mailboxes[i]->name);
			continue;
		}

		api = emul->backend_api;
		if (!api->get_mailbox_emul) {
			LOG_ERR("Emulator '%s' cannot supply mailbox emulator");
			continue;
		}

		amd_sb_rmi_emul_mailbox_register(emul->dev, api->get_mailbox_emul(mailbox));
	}
#endif

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

	//TODO
	amd_sb_rmi_handle_mailbox_command(emul);
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

#define EMUL_CHILD_DEVICE(n) DEVICE_DT_GET(n),

#define AMD_SB_RMI_EMUL(n)                                                   \
	static const struct device *amd_sb_rmi_emul_mailboxes_##n[] = { \
		DT_FOREACH_CHILD_STATUS_OKAY(DT_DRV_INST(n), EMUL_CHILD_DEVICE) \
	}; \
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
		.mailboxes = amd_sb_rmi_emul_mailboxes_##n, \
		.num_mailboxes = ARRAY_SIZE(amd_sb_rmi_emul_mailboxes_##n) \
	};   \
	EMUL_DT_INST_DEFINE(n, amd_sb_rmi_emul_init, &amd_sb_rmi_emul_data_##n, \
			    &amd_sb_rmi_emul_cfg_##n, &i2c_common_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(AMD_SB_RMI_EMUL)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE)