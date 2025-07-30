/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input_kbd_matrix.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(col_gpio_drive, CONFIG_INPUT_LOG_LEVEL);

#define DT_DRV_COMPAT cros_ec_col_gpio

struct col_gpio_config {
	const struct device *kbd_dev;
	struct gpio_dt_spec gpio;
	int col;
	uint32_t settle_time_us;
};

struct col_gpio_data {
	bool state;
};

static void drive_one_col_gpio(const struct device *col_dev,
			       const struct device *kbd_dev, int col)
{
	const struct col_gpio_config *cfg = col_dev->config;
	struct col_gpio_data *data = col_dev->data;
	bool state;

	if (kbd_dev != cfg->kbd_dev) {
		return;
	}

	if (col == INPUT_KBD_MATRIX_COLUMN_DRIVE_ALL || col == cfg->col) {
		gpio_pin_set_dt(&cfg->gpio, 1);
		state = true;
	} else {
		gpio_pin_set_dt(&cfg->gpio, 0);
		state = false;
	}

	if (state != data->state) {
		data->state = state;
		k_busy_wait(cfg->settle_time_us);
	}
}

#define DRIVE_ONE_INSTANCE(inst) \
	drive_one_col_gpio(DEVICE_DT_INST_GET(inst), dev, col);
void input_kbd_matrix_drive_column_hook(const struct device *dev, int col)
{
	DT_INST_FOREACH_STATUS_OKAY(DRIVE_ONE_INSTANCE);
}
#undef DRIVE_ONE_INSTANCE

static int col_gpio_init(const struct device *dev)
{
	const struct col_gpio_config *cfg = dev->config;
	struct col_gpio_data *data = dev->data;
	int ret;

	if (!gpio_is_ready_dt(&cfg->gpio)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&cfg->gpio, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Pin configuration failed: %d", ret);
		return ret;
	}

	data->state = true;

	return 0;
}

#if CONFIG_DT_HAS_ITE_IT8XXX2_KBD_ENABLED
#define ITE_KBD_PARENT_CHECK(inst)                                             \
	BUILD_ASSERT(DT_PROP(DT_PARENT(DT_DRV_INST(inst)), kso_ignore_mask) != \
			     0,                                                \
		     "kso-ignore-mask must be specified on ITE devices for "   \
		     "ec-col-gpio to work correctly")
#else
#define ITE_KBD_PARENT_CHECK(inst)
#endif

#define COL_GPIO_DEVICE_INIT(inst)                                            \
	ITE_KBD_PARENT_CHECK(inst);                                           \
                                                                              \
	static const struct col_gpio_config col_gpio_config_##inst = {        \
		.kbd_dev = DEVICE_DT_GET(DT_PARENT(DT_DRV_INST(inst))),       \
		.gpio = GPIO_DT_SPEC_INST_GET(inst, col_gpios),               \
		.col = DT_INST_PROP(inst, col_num),                           \
		.settle_time_us = DT_INST_PROP(inst, settle_time_us),         \
	};                                                                    \
                                                                              \
	static struct col_gpio_data col_gpio_data_##inst;                     \
                                                                              \
	DEVICE_DT_INST_DEFINE(inst, col_gpio_init, NULL,                      \
			      &col_gpio_data_##inst, &col_gpio_config_##inst, \
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(COL_GPIO_DEVICE_INIT)
