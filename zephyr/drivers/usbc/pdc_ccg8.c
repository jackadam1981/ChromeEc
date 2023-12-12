/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Source file for PD CCG8 driver */

#include "usb_mux.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <drivers/pd_chip.h>
#include <drivers/pdc_ccg8.h>
#include <usbc/utils.h>

#define DT_DRV_COMPAT infineon_ccg8

#define CCG8_DEV(inst) DEVICE_DT_GET(DT_DRV_INST(inst)),

/* Generate device object array for available PDs */
static const struct device *pd_pow_config_array[] = {
	DT_INST_FOREACH_STATUS_OKAY(CCG8_DEV)
};

LOG_MODULE_REGISTER(CCG8, LOG_LEVEL_ERR);

struct ccg8_config {
	/* I2C config */
	struct i2c_dt_spec i2c;
	/* Interrupt pin to trigger power event handlers */
	struct gpio_dt_spec int_gpio;
	/* Shared interrupt pin in dual port solution */
	bool shared_irq;
};

static int pd_write_ccg(const struct device *dev, uint16_t reg, uint8_t len,
			void *data)
{
	const struct ccg8_config *cfg = dev->config;
	uint8_t i2c_buf[PD_MAX_READ_WRITE_SIZE + 2];
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

static int pd_read_ccg(const struct device *dev, uint16_t reg, uint8_t len,
		       void *buf)
{
	const struct ccg8_config *cfg = dev->config;

	return i2c_write_read_dt(&cfg->i2c, (uint8_t *)&reg, 2, buf, len);
}

static int pd_ccg8_init(const struct device *dev)
{
	const struct ccg8_config *cfg = dev->config;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C is not ready");
		return -ENODEV;
	}

	/* TODO: initialiaze CCG interrupt and callback functions */

	return 0;
}

static int pd_intel_retimer_fw_update_ccg(const struct device *dev, bool enable)
{
	return pd_write_ccg(dev, PD_ICL_CTRL_REG, PD_ICL_CTRL_REG_LEN,
			    (uint8_t *)&enable);
}

static const struct pdc_driver_api ccg8_api = {
	.pd_intel_retimer_fw_update = pd_intel_retimer_fw_update_ccg,
};

/*
 * TODO : Move console command to application and make it generic for
 * for all PD chips
 */
#ifdef CONFIG_CONSOLE_CMD_PD_CCG8
struct cmd_args {
	uint8_t port;
	uint16_t reg;
	uint8_t len;
	uint8_t data[PD_MAX_READ_WRITE_SIZE];
};

static int get_int_val(char *arg, void *reg, size_t val_size)
{
	char *e;

	uint16_t val = strtoul(arg, &e, 0);
	if (*e)
		return -EINVAL;

	switch (val_size) {
	case 1:
		if (val > UINT8_MAX)
			return -EINVAL;
		*(uint8_t *)reg = (uint8_t)val;
		break;
	case 2:
		*(uint16_t *)reg = val;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int get_port(char *arg, uint8_t *port)
{
	char *e;

	*port = strtoul(arg, &e, 0);
	if (*e || (*port >= CONFIG_USB_PD_PORT_MAX_COUNT))
		return -EINVAL;

	return 0;
}

static int process_arguments(const struct shell *sh, char **argv,
		struct cmd_args *args)
{
	int rv;

	/* Convert port to int */
	rv = get_port(argv[1], &args->port);
	if (rv) {
		shell_error(sh, "Invalid port");
		return rv;
	}

	/* convert register to int */
	rv = get_int_val(argv[2], &args->reg, sizeof(args->reg));
	if (rv) {
		shell_error(sh, "Invalid register");
		return rv;
	}

	return 0;
}

static int cmd_read_register(const struct shell *sh, size_t argc, char **argv)
{
	struct cmd_args read_args;
	int rv;

	/* Process command arguments */
	rv = process_arguments(sh, argv, &read_args);
	if (rv)
		return rv;

	/* Convert register size to int */
	rv = get_int_val(argv[3], &read_args.len, sizeof(read_args.len));
	if (rv) {
		shell_error(sh, "Invalid length");
		return rv;
	}
	read_args.len = MIN(read_args.len, PD_MAX_READ_WRITE_SIZE);

	/* Read from PD registers */
	rv = pd_read_ccg(pd_pow_config_array[read_args.port], read_args.reg,
			read_args.len, read_args.data);
	if (rv) {
		shell_error(sh, "Read Failed, rv = %d", rv);
		return rv;
	}
	for (int i = 0; i < read_args.len; i++)
		shell_info(sh, "[%d] = %x", i, read_args.data[i]);

	return 0;
}

static int cmd_write_register(const struct shell *sh, size_t argc, char **argv)
{
	struct cmd_args write_args;
	int rv;

	/* Process command arguments */
	write_args.len = MIN(argc - 3, PD_MAX_READ_WRITE_SIZE);
	rv = process_arguments(sh, argv, &write_args);
	if (rv)
		return rv;

	/* Convert data to write to int */
	for (int i = 0; i < write_args.len; i++) {
		rv = get_int_val(argv[3 + i], &write_args.data[i],
				 sizeof(write_args.data[i]));
		if (rv) {
			shell_error(sh, "Invalid data");
			return rv;
		}
	}

	/* Write to PD registers */
	rv = pd_write_ccg(pd_pow_config_array[write_args.port], write_args.reg,
			write_args.len, write_args.data);
	if (rv) {
		shell_error(sh, "Write failed, rv = %d", rv);
		return rv;
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	ccg_sub_cmds,
	SHELL_CMD_ARG(read, NULL,
		      "read from ccg PD\n"
		      "usage: read <port> <reg> <bytes>",
		      cmd_read_register, 4, 0),
	SHELL_CMD_ARG(write, NULL,
		      "write to ccg PD\n"
		      "usage: write <port> <reg> [<byte0>,...]",
		      cmd_write_register, 4, 3),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ccg, &ccg_sub_cmds, "CCG commands\n", NULL);
#endif

#define CCG8_DEFINE(inst)                                           \
	static const struct ccg8_config ccg8_config##inst = {       \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                  \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios), \
		.shared_irq = DT_INST_PROP(inst, irq_shared),       \
	};                                                          \
								    \
	DEVICE_DT_INST_DEFINE(inst, pd_ccg8_init, NULL, NULL,       \
			      &ccg8_config##inst, POST_KERNEL,      \
			      CONFIG_APPLICATION_INIT_PRIORITY, &ccg8_api);

DT_INST_FOREACH_STATUS_OKAY(CCG8_DEFINE)
