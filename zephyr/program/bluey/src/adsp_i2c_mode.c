/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Bluey chipset-specific configuration */

#include "common.h"
#include "gpio.h"
#include "hooks.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bluey_adsp_i2c_switch, LOG_LEVEL_INF);

#define ADSP_I2C_SLAVE_ADDRESS 0x42

const struct device *i2c_dev =
	DEVICE_DT_GET(DT_PHANDLE(DT_NODELABEL(i2c_ec_i2c_adsp), i2c_port));

static int temp_cb(struct i2c_target_config *config)
{
	LOG_INF("Empty Callback No action");
	return 0;
}

static int temp_cb_val(struct i2c_target_config *config, uint8_t val)
{
	LOG_INF("Empty Callback No action");
	return 0;
}

static int temp_cb_pval(struct i2c_target_config *config, uint8_t *val)
{
	LOG_INF("Empty Callback No action");
	return 0;
}

/* i2c target mode callback defination, temp for now */
static const struct i2c_target_callbacks target_callbacks = {
	.write_requested = temp_cb,
	.write_received = temp_cb_val,
	.read_requested = temp_cb_pval,
	.read_processed = temp_cb_pval,
	.stop = temp_cb,
};

struct i2c_target_config target_cfg = {
	.address = ADSP_I2C_SLAVE_ADDRESS,
	.callbacks = &target_callbacks,
};

/* Before AP power on turn the I2C_PORT_ADSP to Target mode */
void board_chipset_pre_init_i2c_target(void)
{
	int ret;

	LOG_INF("Configuring I2C_PORT_ADSP as Target.");
	ret = i2c_target_register(i2c_dev, &target_cfg);
	if (ret < 0) {
		LOG_ERR("Failed to register I2C target (err %d)", ret);
		return;
	}
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init_i2c_target,
	     HOOK_PRIO_POST_DEFAULT);

/* After the AP is powered-off turn back the I2C_PORT_ADSP to controller mode
 * allowing to access the battery pack */
void board_chipset_shutdown_complete_i2c_controller(void)
{
	int ret;
	ret = i2c_target_unregister(i2c_dev, &target_cfg);
	if (ret < 0) {
		LOG_ERR("Failed to unregister I2C target (err %d)", ret);
		return;
	}

	// Re-configure as a controller
	LOG_INF("Configuring I2C_PORT_ADSP as Controller.");
	ret = i2c_configure(i2c_dev, I2C_MODE_CONTROLLER | I2C_SPEED_FAST);
	if (ret < 0) {
		LOG_ERR("Failed to re-configure I2C controller (err %d)", ret);
		return;
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN_COMPLETE,
	     board_chipset_shutdown_complete_i2c_controller, HOOK_PRIO_DEFAULT);
