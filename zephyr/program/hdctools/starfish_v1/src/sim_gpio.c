/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "sim_gpio.h"

#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>

LOG_MODULE_DECLARE(sim);

/*
START_ADC

static const struct adc_dt_spec adc_spec =
    ADC_DT_SPEC_GET(SIM_ADC);
static const struct adc_dt_spec adc_spec =
    { .dev = (&__device_dts_ord_SIM_ADC_P_io_channels_IDX_0_PH_ORD), .channel_id = SIM_ADC_P_io_channels_IDX_0_VAL_input, };

ADC_DT_SPEC_GET(DT_N_S_soc_S_adc_50000000_S_channel_0)
    { .dev = (&__device_dts_ord_DT_N_S_soc_S_adc_50000000_S_channel_0_P_io_channels_IDX_0_PH_ORD), .channel_id = DT_N_S_soc_S_adc_50000000_S_channel_0_P_io_channels_IDX_0_VAL_input, };

(&__device_dts_ord_DT_N_S_zephyr_simadc_P_io_channels_IDX_0_PH_ORD),
.channel_id = DT_N_S_zephyr_simadc_P_io_channels_IDX_0_VAL_input, };
*/

static const struct adc_dt_spec adc_spec = ADC_DT_SPEC_GET(DT_PATH(simadc));

int read_adc() {
	int err = 0;
	uint16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	err = adc_channel_setup_dt(&adc_spec);
	if (err) {
		LOG_ERR("adc_channel_setup_dt() failed %d", err);
		return -1;
	}

	err = adc_sequence_init_dt(&adc_spec, &sequence);
	if (err) {
		LOG_ERR("adc_sequence_init_dt() failed %d", err);
		return -1;
	}

	err = adc_read(adc_spec.dev, &sequence);
	if (err) {
		LOG_ERR("adc_read() failed %d", err);
		return -1;
	}
	int32_t val_mv = buf;
	err = adc_raw_to_millivolts_dt(&adc_spec, &val_mv);
	if (err) {
		LOG_ERR("adc_raw_to_millivolts_dt() failed %d", err);
		return -1;
	}
	if (val_mv < 0) {
		LOG_ERR("val_mv is negative %d", val_mv);
		return -1;
	}
	LOG_ERR("val_mv %d", val_mv);
	return val_mv;
}

struct sim_ctx {
	/* GPIO Context */
	struct gpio_ctx gpio;
};

static struct sim_ctx sim_gpios[] = { GPIO_LIST_CTX(sim) };

static struct gpio_ctx *find_gpio(enum GPIO_LABEL label, int idx)
{
	for (int i = 0; i < ARRAY_SIZE(sim_gpios); i++) {
		if (sim_gpios[i].gpio.label != label) {
			continue;
		}
		if (sim_gpios[i].gpio.idx != idx) {
			continue;
		}
		return &sim_gpios[i].gpio;
	}
	return NULL;
}

int read_gpio(enum GPIO_LABEL label, int idx)
{
	struct gpio_ctx *gpio = find_gpio(label, idx);
	if (gpio) {
		return gpio_pin_get_dt(&gpio->spec);
	}
	return 0;
}

int write_gpio(enum GPIO_LABEL label, int idx, bool state)
{
	struct gpio_ctx *gpio = find_gpio(label, idx);
	if (gpio) {
		gpio_pin_set_dt(&gpio->spec, state);
	}
	return 0;
}

void sim_gpio_init()
{
	for (int i = 0; i < ARRAY_SIZE(sim_gpios); i++) {
		gpio_flags_t flags = GPIO_OUTPUT;
		if (sim_gpios[i].gpio.label == GPIO_LABEL_SIM_CD) {
			flags = GPIO_INPUT;
		}
		gpio_pin_configure_dt(&sim_gpios[i].gpio.spec, flags);
	}

	// Set VSIM to 1.8v
	write_gpio(GPIO_LABEL_SIM_VCC_SEL, 0, 0);
	// Enable SIM host
	write_gpio(GPIO_LABEL_SIM_HOST_EN, 0, 1);
}

