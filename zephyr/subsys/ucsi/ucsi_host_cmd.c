/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI host command */

#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "include/pd_driver.h"
#include "include/platform.h"
#include "include/ppm.h"
#include "ppm_common.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ucsi, LOG_LEVEL_INF);

#define DEV_CAST_FROM(v) (struct ppm_common_device *)(v)

static struct ucsi_ppm_driver *ec_ppm_drv;

struct ucsi_pd_driver *rts5453_open(void);

static void ucsi_notify(void *context)
{
	LOG_INF("Notified");
	pd_send_host_event(PD_EVENT_PPM);
}

/* Sort of main */
void ucsi_init(void)
{
	struct ucsi_pd_driver *drv;

	drv = rts5453_open();
	if (!drv) {
		LOG_ERR("Failed to open rts5453");
		return;
	}

	/* Start a PPM task. */
	if (drv->init_ppm(drv->dev)) {
		LOG_ERR("Failed to init PPM");
		return;
	}

	LOG_INF("Initialized PPM");

	ec_ppm_drv = drv->get_ppm(drv->dev);
	ec_ppm_drv->register_notify(ec_ppm_drv->dev, ucsi_notify, NULL);
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
