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
#define COL_GPIO_NODE DT_DRV_INST(0)
#define NUM_COLS DT_INST_PROP_LEN(0, col_gpios)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
	     "only one cros-ec,col-gpio compatible node can be supported");

BUILD_ASSERT(NUM_COLS > 0, "col-gpios must not be empty");
BUILD_ASSERT(DT_INST_PROP_LEN(0, col_num) == NUM_COLS,
	     "col-num and col-gpios must have the same length");
BUILD_ASSERT(DT_INST_PROP_LEN(0, settle_time_us) == NUM_COLS,
	     "settle-time-us and col-gpios must have the same length");

#if CONFIG_DT_HAS_ITE_IT8XXX2_KBD_ENABLED
BUILD_ASSERT(DT_PROP(DT_PARENT(COL_GPIO_NODE), kso_ignore_mask) != 0,
	     "kso-ignore-mask must be specified on ITE devices for "
	     "ec-col-gpio to work correctly");
#endif

struct col_gpio_config {
	const struct device *kbd_dev;
	int num_gpios;
	const struct gpio_dt_spec *gpios;
	const int *cols;
	const uint32_t *settle_times_us;
};

struct col_gpio_data {
	bool states[NUM_COLS];
};

/* Statically define the configuration arrays from the devicetree */
static const struct gpio_dt_spec col_gpios[] = { DT_INST_FOREACH_PROP_ELEM_SEP(
	0, col_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, )) };
static const int col_nums[] = DT_INST_PROP(0, col_num);
static const uint32_t settle_times_us[] = DT_INST_PROP(0, settle_time_us);

static const struct col_gpio_config col_gpio_cfg_0 = {
	.kbd_dev = DEVICE_DT_GET(DT_PARENT(COL_GPIO_NODE)),
	.num_gpios = NUM_COLS,
	.gpios = col_gpios,
	.cols = col_nums,
	.settle_times_us = settle_times_us,
};

static struct col_gpio_data col_gpio_data_0;

void input_kbd_matrix_drive_column_hook(const struct device *dev, int col)
{
	const struct col_gpio_config *cfg = &col_gpio_cfg_0;
	struct col_gpio_data *data = &col_gpio_data_0;

	if (dev != cfg->kbd_dev) {
		return;
	}

	for (int i = 0; i < cfg->num_gpios; i++) {
		bool new_state;

		if (col == INPUT_KBD_MATRIX_COLUMN_DRIVE_ALL ||
		    col == cfg->cols[i]) {
			gpio_pin_set_dt(&cfg->gpios[i], 1);
			new_state = true;
		} else {
			gpio_pin_set_dt(&cfg->gpios[i], 0);
			new_state = false;
		}

		if (new_state != data->states[i]) {
			data->states[i] = new_state;
			/* Only wait for settle time if it's non-zero. */
			if (cfg->settle_times_us[i] > 0) {
				k_busy_wait(cfg->settle_times_us[i]);
			}
		}
	}
}

static int col_gpio_init(const struct device *dev)
{
	const struct col_gpio_config *cfg = dev->config;
	struct col_gpio_data *data = dev->data;

	for (int i = 0; i < cfg->num_gpios; i++) {
		if (!gpio_is_ready_dt(&cfg->gpios[i])) {
			LOG_ERR("GPIO controller for col %d not ready",
				cfg->cols[i]);
			return -ENODEV;
		}

		int ret = gpio_pin_configure_dt(&cfg->gpios[i],
						GPIO_OUTPUT_ACTIVE);
		if (ret != 0) {
			LOG_ERR("Pin configuration for col %d failed: %d",
				cfg->cols[i], ret);
			return ret;
		}
		data->states[i] = true;
	}

	return 0;
}
DEVICE_DT_DEFINE(COL_GPIO_NODE, col_gpio_init, NULL, &col_gpio_data_0,
		 &col_gpio_cfg_0, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY,
		 NULL);
