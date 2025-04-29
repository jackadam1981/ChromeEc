/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "emul/emul_counter.h"

#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT cros_counter_emul

LOG_MODULE_REGISTER(counter_emul, CONFIG_COUNTER_EMUL_LOG_LEVEL);

static uint32_t top_value_reg;
static uint32_t alarm_value_reg;
static struct emul_counter_ctrl counter_emul_ctrl_reg;

struct emul_counter_ctrl emul_get_counter_ctrl_reg(void)
{
	return counter_emul_ctrl_reg;
}

struct emul_counter_config {
	struct counter_config_info info;
};

/** Data needed to maintain the current emulator state */
struct emul_counter_data {
	counter_top_callback_t top_callback;
	void *top_user_data;
	counter_alarm_callback_t alarm_callback;
	void *alarm_user_data;
};

static int emul_counter_start(const struct device *dev)
{
	ARG_UNUSED(dev);
	/* write ENABLE bit to CTRL register */
	counter_emul_ctrl_reg.top_tmr_start = true;
	LOG_INF("emul top tmr start");
	return 0;
}

static int emul_counter_stop(const struct device *dev)
{
	ARG_UNUSED(dev);
	/* clear ENABLE bit to CTRL register */
	counter_emul_ctrl_reg.top_tmr_start = false;
	return 0;
}

static int emul_counter_get_value(const struct device *dev, uint32_t *ticks)
{
	*ticks = alarm_value_reg;

	return 0;
}

static int emul_counter_set_alarm(const struct device *dev, uint8_t chan_id,
				  const struct counter_alarm_cfg *alarm_cfg)
{
	struct emul_counter_data *data = dev->data;

	/* set alarm value */
	alarm_value_reg = alarm_cfg->ticks;

	data->alarm_callback = alarm_cfg->callback;
	data->alarm_user_data = alarm_cfg->user_data;

	/* write ENABLE bit to CTRL register */
	counter_emul_ctrl_reg.alarm_tmr_start = true;

	return 0;
}

static int emul_counter_cancel_alarm(const struct device *dev, uint8_t chan_id)
{
	struct emul_counter_data *data = dev->data;

	/* clear ENABLE bit to CTRL register */
	counter_emul_ctrl_reg.alarm_tmr_start = false;

	data->alarm_callback = NULL;
	data->alarm_user_data = NULL;

	return 0;
}

static int emul_counter_set_top_value(const struct device *dev,
				      const struct counter_top_cfg *top_cfg)
{
	struct emul_counter_data *data = dev->data;

	data->top_callback = top_cfg->callback;
	data->top_user_data = top_cfg->user_data;
	/* set new top_value */
	top_value_reg = top_cfg->ticks;
	/* write ENABLE bit to CTRL register */
	counter_emul_ctrl_reg.top_tmr_start = true;
	LOG_INF("start top tmr");
	return 0;
}

static uint32_t emul_counter_get_top_value(const struct device *dev)
{
	ARG_UNUSED(dev);
	return top_value_reg;
}

static DEVICE_API(counter, emul_counter_driver_api) = {
	.start = emul_counter_start,
	.stop = emul_counter_stop,
	.get_value = emul_counter_get_value,
	.set_alarm = emul_counter_set_alarm,
	.cancel_alarm = emul_counter_cancel_alarm,
	.set_top_value = emul_counter_set_top_value,
	.get_top_value = emul_counter_get_top_value,
};

static int drv_counter_emul_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	counter_emul_ctrl_reg.top_tmr_start = false;
	LOG_INF("top tmr is start at init");
	counter_emul_ctrl_reg.alarm_tmr_start = false;
	return 0;
}

#define COUNTER_EMUL_INIT(n)                                                  \
	static struct emul_counter_config counter_emul_config_##n = {       \
		.info =                                                     \
			{                                                   \
				.max_top_value = UINT32_MAX,                \
				.freq = 32768,                              \
				.flags = 0,                                 \
				.channels = 1,                              \
			},                                                  \
	}; \
	static struct emul_counter_data counter_emul_data_##n;                \
                                                                              \
	DEVICE_DT_INST_DEFINE(n, &drv_counter_emul_init, NULL,                \
			      &counter_emul_data_##n,                         \
			      &counter_emul_config_##n, POST_KERNEL,          \
			      CONFIG_KERNEL_INIT_PRIORITY_OBJECTS,            \
			      &emul_counter_driver_api);

DT_INST_FOREACH_STATUS_OKAY(COUNTER_EMUL_INIT)
