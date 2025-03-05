// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fingerprint_egis630.h"
#include "fingerprint_egis630_pal.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/cbprintf.h>
#include <zephyr/sys_clock.h>

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

unsigned long long __unused plat_get_time(void)
{
	return (k_uptime_get / MSEC_PER_SEC);
}

unsigned long __unused plat_get_diff_time(unsigned long long begin)
{
	unsigned long long nowTime = plat_get_time();

	return (unsigned long)(nowTime - begin);
}

void __unused plat_wait_time(unsigned long msecs)
{
	k_busy_wait(msecs);
	return;
}

void __unused plat_sleep_time(unsigned long timeInMs)
{
	k_usleep(timeInMs * USEC_PER_MSEC);
	return;
}

#ifdef EGIS_DBG
LOG_LEVEL g_log_level = LOG_DEBUG;
#else
LOG_LEVEL g_log_level = LOG_INFO;
#endif

static char printf_buffer[256]; // emflibrary debug buffer

void set_debug_level(LOG_LEVEL level)
{
	g_log_level = level;
	output_log(LOG_ERROR, "RBS", "", "", 0, "set_debug_level %d", level);
}

void __unused output_log(LOG_LEVEL level, const char *tag,
			 const char *file_path, const char *func, int line,
			 const char *format, ...)
{
	if (format == NULL)
		return;
	if (g_log_level > level)
		return;

	va_list vl;
	va_start(vl, format);
	int n = snprintf(printf_buffer, sizeof(printf_buffer), "%s<%s:%d> ",
			 level == LOG_ERROR ? "Error~! " : "", func, line);
	n += vsnprintf(printf_buffer + n, sizeof(printf_buffer) - n, format,
		       vl);
	va_end(vl);

	switch (level) {
	case LOG_ERROR:
	case LOG_INFO:
	case LOG_DEBUG:
	case LOG_VERBOSE:
		LOG_INF("%s", printf_buffer);
		break;
	default:
		break;
	}
}
