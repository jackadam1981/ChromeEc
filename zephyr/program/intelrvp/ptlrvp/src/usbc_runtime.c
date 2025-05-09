/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usbc/pdc_runtime_port_config.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

#include <drivers/pdc.h>

LOG_MODULE_REGISTER(rvp_usbc, LOG_LEVEL_INF);

/* Bitmap for ports that have been probed */

/* Bitmap for ports that have been enabled */

#define PORT_PDC_DEFINE(id) \
	[USBC_PORT_NEW(id)] = DEVICE_DT_GET(DT_PROP_BY_IDX(id, pdc, 0)),

const struct device *pdc_chips[] = { DT_FOREACH_STATUS_OKAY(named_usbc_port,
							    PORT_PDC_DEFINE) };

static bool probe_pdc_chip(const struct device *dev)
{
	struct pdc_hw_config_t config;
	int rv;

	if (dev == NULL) {
		LOG_ERR("%s: Invalid pointer", __func__);
		return false;
	}

	rv = pdc_get_hw_config(dev, &config);
	if (rv) {
		LOG_ERR("%s: Cannot get bus info for PDC %s: %d", __func__,
			dev->name ? dev->name : "unnamed", rv);
		return false;
	}

	struct i2c_msg msgs[1];
	uint8_t dst;

	msgs[0].buf = &dst;
	msgs[0].len = 0U;
	msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	/* If the transfer succeeds, a chip is at this address */
	return i2c_transfer_dt(&config.i2c, &msgs[0], 1) == 0;
}

static uint8_t probe_pdc_chips(void)
{
	uint8_t ret = 0;

	for (int i = 0; i < ARRAY_SIZE(pdc_chips); i++) {
		if (probe_pdc_chip(pdc_chips[i])) {
			LOG_INF("C%d: PD Chip Found", i);
			ret |= BIT(i);
		}
	}

	return ret;
}

/** Supply pdc_power_mgmt with dynamic USB-C port configuration data */
int board_get_pdc_for_port(int port, const struct device **dev)
{
	static bool pdcs_probed = false;
	static uint8_t pdcs_avail = 0;
	uint8_t mask;

	if (pdcs_probed == false) {
		pdcs_avail = probe_pdc_chips();
		pdcs_probed = true;
	}

	if (dev == NULL) {
		return -EINVAL;
	}

	mask = BIT(port + 1) - 1;
	/* Ensure only contiguous ports are enabled */
	if ((mask & pdcs_avail) == mask) {
		*dev = pdc_chips[port];
		return 0;
	}

	*dev = NULL;
	return -ENOENT;
}
