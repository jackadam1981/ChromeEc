/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_clock_control_emul

#include <device.h>
#include <drivers/clock_control.h>
#include <kernel.h>

#include "emul/emul_clock_control.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(clock_control_emul, CONFIG_CLOCK_CONTROL_EMUL_LOG_LEVEL);

/** Data needed to maintain the current emulator state */
struct emul_clock_ctrl_data {
	/** The current clock rate */
	uint32_t rate;
	/** The current clock status */
	enum clock_control_status status;
	/** Async k_work structure */
	struct k_work async_on_work;
	/** Async callback */
	clock_control_cb_t cb;
	/** Async user data to pass to the callback */
	void *cb_user_data;
	/** Async device to pass to the callback */
	const struct device *cb_dev;
	/** Async subsystem to pass to the callback */
	clock_control_subsys_t cb_subsys;
};

static int drv_clock_ctrl_on(const struct device *dev,
			     clock_control_subsys_t sys)
{
	struct emul_clock_ctrl_data *data = dev->data;

	data->status = CLOCK_CONTROL_STATUS_ON;
	return 0;
}

static int drv_clock_ctrl_off(const struct device *dev,
			      clock_control_subsys_t sys)
{
	struct emul_clock_ctrl_data *data = dev->data;

	data->status = CLOCK_CONTROL_STATUS_OFF;
	return 0;
}

static void clock_ctrl_on_from_work(struct k_work *work)
{
	struct emul_clock_ctrl_data *data =
		CONTAINER_OF(work, struct emul_clock_ctrl_data, async_on_work);

	data->status = CLOCK_CONTROL_STATUS_ON;
	data->cb(data->cb_dev, data->cb_subsys, data->cb_user_data);
	data->cb = NULL;
	data->cb_dev = NULL;
	data->cb_subsys = NULL;
	data->cb_user_data = NULL;
}

static int drv_clock_ctrl_async_on(const struct device *dev,
				   clock_control_subsys_t sys,
				   clock_control_cb_t cb, void *user_data)
{
	struct emul_clock_ctrl_data *data = dev->data;
	int rc;

	data->status = CLOCK_CONTROL_STATUS_STARTING;
	data->cb = cb;
	data->cb_dev = dev;
	data->cb_subsys = sys;
	data->cb_user_data = user_data;

	k_work_init(&data->async_on_work, clock_ctrl_on_from_work);
	rc = k_work_submit(&data->async_on_work);

	if (rc < 0) {
		/* Return on error */
		return rc;
	}
	/* Treat all others as success */
	return 0;
}

static int drv_clock_ctrl_get_rate(const struct device *dev,
				   clock_control_subsys_t sys, uint32_t *rate)
{
	struct emul_clock_ctrl_data *data = dev->data;

	*rate = data->rate;
	return 0;
}

static enum clock_control_status
drv_clock_ctrl_get_status(const struct device *dev, clock_control_subsys_t sys)
{
	return -EINVAL;
}

static const struct clock_control_driver_api driver_api = {
	.on = drv_clock_ctrl_on,
	.off = drv_clock_ctrl_off,
	.async_on = drv_clock_ctrl_async_on,
	.get_rate = drv_clock_ctrl_get_rate,
	.get_status = drv_clock_ctrl_get_status,
};

static int drv_clock_ctrl_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

#define INIT_CLOCK_CTRL(n)                                                   \
	static struct emul_clock_ctrl_data emul_clock_ctrl_data_##n = {      \
		.is_on = DT_COND_1(DT_INST_PROP(n, clock_frequency) == 0,    \
				   (false), (true)),                         \
		.rate = DT_INST_PROP(n, clock_frequency),                    \
	};                                                                   \
	DEVICE_DT_INST_DEFINE(n, drv_clock_ctrl_init, NULL,                  \
			      &emul_clock_ctrl_data_##n, NULL, PRE_KERNEL_1, \
			      CONFIG_KERNEL_INIT_PRIORITY_OBJECTS,           \
			      &driver_api);

DT_INST_FOREACH_STATUS_OKAY(INIT_CLOCK_CTRL)
