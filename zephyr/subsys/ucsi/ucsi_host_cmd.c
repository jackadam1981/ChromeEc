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

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include <drivers/pdc.h>

LOG_MODULE_REGISTER(ucsi, LOG_LEVEL_INF);

#define DT_DRV_COMPAT realtek_rts54_pdc
#define DEV_CAST_FROM(v) (struct ppm_common_device *)(v)
#define CAST_FROM(v) (struct rts5453_device *)(v)
#define SMBUS_MAX_BLOCK_SIZE 32
#define UCSI_7BIT_PORTMASK(p) ((p) & 0x7F)
#define NUM_PDC_RTS54XX_PORTS DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)

struct rts5453_ucsi_commands {
	uint8_t command;
	uint8_t command_copy_length;
};

#define UCSI_CMD_ENTRY(cmd, length)                            \
	{                                                      \
		.command = cmd, .command_copy_length = length, \
	}

struct rts5453_ucsi_commands ucsi_commands[UCSI_CMD_VENDOR_CMD + 1] = {
	UCSI_CMD_ENTRY(UCSI_CMD_RESERVED, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_PPM_RESET, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_CANCEL, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_CONNECTOR_RESET, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_ACK_CC_CI, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_NOTIFICATION_ENABLE, 3),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CAPABILITY, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CONNECTOR_CAPABILITY, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_CCOM, 2),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_UOR, 2),
	UCSI_CMD_ENTRY(obsolete_UCSI_CMD_SET_PDM, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_PDR, 2),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_ALTERNATE_MODES, 4),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CAM_SUPPORTED, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CURRENT_CAM, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_NEW_CAM, 6),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_PDOS, 3),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CABLE_PROPERTY, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CONNECTOR_STATUS, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_ERROR_STATUS, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_POWER_LEVEL, 6),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_PD_MESSAGE, 4),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_ATTENTION_VDO, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_reserved_0x17, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CAM_CS, 2),
	UCSI_CMD_ENTRY(UCSI_CMD_LPM_FW_UPDATE_REQUEST, 4),
	UCSI_CMD_ENTRY(UCSI_CMD_SECURITY_REQUEST, 5),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_RETIMER_MODE, 5),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_SINK_PATH, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_PDOS, 3),
	UCSI_CMD_ENTRY(UCSI_CMD_READ_POWER_LEVEL, 3),
	UCSI_CMD_ENTRY(UCSI_CMD_CHUNKING_SUPPORT, 1),
	UCSI_CMD_ENTRY(UCSI_CMD_VENDOR_CMD, 6),
};

struct rts5453_device {
	/* PPM driver (common implementation). */
	struct ucsi_ppm_driver *ppm;

	/* Number of active ports from |GET_CAPABILITIES|. */
	uint8_t active_port_count;

	const struct device *pdc[NUM_PDC_RTS54XX_PORTS];
};

static struct ucsi_ppm_driver *ppm_drv;

static int rts54xx_ucsi_init_ppm(struct ucsi_pd_device *device)
{
	struct rts5453_device *dev = CAST_FROM(device);

	return dev->ppm->init_and_wait(dev->ppm->dev, ARRAY_SIZE(dev->pdc));
}

static struct ucsi_ppm_driver *
rts54xx_ucsi_get_ppm(struct ucsi_pd_device *device)
{
	struct rts5453_device *dev = CAST_FROM(device);

	return dev->ppm;
}

static int rts54xx_ucsi_execute_cmd(struct ucsi_pd_device *device,
				    struct ucsi_control *control,
				    uint8_t *lpm_data_out)
{
	struct rts5453_device *rts = CAST_FROM(device);
	struct ppm_common_device *dev = DEV_CAST_FROM(rts->ppm->dev);
	uint8_t ucsi_command = control->command;
	uint8_t conn; /* 1:port=0, 2:port=1, ... */
	uint8_t data_size;

	if (control->command == 0 || control->command > UCSI_CMD_VENDOR_CMD) {
		LOG_ERR("Invalid command 0x%x", control->command);
		return -1;
	}

	switch (ucsi_command) {
	case UCSI_CMD_GET_ALTERNATE_MODES:
	case UCSI_CMD_PPM_RESET:
	case UCSI_CMD_SET_NOTIFICATION_ENABLE:
		return -ENOTSUP;
	case UCSI_CMD_CONNECTOR_RESET:
	case UCSI_CMD_GET_CONNECTOR_CAPABILITY:
	case UCSI_CMD_GET_CAM_SUPPORTED:
	case UCSI_CMD_GET_CURRENT_CAM:
	case UCSI_CMD_SET_NEW_CAM:
	case UCSI_CMD_GET_PDOS:
	case UCSI_CMD_GET_CABLE_PROPERTY:
	case UCSI_CMD_GET_CONNECTOR_STATUS:
	case UCSI_CMD_GET_ERROR_STATUS:
	case UCSI_CMD_GET_PD_MESSAGE:
	case UCSI_CMD_GET_ATTENTION_VDO:
	case UCSI_CMD_GET_CAM_CS:
		conn = UCSI_7BIT_PORTMASK(control->command_specific[0]);
		/*
		 * The OPS can set conn=0 for ERROR_STATUS if the error happened
		 * in PPM. The OPS will know it was an error specific to a
		 * connector when it sees -EINVAL.
		 */
		if (conn == 0 || conn > dev->num_ports) {
			LOG_ERR("Invalid conn=%d", conn);
			return -EINVAL;
		}
		break;
	default:
		conn = 1;
	}

	data_size = ucsi_commands[ucsi_command].command_copy_length;
	LOG_INF("%s: Executing conn=%u cmd=0x%02x data_size=%d", __func__, conn,
		ucsi_command, data_size);
	return pdc_execute_command(rts->pdc[conn - 1], ucsi_command, data_size,
				   control->command_specific, lpm_data_out);
}

