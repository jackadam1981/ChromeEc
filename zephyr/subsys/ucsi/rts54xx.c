/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI host command */

#include "include/pd_driver.h"
#include "include/ppm.h"
#include "ppm_common.h"
#include "usb_pd.h"
#include "util.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include <drivers/pdc.h>

LOG_MODULE_REGISTER(rts54xx, LOG_LEVEL_INF);

#define DT_DRV_COMPAT ucsi_lpm_driver
#define DEV_CAST_FROM(v) (struct ppm_common_device *)(v)
#define CAST_FROM(v) (struct rts5453_device *)(v)
#define UCSI_7BIT_PORTMASK(p) ((p) & 0x7F)
#define DT_LPM_DRV DT_INST(0, ucsi_lpm_driver)
#define NUM_PORTS DT_PROP_LEN(DT_LPM_DRV, pdc)

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

	const struct device *pdc[NUM_PORTS];
};

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
	case UCSI_CMD_GET_ALTERNATE_MODES:
		conn = UCSI_7BIT_PORTMASK(control->command_specific[1]);
		if (conn == 0 || conn > dev->num_ports)
			return -EINVAL;
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

#define PHANDLE_TO_DEV(node_id, prop, idx) \
	[idx] = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static struct rts5453_device rts54xx_dev = {
	.pdc = { DT_FOREACH_PROP_ELEM(DT_LPM_DRV, pdc, PHANDLE_TO_DEV) },
};

static struct ucsi_pd_driver rts54xx_drv = {
	.init_ppm = rts54xx_ucsi_init_ppm,
	.get_ppm = rts54xx_ucsi_get_ppm,
	.execute_cmd = rts54xx_ucsi_execute_cmd,
	.dev = (struct ucsi_pd_device *)&rts54xx_dev,
};

static struct ucsiv3_get_connector_status_data
	port_status[NUM_PORTS] __aligned(4);

static int rts54xx_init(const struct device *device)
{
	const struct ucsi_pd_driver *drv = device->api;
	struct ppm_common_device *ppm_dev;
	struct rts5453_device *dev = device->data;

	/* Initialize the PPM. */
	dev->ppm = ppm_open(drv);
	if (!dev->ppm) {
		LOG_ERR("Failed to open PPM");
		return -ENODEV;
	}

	ppm_dev = DEV_CAST_FROM(dev->ppm->dev);
	ppm_dev->num_ports = ARRAY_SIZE(port_status);
	ppm_dev->per_port_status = port_status;

	return 0;
}
DEVICE_DT_INST_DEFINE(0, &rts54xx_init, NULL, &rts54xx_dev, NULL, POST_KERNEL,
		      CONFIG_PDC_POWER_MGMT_INIT_PRIORITY, &rts54xx_drv);
