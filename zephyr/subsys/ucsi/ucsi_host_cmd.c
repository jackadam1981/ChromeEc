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
#include "util.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ucsi, LOG_LEVEL_DBG);

#define DEV_CAST_FROM(v) (struct ppm_common_device *)(v)

static struct ucsi_ppm_driver *ppm_drv;

void ppm_cci_cb(union cci_event_t cci, void *cb_data)
{
	struct ppm_common_device *dev = DEV_CAST_FROM(ppm_drv->dev);

	LOG_INF("%s %s%s(0x%08x)", __func__, cci.connector_change ? "CI " : "",
		cci.command_completed ? "CC " : "", cci.raw_value);

	if (dev->ppm_state == PPM_STATE_IDLE ||
	    dev->ppm_state == PPM_STATE_NOT_READY) {
		LOG_INF("%s: Not ready to handle CCI", __func__);
		return;
	}

	memcpy(&dev->ucsi_data.cci, &cci, sizeof(cci));

	if (cci.connector_change) {
		dev->pending.async_event = 1;
		dev->last_connector_alerted = cci.connector_change;
	}

	LOG_INF("%s: Waking up PPM", __func__);
	platform_condvar_signal(dev->ppm_condvar);
}

static void opm_notify(void *context)
{
	LOG_INF("Notifying OPM");
	pd_send_host_event(PD_EVENT_PPM);
}

/* Sort of main */
static int eppm_init(void)
{
	const struct ucsi_pd_driver *drv;
	const struct device *pdc_dev;
	struct ppm_common_device *ppm_dev;

	LOG_DBG("%s", __func__);

	pdc_dev = DEVICE_DT_GET(DT_INST(0, ucsi_ppm_driver));
	drv = pdc_dev->api;
	if (!drv) {
		LOG_ERR("Failed to open PDC");
		return -ENODEV;
	}

	/* Start a PPM task. */
	if (drv->init_ppm(pdc_dev)) {
		LOG_ERR("Failed to init PPM");
		return -ENODEV;
	}

	ppm_drv = drv->get_ppm(pdc_dev);
	ppm_dev = DEV_CAST_FROM(ppm_drv->dev);
	LOG_INF("Initialized PPM num_ports=%u", ppm_dev->num_ports);
	ppm_drv->register_notify(ppm_drv->dev, opm_notify, NULL);

	return 0;
}
SYS_INIT(eppm_init, APPLICATION, 99);

static enum ec_status hc_ucsi_ppm_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_set *p = args->params;

	if (!ppm_drv)
		return EC_RES_UNAVAILABLE;

	size_t len = args->params_size - sizeof(p->offset);
	LOG_DBG("%s: offset=0x%02x size=%u", __func__, p->offset, len);
	hexdumpk(p->data, len);
	cflush();

	if (ppm_drv->write(ppm_drv->dev, p->offset, p->data,
			   args->params_size - sizeof(p->offset)))
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM_SET, hc_ucsi_ppm_set, EC_VER_MASK(0));

static enum ec_status hc_ucsi_ppm_get(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_get *p = args->params;
	int len;

	if (!ppm_drv)
		return EC_RES_UNAVAILABLE;

	len = ppm_drv->read(ppm_drv->dev, p->offset, args->response, p->size);
	if (len < 0)
		return EC_RES_ERROR;

	args->response_size = len;

	LOG_DBG("%s: offset=0x%02x size=%u len=%d", __func__, p->offset,
		p->size, len);
	hexdumpk(args->response, len);
	cflush();

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM_GET, hc_ucsi_ppm_get, EC_VER_MASK(0));
