/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI host command */

#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "include/platform.h"
#include "include/ppm.h"
#include "ppm_common.h"
#include "rts5453.h"
#include "task.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ucsi, LOG_LEVEL_INF);

#define DT_DRV_COMPAT realtek_rts54_pdc

#define NUM_PDC_RTS54XX_PORTS DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)
#define DEV_CAST_FROM(v) (struct ppm_common_device *)(v)

static struct ucsi_ppm_driver *ec_ppm_drv;

struct smbus_driver *smbus_open(const struct i2c_dt_spec *i2c,
				const struct gpio_dt_spec *irq_gpios);

struct pdc_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec irq_gpios;
};

/*
 * Create PDC configs based on the usb-c devicetree.
 */
#define PDC_DEFINE(inst)                                     \
static const struct pdc_config pdc_config_##inst = {          \
	.i2c = I2C_DT_SPEC_INST_GET(inst),                   \
	.irq_gpios = GPIO_DT_SPEC_INST_GET(inst, irq_gpios), \
};
DT_INST_FOREACH_STATUS_OKAY(PDC_DEFINE)

/* Sort of main */
static void ucsi_init(void)
{
	struct smbus_driver *smbus;
	struct ucsi_pd_driver *drv;

	LOG_INF("pdc_config_0.i2c = 0x%4x", pdc_config_0.i2c.addr);
	LOG_INF("pdc_config_1.i2c = 0x%4x", pdc_config_1.i2c.addr);

	/*
	 * TODO: Should initialize each port in a loop:
	 * for (i = 0; i < NUM_PDC_RTS54XX_PORTS; i++) { ... }
	 */
	smbus = smbus_open(&pdc_config_0.i2c, &pdc_config_0.irq_gpios);

	drv = rts5453_open(smbus, NULL);
	if (!drv)
		return;

	ec_ppm_drv = drv->get_ppm(drv->dev);

	/* Start a LPM task. */
	drv->configure_lpm_irq(drv->dev);

	/* Start a PPM task. */
	drv->init_ppm(drv->dev);
}
DECLARE_HOOK(HOOK_INIT, ucsi_init, HOOK_PRIO_DEFAULT);

static enum ec_status hc_ucsi_ppm_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_set *p = args->params;
	struct ppm_common_device *dev;

	if (!ec_ppm_drv)
		return EC_RES_UNAVAILABLE;

	if (ec_ppm_drv->write(ec_ppm_drv->dev, p->offset, p->data,
			      args->params_size - sizeof(p->offset)))
		return EC_RES_ERROR;

	/* Wake up PPM task. */
	dev = DEV_CAST_FROM(ec_ppm_drv->dev);
	platform_condvar_signal(dev->ppm_condvar);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM_SET, hc_ucsi_ppm_set, EC_VER_MASK(0));

static enum ec_status hc_ucsi_ppm_get(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_get *p = args->params;
	int length;

	if (!ec_ppm_drv)
		return EC_RES_UNAVAILABLE;

	length = ec_ppm_drv->read(ec_ppm_drv->dev, p->offset, args->response,
				  p->size);
	if (length < 0)
		return EC_RES_ERROR;

	args->response_size = length;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM_GET, hc_ucsi_ppm_get, EC_VER_MASK(0));
