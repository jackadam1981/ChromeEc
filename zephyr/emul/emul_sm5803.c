/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "driver/charger/sm5803.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_sm5803.h"
#include "emul/emul_stub_device.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#define DT_DRV_COMPAT cros_sm5803_emul

LOG_MODULE_REGISTER(sm5803_emul, CONFIG_SM5803_EMUL_LOG_LEVEL);

#define VBUS_GPADC_LSB_MV 23.4
#define CHG_DET_THRESHOLD_MV 4000

struct sm5803_emul_data {
	struct i2c_common_emul_data i2c_main;
	struct i2c_common_emul_data i2c_chg;
	struct i2c_common_emul_data i2c_meas;

	uint8_t input_current_limit;
	uint8_t fast_charge_current_limit;
	uint8_t gpadc_conf1, gpadc_conf2;
	uint8_t irq1, irq2, irq3, irq4;
	uint16_t vbus_mv;
};

struct sm5803_emul_cfg {
	const struct i2c_common_emul_cfg i2c_main;
	const struct i2c_common_emul_cfg i2c_chg;
	const struct i2c_common_emul_cfg i2c_meas;
	struct gpio_dt_spec interrupt_gpio;
};

const struct gpio_dt_spec *
sm5803_emul_get_interrupt_gpio(const struct emul *emul)
{
	const struct sm5803_emul_cfg *cfg = emul->cfg;

	return &cfg->interrupt_gpio;
}

struct i2c_common_emul_data *sm5803_emul_get_i2c_main(const struct emul *emul)
{
	struct sm5803_emul_data *data = emul->data;

	return &data->i2c_main;
}

struct i2c_common_emul_data *sm5803_emul_get_i2c_chg(const struct emul *emul)
{
	struct sm5803_emul_data *data = emul->data;

	return &data->i2c_chg;
}

struct i2c_common_emul_data *sm5803_emul_get_i2c_meas(const struct emul *emul)
{
	struct sm5803_emul_data *data = emul->data;

	return &data->i2c_meas;
}

int sm5803_emul_read_chg_reg(const struct emul *emul, uint8_t reg)
{
	struct sm5803_emul_data *data = emul->data;

	switch (reg) {
	case SM5803_REG_CHG_ILIM:
		return data->input_current_limit;
	case SM5803_REG_FAST_CONF4:
		return data->fast_charge_current_limit;
	}
	return -ENOTSUP;
}

void sm5803_emul_set_vbus_voltage(const struct emul *emul, uint16_t mv)
{
	struct sm5803_emul_data *data = emul->data;
	uint16_t old = (float)data->vbus_mv * VBUS_GPADC_LSB_MV;

	data->vbus_mv = (uint16_t)((float)mv / VBUS_GPADC_LSB_MV);

	if (MIN(mv, old) <= CHG_DET_THRESHOLD_MV &&
	    MAX(mv, old) > CHG_DET_THRESHOLD_MV) {
		/* CHG_DET changes state; trigger an interrupt. */
		sm5803_emul_set_irqs(emul, SM5803_INT1_CHG, 0, 0, 0);
	}
}

static void update_interrupt_pin(const struct emul *emul)
{
	struct sm5803_emul_data *data = emul->data;
	const struct sm5803_emul_cfg *cfg = emul->cfg;

	bool pending = data->irq1 || data->irq2 || data->irq3 || data->irq4;

	/* Pin goes low if any IRQ is pending. */
	gpio_emul_input_set(cfg->interrupt_gpio.port, cfg->interrupt_gpio.pin,
			    !pending);
}

void sm5803_emul_set_irqs(const struct emul *emul, uint8_t irq1, uint8_t irq2,
			  uint8_t irq3, uint8_t irq4)
{
	struct sm5803_emul_data *data = emul->data;

	data->irq1 |= irq1;
	data->irq2 |= irq2;
	data->irq3 |= irq3;
	data->irq4 |= irq4;
	update_interrupt_pin(emul);
}

