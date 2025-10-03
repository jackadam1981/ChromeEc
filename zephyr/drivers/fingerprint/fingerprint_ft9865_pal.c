/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fingerprint_ft9865.h"
#include "fingerprint_ft9865_pal.h"
#include "fingerprint_ft9865_private.h"

#include <stdint.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/fingerprint.h>

/* Platform Abstraction Layer for FT binary */

LOG_MODULE_REGISTER(ft9865_pal, LOG_LEVEL_INF);

#if !DT_HAS_CHOSEN(cros_fp_fingerprint_sensor)
#error "cros-fp,fingerprint-sensor device must be chosen"
#else
#define fp_sensor_dev DEVICE_DT_GET(DT_CHOSEN(cros_fp_fingerprint_sensor))
#endif

void ft_delay_ms(uint32_t ms)
{
	k_msleep(ms);
}

int ft_sensor_hw_reset(void)
{
	const struct ft9865_cfg *cfg = fp_sensor_dev->config;

	gpio_pin_set_dt(&cfg->reset_pin, 1);
	k_msleep(5);
	gpio_pin_set_dt(&cfg->reset_pin, 0);
	return 0;
}

int ft_spi_write(uint8_t *buffer, uint32_t len)
{
	int ret = 0;
	const struct ft9865_cfg *cfg = fp_sensor_dev->config;

	struct spi_buf tx_buf[1] = {
		{ .buf = buffer, .len = len },
	};

	struct spi_buf_set tx_set = { .buffers = tx_buf, .count = 1 };

	ret = spi_transceive_dt(&cfg->spi, &tx_set, NULL);

	return ret;
}

int ft_spi_write_then_read(uint8_t *tx_buffer, uint32_t tx_len,
			   uint8_t *rx_buffer, uint32_t rx_len)
{
	int ret;
	uint8_t *tx_buffer_dummy = rx_buffer;
	uint32_t tx_len_demmy = rx_len;
	const struct ft9865_cfg *cfg = fp_sensor_dev->config;

	memset(tx_buffer_dummy, 0, tx_len_demmy);

	struct spi_buf tx_buf[2] = {
		{ .buf = tx_buffer, .len = tx_len },
		{ .buf = tx_buffer_dummy, .len = tx_len_demmy },
	};

	uint8_t rx_buffer_temp[tx_len];
	struct spi_buf rx_buf[2] = {
		{ .buf = rx_buffer_temp, .len = tx_len },
		{ .buf = rx_buffer, .len = rx_len },
	};

	struct spi_buf_set tx_set = { .buffers = tx_buf, .count = 2 };
	struct spi_buf_set rx_set = { .buffers = rx_buf, .count = 2 };

	ret = spi_transceive_dt(&cfg->spi, &tx_set, &rx_set);

	return ret;
}
