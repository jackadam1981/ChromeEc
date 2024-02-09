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

#define PDC_I2C_PORT 0
#define PDC_I2C_ADDRESS 0
#define GPIO_LPM_INTERRUPT 0

#define DEV_CAST_FROM(v) (struct ppm_common_device *)(v)

static struct ucsi_ppm_driver *ec_ppm_drv;

struct smbus_driver *smbus_open(int bus_num, uint8_t chip_address,
				int gpio_line);

/* Sort of main */
static void ucsi_init(void)
{
	struct smbus_driver *smbus;
	struct ucsi_pd_driver *drv;

	smbus = smbus_open(PDC_I2C_PORT, PDC_I2C_ADDRESS, GPIO_LPM_INTERRUPT);

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

	if (!ec_ppm_drv)
		return EC_RES_UNAVAILABLE;

	if (ec_ppm_drv->write(ec_ppm_drv->dev, p->offset, p->data,
			      args->params_size - sizeof(p->offset)))
		return EC_RES_ERROR;

	/* Wake up PPM task. */
	//task_wake(TASK_ID_PPM);

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
