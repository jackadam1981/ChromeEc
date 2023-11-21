/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Source file for PD CCG8 driver */

#include "console.h"
#include "usb_pd.h"

#include <stdlib.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/ccg8_pd.h>
#include <usbc/utils.h>

#define DT_DRV_COMPAT intel_pd_powermode

#define CCG8_DEV(inst) DEVICE_DT_GET(DT_DRV_INST(inst)),
const struct device *pd_pow_config_array[] = { DT_INST_FOREACH_STATUS_OKAY(
	CCG8_DEV) };

#define PD_MAX_WRITE_SIZE 4
/* Generate device tree for available PDs */

LOG_MODULE_REGISTER(INTEL_POWMODE, LOG_LEVEL_ERR);

struct ccg8_powmode_config {
	/* I2C config */
	struct i2c_dt_spec i2c;
	/* Interrupt pin to trigger power event handlers */
	struct gpio_dt_spec int_gpio;
	/* Shared interrupt pin in dual port solution */
	bool shared_irq;
};

static int intel_write_powmode(const struct device *dev, uint16_t reg,
			       uint8_t len, void *data)
{
	const struct ccg8_powmode_config *cfg = dev->config;
	uint8_t i2c_buf[PD_MAX_WRITE_SIZE + 2];
	struct i2c_msg msg;
	/*
	 * Write sequence
	 * DEV_ADDR - REG_ID_0 - REG_ID_1 - DATA0 .. DATAn
	 */
	i2c_buf[0] = reg & 0x00ff;
	i2c_buf[1] = (reg & 0xff00) >> 8;
	memcpy(&i2c_buf[2], data, len);

	msg.buf = (uint8_t *)&i2c_buf;
	msg.len = len + 2;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;
	return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int intel_read_powmode(const struct device *dev, uint16_t reg,
			      uint8_t len, void *buf)
{
	const struct ccg8_powmode_config *cfg = dev->config;
	uint8_t write_len = 2;
	uint8_t write_buf[2];
	write_buf[0] = reg & 0x00ff;
	write_buf[1] = (reg & 0xff00) >> 8;
	return i2c_write_read_dt(&cfg->i2c, write_buf, write_len, buf, len);
}

static const struct ccg8_driver_api intel_ccg8_driver_api = {
	.pd_write_powmode = intel_write_powmode,
	.pd_read_powmode = intel_read_powmode,
};

#ifdef CONFIG_CONSOLE_CMD_PD_CCG8
static int get_int_val(char *arg, void *reg, size_t val_size)
{
	char *e;
	switch (val_size) {
	case 1:
		*(uint8_t *)reg = strtol(arg, &e, 0);
		break;
	case 2:
		*(uint16_t *)reg = strtol(arg, &e, 0);
		break;
	default:
		return 1;
		break;
	}
	return 0;
}

static int cmd_read_register(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t reg;
	uint16_t len;
	uint8_t red_buf[4] = { 0 };
	int rv = get_int_val(argv[1], &reg, sizeof(reg));
	rv = get_int_val(argv[2], &len, sizeof(len));
	intel_read_powmode(pd_pow_config_array[0], reg, len, red_buf);
	for (int i = 0; i < len; i++)
		shell_print(sh, "[%d] = %x\n", i, red_buf[i]);
	return 0;
}

static int cmd_write_register(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t reg;
	get_int_val(argv[1], &reg, sizeof(reg));
	uint16_t len = argc - 2 < PD_MAX_WRITE_SIZE ? argc - 2 :
						      PD_MAX_WRITE_SIZE;
	uint8_t buf[PD_MAX_WRITE_SIZE];
	for (int i = 0; i < len; i++)
		get_int_val(argv[2 + i], &buf[i], sizeof(buf[i]));
	intel_write_powmode(pd_pow_config_array[0], reg, len, buf);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ccg_sub,
			       SHELL_CMD_ARG(read, NULL, "read from ccg PD\n",
					     cmd_read_register, 3, 0),
			       SHELL_CMD_ARG(write, NULL, "write to ccg PD\n",
					     cmd_write_register, 3, 3),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ccg, &ccg_sub, "Read for ccg8 pd\n", NULL);
#endif

#define CCG8_ALTMODE_DEFINE(inst)                                             \
	static const struct ccg8_powmode_config ccg8_powmode_config##inst = { \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                            \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),           \
		.shared_irq = DT_INST_PROP(inst, irq_shared),                 \
	};                                                                    \
                                                                              \
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL,                         \
			      &ccg8_powmode_config##inst, POST_KERNEL,        \
			      CONFIG_APPLICATION_INIT_PRIORITY,               \
			      &intel_ccg8_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CCG8_ALTMODE_DEFINE)
