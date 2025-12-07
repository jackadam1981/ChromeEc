/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "driver/tcpm/it83xx_pd.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"
#include "emul/tcpc/emul_it8xxx2.h"
#include "emul/tcpc/emul_tcpci.h"
#include "tcpm/tcpci.h"

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

/*
 * Note, compatible is for the parent multi-function device. The TCPC device
 * is a child to the MFD.
 *
 */
#define DT_DRV_COMPAT ite_it8xxx2

#define IT8XXX2_VENDOR_REG_START 0xC0
#define IT8XXX2_VENDOR_REG_END 0xDE
#define IT8XXX2_VENDOR_REG_COUNT \
	(IT8XXX2_VENDOR_REG_END - IT8XXX2_VENDOR_REG_START)

LOG_MODULE_REGISTER(it8xxx2_emul, CONFIG_TCPCI_EMUL_LOG_LEVEL);

/* For vendor specific registers. */
struct it8xxx2_register {
	uint8_t reg;
	uint8_t def;
	uint8_t value;
	uint8_t reserved;
};

struct it8xxx2_emul_data {
	struct it8xxx2_register regs[IT8XXX2_VENDOR_REG_COUNT];
};

const static struct it8xxx2_register
	default_reg_configs[IT8XXX2_VENDOR_REG_COUNT] = {
		{
			.reg = IT8XXX2_REG_CTRL_OUT_EN,
			.def = IT8XXX2_REG_CTRL_OUT_EN_DEFAULT,
			.reserved = IT8XXX2_REG_CTRL_OUT_EN_RESERVED_MASK,
		},
		{
			.reg = IT8XXX2_REG_VBC_FAULT_CTL,
			.def = IT8XXX2_REG_VBC_FAULT_CTL_DEFAULT,
			.value = IT8XXX2_REG_VBC_FAULT_CTL_DEFAULT,
			.reserved = IT8XXX2_REG_VBC_FAULT_CTL_RESERVED_MASK,
		}
	};

static struct it8xxx2_register *get_register_mut(const struct emul *emul,
						 int reg)
{
	struct tcpc_emul_data *tcpc_data = emul->data;
	struct it8xxx2_emul_data *it8xxx2 = tcpc_data->chip_data;

	for (size_t i = 0; i < ARRAY_SIZE(it8xxx2->regs); i++) {
		if (it8xxx2->regs[i].reg && it8xxx2->regs[i].reg == reg)
			return &it8xxx2->regs[i];
	}

	return NULL;
}

static const struct it8xxx2_register *
get_register_const(const struct emul *emul, int reg)
{
	return get_register_mut(emul, reg);
}

int it8xxx2_emul_get_reg(const struct emul *emul, int r, uint16_t *val)
{
	const struct it8xxx2_register *reg = get_register_const(emul, r);

	if (reg) {
		*val = reg->value;
		return 0;
	}

	return tcpci_emul_get_reg(emul, r, val);
}

static int it8xxx2_set_vendor_reg_raw(struct it8xxx2_register *reg,
				      uint16_t val)
{
	if ((val & reg->reserved) != (reg->def & reg->reserved)) {
		LOG_DBG("Reserved bits modified for reg %02x, val: %02x, \
			default: %02x, reserved: %02x",
			reg->reg, val, reg->def, reg->reserved);
		return -EINVAL;
	}

	reg->value = val;
	return 0;
}

int it8xxx2_emul_set_reg(const struct emul *emul, int r, uint16_t val)
{
	struct it8xxx2_register *reg = get_register_mut(emul, r);

	if (reg)
		return it8xxx2_set_vendor_reg_raw(reg, val);

	return tcpci_emul_set_reg(emul, r, val);
}

static int i2c_it8xxx2_emul_transfer(const struct emul *target,
				     struct i2c_msg *msgs, int num_msgs,
				     int addr)
{
	struct tcpc_emul_data *tcpc_data = target->data;
	struct tcpci_ctx *tcpci_ctx = tcpc_data->tcpci_ctx;

	return i2c_common_emul_transfer_workhorse(target, &tcpci_ctx->common,
						  &tcpc_data->i2c_cfg, msgs,
						  num_msgs, addr);
}

