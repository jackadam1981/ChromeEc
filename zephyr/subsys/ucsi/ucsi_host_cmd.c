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

#include <zephyr/logging/log.h>

static struct ucsi_ppm_driver *ec_ppm_drv;

LOG_MODULE_REGISTER(ucsi, LOG_LEVEL_ERR);

static int rts5453_ucsi_init_ppm(struct ucsi_pd_device* device)
{
	return 0;
}

static struct ucsi_ppm_driver* rts5453_ucsi_get_ppm(
		struct ucsi_pd_device* device)
{
	struct rts5453_device *dev = (struct rts5453_device*)device;
	return dev->ppm;
}

static int rts5453_ucsi_execute_cmd(struct ucsi_pd_device* device,
                                    struct ucsi_control* control,
                                    uint8_t* lpm_data_out)
{
	return 0;
}

static int rts5453_ucsi_handle_interrupt(struct ucsi_pd_device* device)
{
	return 0;
}

static void rts5453_ucsi_cleanup(struct ucsi_pd_driver* driver)
{
	return;
}

static void ucsi_init(void)
{
	struct rts5453_device *dev = NULL;
	struct ucsi_pd_driver *drv = NULL;

	dev = platform_calloc(1, sizeof(struct rts5453_device));
	if (!dev) {
		goto handle_error;
	}

	dev->smbus = NULL;
	dev->driver_config = NULL;

	drv = platform_calloc(1, sizeof(struct ucsi_pd_driver));
	if (!drv) {
		goto handle_error;
	}

	drv->dev = (struct ucsi_pd_device*) dev;

	drv->init_ppm = rts5453_ucsi_init_ppm;
	drv->get_ppm = rts5453_ucsi_get_ppm;
	drv->execute_cmd = rts5453_ucsi_execute_cmd;
	drv->handle_interrupt = rts5453_ucsi_handle_interrupt;
	drv->cleanup = rts5453_ucsi_cleanup;

	// Initialize the PPM.
	dev->ppm = ppm_open(drv);
	if (!dev->ppm) {
		ELOG("Failed to open PPM");
		goto handle_error;
	}
	ec_ppm_drv = dev->ppm;

handle_error:
	if (dev && dev->ppm) {
		dev->ppm->cleanup(dev->ppm);
		dev->ppm = NULL;
	}

	platform_free(dev);
	platform_free(drv);
}
DECLARE_HOOK(HOOK_INIT, ucsi_init, HOOK_PRIO_DEFAULT);

#if 1
static enum ec_status hc_ucsi_ppm_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_set *p = args->params;

	if (!ec_ppm_drv)
		return EC_RES_UNAVAILABLE;

	if (ec_ppm_drv->write(ec_ppm_drv->dev, p->offset, p->data,
			      args->params_size - sizeof(p->offset)))
		return EC_RES_ERROR;

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

#else

static enum ec_status hc_ucsi_ppm(struct host_cmd_handler_args *args)
{
	struct ppm_common_device* dev = DEV_CAST_FROM(ec_ppm_drv->dev);
	struct ec_params_ucsi_ppm *p = args->params;
	/* Payload size */
	uint16_t length = args->params_size - sizeof(p->subcmd);
	enum ec_status rv = EC_RES_SUCCESS;
	const void *buf = p->data;

	switch (p->subcmd) {
	case EC_UCSI_SUBCMD_SET_CONTROL:
		rv = ppm_common_handle_control_message(dev, buf, length);
		break;
	case EC_UCSI_SUBCMD_SET_MESSAGE:
		memcpy(dev->ucsi_data.message_out, buf, length);
		break;
	default:
		ELOG("Invalid command received: 0x%02x", p->subcmd);
		rv = EC_RES_INVALID_COMMAND;
	}

	return rv;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM, hc_ucsi_ppm, EC_VER_MASK(0));

#endif
