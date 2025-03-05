// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fingerprint_egis630.h"
#include "fingerprint_egis630_pal.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/fingerprint.h>

/* Platform Abstraction Layer for FPC binary */

LOG_MODULE_REGISTER(egis630_pal, LOG_LEVEL_INF);

#if !DT_HAS_CHOSEN(cros_fp_fingerprint_sensor)
#error "cros-fp,fingerprint-sensor device must be chosen"
#else
#define fp_sensor_dev DEVICE_DT_GET(DT_CHOSEN(cros_fp_fingerprint_sensor))
#endif

#define LOG_TAG "PLAT-SPI"

int __unused periphery_spi_write_read(uint8_t *tx_buf, uint32_t tx_len,
				      uint8_t *rx_buf, uint32_t rx_len)
{
	const struct egis_cfg *cfg = fp_sensor_dev->config;
	const struct spi_buf tx_buf[1] = { { .buf = write, .len = size } };
	const struct spi_buf rx_buf[1] = { { .buf = read, .len = size } };
	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 1 };
	const struct spi_buf_set rx = { .buffers = rx_buf, .count = 1 };

	/* Block communicating with sensor by other threads while a series of
	 * SPI transaction is ongoing, until CS is asserted,
	 */
	fp_sensor_lock(fp_sensor_dev);
	int err = spi_transceive_dt(&cfg->spi, &tx, &rx);

	if (err != 0) {
		LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
			__func__, err);
		return FPC_BEP_RESULT_IO_ERROR;
	}

	return err;
}