struct i2c_emul_api i2c_it8xxx2_emul_api = {
	.transfer = i2c_it8xxx2_emul_transfer,
};

static int it8xxx2_emul_tcpc_write_byte(const struct emul *emul, int r,
					uint8_t val, int bytes)
{
	struct it8xxx2_register *reg = get_register_mut(emul, r);

	if (reg) {
		/* Process vendor-defined register write. */
		if (bytes != 1) {
			LOG_DBG("Write %d bytes to single-byte register %x\n",
				r);
			return -EIO;
		}

		return it8xxx2_set_vendor_reg_raw(reg, val);
	}

	return tcpci_emul_write_byte(emul, r, val, bytes);
}

static int it8xxx2_emul_tcpc_read_byte(const struct emul *emul, int r,
				       uint8_t *val, int bytes)
{
	const struct it8xxx2_register *reg = get_register_const(emul, r);

	if (reg) {
		/* Process vendor-defined register read. */
		if (bytes != 0) {
			LOG_DBG("Read %d bytes from single-byte register %x\n",
				r);
			return -EIO;
		}

		*val = reg->value;
		return 0;
	}

	return tcpci_emul_read_byte(emul, r, val, bytes);
}

static int it8xxx2_emul_finish_write(const struct emul *emul, int reg,
				     int msg_len)
{
	/* Always report success for our vendor-specific registers. */
	if (get_register_const(emul, reg))
		return 0;

	return tcpci_emul_handle_write(emul, reg, msg_len);
}

static int it8xxx2_emul_access_reg(const struct emul *emul, int reg, int bytes,
				   bool read)
{
	return reg;
}

void it8xxx2_emul_reset(const struct emul *emul)
{
	struct tcpc_emul_data *tcpc_data = emul->data;
	struct it8xxx2_emul_data *it8xxx2 = tcpc_data->chip_data;

	memcpy(&it8xxx2->regs, default_reg_configs,
	       IT8XXX2_VENDOR_REG_COUNT * sizeof(struct it8xxx2_register));
	/* Using the setter helps catch any default misconfigs. */
	for (size_t i = 0; i < ARRAY_SIZE(it8xxx2->regs); i++) {
		if (it8xxx2->regs[i].reg == 0)
			continue;

		it8xxx2_emul_set_reg(emul, it8xxx2->regs[i].reg,
				     it8xxx2->regs[i].def);
	}

	tcpci_emul_reset(emul);
}

static int it8xxx2_emul_init(const struct emul *emul,
			     const struct device *parent)
{
	struct tcpc_emul_data *tcpc_data = emul->data;
	struct tcpci_ctx *tcpci_ctx = tcpc_data->tcpci_ctx;

	tcpci_ctx->common.access_reg = it8xxx2_emul_access_reg;
	tcpci_ctx->common.read_byte = it8xxx2_emul_tcpc_read_byte;
	tcpci_ctx->common.finish_write = it8xxx2_emul_finish_write;
	tcpci_ctx->common.write_byte = it8xxx2_emul_tcpc_write_byte;

	tcpci_emul_i2c_init(emul, parent);
	it8xxx2_emul_reset(emul);
	return 0;
}

#define IT8XXX2_EMUL(n)                                                       \
	static struct it8xxx2_emul_data it8xxx2_emul_data_##n;                \
	TCPCI_EMUL_DEFINE(n, it8xxx2_emul_init, NULL, &it8xxx2_emul_data_##n, \
			  &i2c_it8xxx2_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(IT8XXX2_EMUL);

#define IT8XXX2_EMUL_RESET_RULE_AFTER(n) \
	it8xxx2_emul_reset(EMUL_DT_GET(DT_DRV_INST(n)));

static void it8xxx2_emul_test_reset(const struct ztest_unit_test *test,
				    void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	DT_INST_FOREACH_STATUS_OKAY(IT8XXX2_EMUL_RESET_RULE_AFTER)
}

ZTEST_RULE(emul_it8xxx2_reset, NULL, it8xxx2_emul_test_reset);

#ifndef CONFIG_MFD_IT8XXX2
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);
#endif