static void rts54xx_ucsi_cleanup(struct ucsi_pd_driver *driver)
{
}

static void ppm_cci_cb(union cci_event_t cci_event, void *cb_data)
{
	struct ucsi_ppm_driver *drv = cb_data;
	struct ppm_common_device *dev = (struct ppm_common_device *)drv->dev;

	if (dev->ppm_state == PPM_STATE_IDLE ||
	    dev->ppm_state == PPM_STATE_NOT_READY) {
		LOG_INF("%s: Not ready to handle CCI", __func__);
		return;
	}

	memcpy(&dev->ucsi_data.cci, &cci_event, sizeof(cci_event));

	if (cci_event.connector_change) {
		LOG_INF("%s: CI conn=%d", __func__, cci_event.connector_change);
		dev->pending.async_event = 1;
		dev->last_connector_alerted = cci_event.connector_change;
	}

	LOG_INF("%s: Waking up PPM", __func__);
	platform_condvar_signal(dev->ppm_condvar);
}

int rts54_set_handler_cb_ex(const struct device *dev,
			    pdc_cci_handler_cb_t cci_cb, void *cb_data);

static struct ucsi_pd_driver *rts54xx_open(void)
{
	static struct rts5453_device dev;
	static struct ucsi_pd_driver drv;
	struct ppm_common_device *ppm_dev;
	static struct ucsiv3_get_connector_status_data
		port_status[NUM_PDC_RTS54XX_PORTS];

	drv.dev = (struct ucsi_pd_device *)&dev;
	drv.init_ppm = rts54xx_ucsi_init_ppm;
	drv.get_ppm = rts54xx_ucsi_get_ppm;
	drv.execute_cmd = rts54xx_ucsi_execute_cmd;
	drv.cleanup = rts54xx_ucsi_cleanup;
	dev.pdc[0] = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p0));
	dev.pdc[1] = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p1));

	/* Initialize the PPM. */
	dev.ppm = ppm_open(&drv);
	if (!dev.ppm) {
		LOG_ERR("Failed to open PPM");
		return NULL;
	}

	ppm_dev = DEV_CAST_FROM(dev.ppm->dev);
	ppm_dev->num_ports = ARRAY_SIZE(port_status);
	ppm_dev->per_port_status = port_status;

	for (int i = 0; i < ARRAY_SIZE(dev.pdc); i++) {
		rts54_set_handler_cb_ex(dev.pdc[i], ppm_cci_cb, dev.ppm);
	}

	return &drv;
}

static void opm_notify(void *context)
{
	LOG_INF("Notifying OPM");
	pd_send_host_event(PD_EVENT_PPM);
}

/* Sort of main */
void eppm_init(void)
{
	struct ucsi_pd_driver *drv;

	drv = rts54xx_open();
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

	ppm_drv = drv->get_ppm(drv->dev);
	ppm_drv->register_notify(ppm_drv->dev, opm_notify, NULL);
}
DECLARE_HOOK(HOOK_INIT, eppm_init, HOOK_PRIO_DEFAULT);

static enum ec_status hc_ucsi_ppm_set(struct host_cmd_handler_args *args)
{
	const struct ec_params_ucsi_ppm_set *p = args->params;
	struct ppm_common_device *dev;

	if (!ppm_drv)
		return EC_RES_UNAVAILABLE;

	if (ppm_drv->write(ppm_drv->dev, p->offset, p->data,
			   args->params_size - sizeof(p->offset)))
		return EC_RES_ERROR;

	/* Wake up PPM task. */
	dev = DEV_CAST_FROM(ppm_drv->dev);
	platform_condvar_signal(dev->ppm_condvar);

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

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_UCSI_PPM_GET, hc_ucsi_ppm_get, EC_VER_MASK(0));
