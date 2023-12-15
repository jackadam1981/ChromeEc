/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "pdc/pdc.h"

#include <string.h>

#include <zephyr/device.h>

#include <drivers/pdc.h>

#ifdef CONFIG_PLATFORM_EC_HOSTCMD_PD_CHIP_INFO
/* EC_CMD_PD_CHIP_INFO implementation when a PDC is used. */

static enum ec_status hc_remote_pd_chip_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_pd_chip_info *p = args->params;
	struct ec_response_pd_chip_info_v1 info = { 0 };
	const struct device *dev;
	uint32_t output;
	int rc;

	dev = pdc_get_device_for_port(p->port);
	if (!dev) {
		return EC_RES_INVALID_PARAM;
	}

	rc = pdc_get_vid_pid(dev, &output);
	if (rc) {
		return EC_RES_ERROR;
	}

	info.vendor_id = PDC_VIDPID_GET_VID(output);
	info.product_id = PDC_VIDPID_GET_PID(output);

	rc = pdc_get_fw_version(dev, &output);
	if (rc) {
		return EC_RES_ERROR;
	}

	/* Ver output is 3 bytes right-aligned in a 32-bit container. Map into
	 * the first three bytes of fw_version_string.
	 */

	info.fw_version_string[0] = PDC_FWVER_GET_MAJOR(output);
	info.fw_version_string[1] = PDC_FWVER_GET_MINOR(output);
	info.fw_version_string[2] = PDC_FWVER_GET_PATCH(output);

	/*
	 * Take advantage of the fact that v0 and v1 structs have the
	 * same layout for v0 data. (v1 just appends data)
	 */
	args->response_size =
		args->version ? sizeof(struct ec_response_pd_chip_info_v1) :
				sizeof(struct ec_response_pd_chip_info);

	memcpy(args->response, &info, args->response_size);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_CHIP_INFO, hc_remote_pd_chip_info,
		     EC_VER_MASK(0) | EC_VER_MASK(1));
#endif /* CONFIG_HOSTCMD_PD_CHIP_INFO */
