/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/retimer/anx7452.h"
#include "emul/emul_anx7452.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT analogix_anx7452

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
LOG_MODULE_REGISTER(emul_anx7452);

/** Run-time data used by the emulator */
struct anx7452_emul_data {
	/** Common I2C data */
	struct i2c_common_emul_data common;

	/** Current state of all emulated ANX7452 retimer registers */
	uint8_t top_reg;

	uint8_t top_ctltop_i2c_addr_reg;

	uint8_t ctltop_cfg0_reg;

	uint8_t ctltop_cfg1_reg;

	uint8_t ctltop_cfg2_reg;
};

/* Workhorse for mapping i2c reg to internal emulator data access */
static uint8_t *anx7452_emul_get_reg_ptr(struct anx7452_emul_data *data,
					 int reg)
{
	switch (reg) {
	case ANX7452_TOP_REG:
		return &(data->top_reg);
	case ANX7452_TOP_CTLTOP_I2C_ADDR_REG:
		return &(data->top_ctltop_i2c_addr_reg);
	case ANX7452_CTLTOP_CFG0_REG:
		return &(data->ctltop_cfg0_reg);
	case ANX7452_CTLTOP_CFG1_REG:
		return &(data->ctltop_cfg1_reg);
	case ANX7452_CTLTOP_CFG2_REG:
		return &(data->ctltop_cfg2_reg);
	default:
		__ASSERT(false, "Unimplemented Register Access Error on 0x%x",
			 reg);
		/* Statement never reached, required for compiler warnings */
		return NULL;
	}
}

/** Check description in emul_anx7452.h */
void anx7452_emul_set_reg(const struct emul *emul, int reg, uint8_t val)
{
	if (reg == ANX7452_TOP_CTLTOP_I2C_ADDR_REG) {
		__ASSERT(false, "Write to read only register 0x%x", reg);
	}

	struct anx7452_emul_data *data = emul->data;

	uint8_t *reg_to_write = anx7452_emul_get_reg_ptr(data, reg);
	*reg_to_write = val;
}

/** Check description in emul_anx7452.h */
uint8_t anx7452_emul_get_reg(const struct emul *emul, int reg)
{
	struct anx7452_emul_data *data = emul->data;
	uint8_t *reg_to_read = anx7452_emul_get_reg_ptr(data, reg);

	return *reg_to_read;
}

/** Check description in emul_anx7452.h */
void anx7452_emul_reset(const struct emul *emul)
{
	struct anx7452_emul_data *data;

	data = emul->data;

	data->top_reg = 0x00;

	data->top_ctltop_i2c_addr_reg = ANX7452_CTLTOP_I2C_ADDR;

	data->ctltop_cfg0_reg = 0x00;

	data->ctltop_cfg1_reg = 0x00;

	data->ctltop_cfg2_reg = 0x00;
}

static int anx7452_emul_write_byte(const struct emul *emul, int reg,
				   uint8_t val, int bytes)
{
	if (reg == ANX7452_TOP_CTLTOP_I2C_ADDR_REG) {
		__ASSERT(false, "Write to read only register 0x%x", reg);
		/* Statement never reached, required for compiler warnings */
		return NULL;
	}

	struct anx7452_emul_data *data = emul->data;

	uint8_t *reg_to_write = anx7452_emul_get_reg_ptr(data, reg);
	*reg_to_write = val;

	return 0;
}

static int anx7452_emul_read_byte(const struct emul *emul, int reg,
				  uint8_t *val, int bytes)
{
	struct anx7452_emul_data *data = emul->data;
	uint8_t *reg_to_read = anx7452_emul_get_reg_ptr(data, reg);

	*val = *reg_to_read;

	return 0;
}

/* Device instantiation */

/**
 * @brief Set up a new ANX7452 retimer emulator
 *
 * This should be called for each ANX7452 retimer device that needs to be
 * emulated. It registers it with the I2C emulation controller.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */
static int anx7452_emul_init(const struct emul *emul,
			     const struct device *parent)
{
	struct anx7452_emul_data *data = emul->data;

	data->common.i2c = parent;

	i2c_common_emul_init(&data->common);

	anx7452_emul_reset(emul);

	return 0;
}

#define ANX7452_EMUL(n)                                                   \
	static struct anx7452_emul_data anx7452_emul_data_##n = {	  \
		.common = {						  \
			.write_byte = anx7452_emul_write_byte,		  \
			.read_byte = anx7452_emul_read_byte,		  \
		},							  \
	};     \
	static const struct i2c_common_emul_cfg anx7452_emul_cfg_##n = {  \
		.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)),           \
		.data = &anx7452_emul_data_##n.common,                    \
		.addr = DT_INST_REG_ADDR(n),                              \
	};                                                                \
	EMUL_DT_INST_DEFINE(n, anx7452_emul_init, &anx7452_emul_data_##n, \
			    &anx7452_emul_cfg_##n, &i2c_common_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(ANX7452_EMUL);

DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);

struct i2c_common_emul_data *
emul_anx7452_get_i2c_common_data(const struct emul *emul)
{
	return emul->data;
}