static bool is_chg_det(struct sm5803_emul_data *data)
{
	/* Assume charger presence is cut off at 4V VBUS. */
	return data->vbus_mv * VBUS_GPADC_LSB_MV > CHG_DET_THRESHOLD_MV;
}

void sm5803_emul_set_gpadc_conf(const struct emul *emul, uint8_t conf1,
				uint8_t conf2)
{
	struct sm5803_emul_data *data = emul->data;

	data->gpadc_conf1 = conf1;
	data->gpadc_conf2 = conf2;
}

static void sm5803_emul_reset(const struct emul *emul)
{
	struct sm5803_emul_data *data = emul->data;
	const struct sm5803_emul_cfg *cfg = emul->cfg;

	i2c_common_emul_set_read_fail_reg(&data->i2c_main,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_write_fail_reg(&data->i2c_main,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_read_fail_reg(&data->i2c_chg,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_write_fail_reg(&data->i2c_chg,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_read_fail_reg(&data->i2c_meas,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_write_fail_reg(&data->i2c_meas,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Registers set to chip reset values */
	data->input_current_limit = 4;
	data->fast_charge_current_limit = 0;
	data->gpadc_conf1 = 0xf3;
	data->gpadc_conf2 = 0x01;
	data->irq1 = data->irq2 = data->irq3 = data->irq4 = 0;
	data->vbus_mv = 0;

	/* Interrupt pin deasserted */
	gpio_emul_input_set(cfg->interrupt_gpio.port, cfg->interrupt_gpio.pin,
			    1);
}

static int sm5803_main_read_byte(const struct emul *target, int reg,
				 uint8_t *val, int bytes)
{
	struct sm5803_emul_data *data = target->data;

	switch (reg) {
	case SM5803_REG_STATUS1:
		*val = is_chg_det(data) ? SM5803_STATUS1_CHG_DET : 0;
		return 0;
	case SM5803_REG_INT1_REQ:
		*val = data->irq1;
		/* register clears on read */
		data->irq1 = 0;
		update_interrupt_pin(target);
		return 0;
	case SM5803_REG_INT2_REQ:
		*val = data->irq2;
		/* register clears on read */
		data->irq2 = 0;
		update_interrupt_pin(target);
		return 0;
	case SM5803_REG_INT3_REQ:
		*val = data->irq3;
		/* register clears on read */
		data->irq3 = 0;
		update_interrupt_pin(target);
		return 0;
	case SM5803_REG_INT4_REQ:
		*val = data->irq4;
		/* register clears on read */
		data->irq4 = 0;
		update_interrupt_pin(target);
		return 0;
	}
	LOG_INF("SM5803 main page read of register %#x unhandled", reg);
	return -ENOTSUP;
}

static int sm5803_main_write_byte(const struct emul *target, int reg,
				  uint8_t val, int bytes)
{
	LOG_INF("SM5803 main page write of register %#x unhandled", reg);
	return -ENOTSUP;
}

static int sm5803_chg_read_byte(const struct emul *target, int reg,
				uint8_t *val, int bytes)
{
	struct sm5803_emul_data *data = target->data;

	switch (reg) {
	case SM5803_REG_CHG_ILIM:
		*val = data->input_current_limit;
		return 0;
	case SM5803_REG_FAST_CONF4:
		*val = data->fast_charge_current_limit | GENMASK(7, 6);
		return 0;
	}
	LOG_INF("SM5803 charger page read of register %#x unhandled", reg);
	return -ENOTSUP;
}

static int sm5803_chg_write_byte(const struct emul *target, int reg,
				 uint8_t val, int bytes)
{
	struct sm5803_emul_data *data = target->data;

	switch (reg) {
	case SM5803_REG_CHG_ILIM:
		data->input_current_limit = val & GENMASK(4, 0);
		return 0;
	case SM5803_REG_FAST_CONF4:
		data->fast_charge_current_limit = val & GENMASK(5, 0);
		return 0;
	}
	LOG_INF("SM5803 charger page write of register %#x unhandled", reg);
	return -ENOTSUP;
}

static int sm5803_meas_read_byte(const struct emul *target, int reg,
				 uint8_t *val, int bytes)
{
	struct sm5803_emul_data *data = target->data;

	switch (reg) {
	case SM5803_REG_GPADC_CONFIG1:
		*val = data->gpadc_conf1;
		return 0;
	case SM5803_REG_GPADC_CONFIG2:
		*val = data->gpadc_conf2;
		return 0;
	case SM5803_REG_VBUS_MEAS_MSB:
		*val = (data->vbus_mv & GENMASK(9, 2)) >> 2;
		return 0;
	case SM5803_REG_VBUS_MEAS_LSB:
		*val = (is_chg_det(data) ? SM5803_VBUS_MEAS_CHG_DET : 0) |
		       (data->vbus_mv & GENMASK(1, 0));
		return 0;
	}
	LOG_INF("SM5803 meas page read of register %#x unhandled", reg);
	return -ENOTSUP;
}

static int sm5803_meas_write_byte(const struct emul *target, int reg,
				  uint8_t val, int bytes)
{
	LOG_INF("SM5803 meas page write of register %#x unhandled", reg);
	return -ENOTSUP;
}

static int sm5803_emul_i2c_transfer(const struct emul *target,
				    struct i2c_msg *msgs, int num_msgs,
				    int addr)
{
	struct sm5803_emul_data *data = target->data;
	const struct sm5803_emul_cfg *cfg = target->cfg;

	if (addr == cfg->i2c_main.addr) {
		return i2c_common_emul_transfer_workhorse(target,
							  &data->i2c_main,
							  &cfg->i2c_main, msgs,
							  num_msgs, addr);
	} else if (addr == cfg->i2c_chg.addr) {
		return i2c_common_emul_transfer_workhorse(target,
							  &data->i2c_chg,
							  &cfg->i2c_chg, msgs,
							  num_msgs, addr);
	} else if (addr == cfg->i2c_meas.addr) {
		return i2c_common_emul_transfer_workhorse(target,
							  &data->i2c_meas,
							  &cfg->i2c_meas, msgs,
							  num_msgs, addr);
	}
	LOG_ERR("I2C transaction for address %#x not supported by SM5803",
		addr);
	return -ENOTSUP;
}

const static struct i2c_emul_api sm5803_emul_api = {
	.transfer = sm5803_emul_i2c_transfer,
};

static int sm5803_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	struct sm5803_emul_data *data = emul->data;
	struct i2c_common_emul_data *const i2c_pages[] = {
		&data->i2c_chg,
		&data->i2c_main,
		&data->i2c_meas,
	};

	for (int i = 0; i < ARRAY_SIZE(i2c_pages); i++) {
		int rv = i2c_emul_register(parent, &i2c_pages[i]->emul);

		if (rv != 0) {
			k_oops();
		}
		i2c_common_emul_init(i2c_pages[i]);
	}

	sm5803_emul_reset(emul);

	return 0;
}

#define INIT_SM5803(n)                                                         \
	const static struct sm5803_emul_cfg sm5803_emul_cfg_##n;               \
	static struct sm5803_emul_data sm5803_emul_data_##n = {                \
		.i2c_main =                                                    \
			(struct i2c_common_emul_data){                         \
				.i2c = DEVICE_DT_GET(DT_INST_PARENT(n)),       \
				.emul =                                        \
					(struct i2c_emul){                     \
						.target = EMUL_DT_GET(         \
							DT_DRV_INST(n)),       \
						.api = &sm5803_emul_api,       \
						.addr = DT_INST_PROP(          \
							n, main_addr),         \
					},                                     \
				.cfg = &sm5803_emul_cfg_##n.i2c_main,          \
				.read_byte = &sm5803_main_read_byte,           \
				.write_byte = &sm5803_main_write_byte,         \
			},                                                     \
		.i2c_chg =                                                     \
			(struct i2c_common_emul_data){                         \
				.i2c = DEVICE_DT_GET(DT_INST_PARENT(n)),       \
				.emul =                                        \
					(struct i2c_emul){                     \
						.target = EMUL_DT_GET(         \
							DT_DRV_INST(n)),       \
						.api = &sm5803_emul_api,       \
						.addr = DT_INST_REG_ADDR(n),   \
					},                                     \
				.cfg = &sm5803_emul_cfg_##n.i2c_chg,           \
				.read_byte = &sm5803_chg_read_byte,            \
				.write_byte = &sm5803_chg_write_byte,          \
			},                                                     \
		.i2c_meas =                                                    \
			(struct i2c_common_emul_data){                         \
				.i2c = DEVICE_DT_GET(DT_INST_PARENT(n)),       \
				.emul =                                        \
					(struct i2c_emul){                     \
						.target = EMUL_DT_GET(         \
							DT_DRV_INST(n)),       \
						.api = &sm5803_emul_api,       \
						.addr = DT_INST_PROP(          \
							n, meas_addr),         \
					},                                     \
				.cfg = &sm5803_emul_cfg_##n.i2c_meas,          \
				.read_byte = &sm5803_meas_read_byte,           \
				.write_byte = &sm5803_meas_write_byte,         \
			},                                                     \
	};                                                                     \
	const static struct sm5803_emul_cfg sm5803_emul_cfg_##n = {          \
		.i2c_main =                                                  \
			(struct i2c_common_emul_cfg){                        \
				.dev_label =                                 \
					DT_NODE_FULL_NAME(DT_DRV_INST(n)),   \
				.addr = DT_INST_PROP(n, main_addr),          \
				.data = &sm5803_emul_data_##n.i2c_main,      \
			},                                                   \
		.i2c_chg =                                                   \
			(struct i2c_common_emul_cfg){                        \
				.dev_label =                                 \
					DT_NODE_FULL_NAME(DT_DRV_INST(n)),   \
				.addr = DT_INST_REG_ADDR(n),                 \
				.data = &sm5803_emul_data_##n.i2c_chg,       \
			},                                                   \
		.i2c_meas =                                                  \
			(struct i2c_common_emul_cfg){                        \
				.dev_label =                                 \
					DT_NODE_FULL_NAME(DT_DRV_INST(n)),   \
				.addr = DT_INST_PROP(n, meas_addr),          \
				.data = &sm5803_emul_data_##n.i2c_meas,      \
			},                                                   \
		.interrupt_gpio = {                                          \
			.port = DEVICE_DT_GET(DT_GPIO_CTLR(DT_DRV_INST(n),   \
							   interrupt_gpios)),\
			.pin = DT_INST_GPIO_PIN(n, interrupt_gpios),         \
			.dt_flags = DT_INST_GPIO_FLAGS(n, interrupt_gpios),  \
		},                                                           \
	}; \
	EMUL_DT_INST_DEFINE(n, sm5803_emul_init, &sm5803_emul_data_##n,        \
			    &sm5803_emul_cfg_##n, &sm5803_emul_api, NULL);

DT_INST_FOREACH_STATUS_OKAY(INIT_SM5803)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE)

static void sm5803_emul_reset_before(const struct ztest_unit_test *test,
				     void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

#define SM5803_EMUL_RESET_RULE_BEFORE(n) \
	sm5803_emul_reset(EMUL_DT_GET(DT_DRV_INST(n)));

	DT_INST_FOREACH_STATUS_OKAY(SM5803_EMUL_RESET_RULE_BEFORE);
}
ZTEST_RULE(sm5803_emul_reset, sm5803_emul_reset_before, NULL);
