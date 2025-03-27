/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fingerprint_elan80sg.h"
#include "fingerprint_elan80sg_pal.h"
#include "fingerprint_elan80sg_private.h"

#include <stdio.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/cbprintf.h>

#include <drivers/fingerprint.h>

/* Platform Abstraction Layer for FPC libfp binary */

LOG_MODULE_REGISTER(elan80sg_pal, LOG_LEVEL_INF);

#if !DT_HAS_CHOSEN(cros_fp_fingerprint_sensor)
#error "cros-fp,fingerprint-sensor device must be chosen"
#else
#define fp_sensor_dev DEVICE_DT_GET(DT_CHOSEN(cros_fp_fingerprint_sensor))
#endif

K_HEAP_DEFINE(fp_driver_heap, CONFIG_FINGERPRINT_SENSOR_ELAN80SG_HEAP_SIZE);

static uint8_t tx_buf[ELAN_SPI_TX_BUF_SIZE]; //__uncached;
static uint8_t rx_buf[ELAN_SPI_RX_BUF_SIZE]; // __uncached;

static int elan_spi_transaction_fullplex(uint8_t *tx_buf, uint8_t *rx_buf,
					 size_t trx_len)
{
	const struct elan80sg_cfg *cfg = fp_sensor_dev->config;

	const struct spi_buf write_buf[1] = { { .buf = tx_buf,
						.len = trx_len } };
	const struct spi_buf read_buf[1] = { { .buf = rx_buf,
					       .len = trx_len } };
	const struct spi_buf_set tx = { .buffers = write_buf, .count = 1 };
	const struct spi_buf_set rx = { .buffers = read_buf, .count = 1 };

	int err = spi_transceive_dt(&cfg->spi, &tx, &rx);

	return err;
}

static int elan_spi_transaction_duplex(uint8_t *tx_buf, size_t tx_len,
				       uint8_t *rx_buf, size_t rx_len)
{
	const struct elan80sg_cfg *cfg = fp_sensor_dev->config;

	const struct spi_buf write_buf[] = { { .buf = tx_buf, .len = tx_len },
					     { .buf = NULL, .len = rx_len } };
	const struct spi_buf read_buf[] = { { .buf = NULL, .len = tx_len },
					    { .buf = rx_buf, .len = rx_len } };
	const struct spi_buf_set tx = { .buffers = write_buf,
					.count = ARRAY_SIZE(write_buf) };
	const struct spi_buf_set rx = { .buffers = read_buf,
					.count = ARRAY_SIZE(read_buf) };

	int err = spi_transceive_dt(&cfg->spi, &tx, &rx);

	return err;
}

int __unused elan_write_cmd(uint8_t fp_cmd)
{
	memset(tx_buf, 0, ELAN_SPI_TX_BUF_SIZE);
	memset(rx_buf, 0, ELAN_SPI_RX_BUF_SIZE);

	tx_buf[0] = fp_cmd;

	int err = elan_spi_transaction_fullplex(tx_buf, rx_buf,
						ELAN_SPI_TX_BUF_SIZE);

	if (err != 0) {
		LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
			__func__, err);
		return -EIO;
	}

	return err;
}

int __unused elan_read_cmd(uint8_t fp_cmd, uint8_t *regdata)
{
	memset(tx_buf, 0, ELAN_SPI_TX_BUF_SIZE);
	memset(rx_buf, 0, ELAN_SPI_RX_BUF_SIZE);

	tx_buf[0] = fp_cmd; /* one byte data read */

	int err = elan_spi_transaction_fullplex(tx_buf, rx_buf,
						ELAN_SPI_TX_BUF_SIZE);

	if (err != 0) {
		LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
			__func__, err);
		return -EIO;
	}

	*regdata = rx_buf[1];

	return err;
}

int __unused elan_spi_transaction(uint8_t *tx_data, int tx_len,
				  uint8_t *rx_data, int rx_len)
{
	memset(tx_buf, 0, ELAN_SPI_TX_BUF_SIZE);
	memset(rx_buf, 0, ELAN_SPI_RX_BUF_SIZE);

	memcpy(tx_buf, tx_data, tx_len);

	int err = elan_spi_transaction_duplex(tx_buf, tx_len, rx_buf, rx_len);

	if (err != 0) {
		LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
			__func__, err);
		return -EIO;
	}

	memcpy(rx_data, rx_buf, rx_len);

	return err;
}

int __unused elan_write_register(uint8_t regaddr, uint8_t regdata)
{
	memset(tx_buf, 0, ELAN_SPI_TX_BUF_SIZE);
	memset(rx_buf, 0, ELAN_SPI_RX_BUF_SIZE);

	tx_buf[0] = WRITE_REG_HEAD + regaddr; /* one byte data write */
	tx_buf[1] = regdata;

	int err = elan_spi_transaction_fullplex(tx_buf, rx_buf,
						ELAN_SPI_TX_BUF_SIZE);

	if (err != 0) {
		LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
			__func__, err);
		return -EIO;
	}

	return err;
}

int __unused elan_read_register(uint8_t regaddr, uint8_t *regdata)
{
	return elan_read_cmd(READ_REG_HEAD + regaddr, regdata);
}

int __unused elan_write_page(uint8_t page)
{
	memset(tx_buf, 0, ELAN_SPI_TX_BUF_SIZE);
	memset(rx_buf, 0, ELAN_SPI_RX_BUF_SIZE);

	tx_buf[0] = PAGE_SEL;
	tx_buf[1] = page;

	int err = elan_spi_transaction_fullplex(tx_buf, rx_buf,
						ELAN_SPI_TX_BUF_SIZE);

	if (err != 0) {
		LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
			__func__, err);
		return -EIO;
	}

	return err;
}

int __unused elan_write_reg_vector(const uint8_t *reg_table, int length)
{
	int ret = 0;
	int i = 0;
	uint8_t write_regaddr;
	uint8_t write_regdata;

	for (i = 0; i < length; i = i + 2) {
		write_regaddr = reg_table[i];
		write_regdata = reg_table[i + 1];
		ret = elan_write_register(write_regaddr, write_regdata);
		if (ret < 0)
			break;
	}
	return ret;
}

int __unused elan_raw_capture(uint16_t *short_raw)
{
	int ret = 0, i = 0, cnt_timer = 0, rx_index = 0;
	uint8_t regdata[4] = { 0 };

	memset(short_raw, 0, sizeof(uint16_t) * IMAGE_TOTAL_PIXEL);

	/* Write start scans command to fp sensor */
	if (elan_write_cmd(START_SCAN) < 0) {
		ret = ELAN_ERROR_SPI;
		LOGE_SA("%s SPISendCommand( SSP2, START_SCAN ) fail ret = %d",
			__func__, ret);
		goto exit;
	}
	/* Polling scan status */
	cnt_timer = 0;
	while (1) {
		k_usleep(1000);
		cnt_timer++;
		regdata[0] = SENSOR_STATUS;
		elan_spi_transaction(regdata, 2, regdata, 2);
		if (regdata[0] & 0x04)
			break;

		if (cnt_timer > POLLING_SCAN_TIMER) {
			ret = ELAN_ERROR_SCAN;
			LOGE_SA("%s regdata = 0x%x, fail ret = %d", __func__,
				regdata[0], ret);
			goto exit;
		}
	}

	/* Read the image from fp sensor */
	for (i = 0; i < ELAN_DMA_LOOP; i++) {
		memset(tx_buf, 0, ELAN_SPI_TX_BUF_SIZE);
		memset(rx_buf, 0, ELAN_SPI_RX_BUF_SIZE);
		tx_buf[0] = START_READ_IMAGE;

		ret = elan_spi_transaction_duplex(tx_buf, ELAN_SPI_TX_BUF_SIZE,
						  rx_buf, ELAN_SPI_RX_BUF_SIZE);

		if (ret != 0) {
			LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
				__func__, ret);
			return -EIO;
		}

		for (int y = 0; y < IMAGE_HEIGHT / ELAN_DMA_LOOP; y++) {
			for (int x = 0; x < IMAGE_WIDTH; x++) {
				rx_index = (x * 2) + (RAW_DATA_SIZE * y);
				short_raw[(x + y * IMAGE_WIDTH) +
					  i * ELAN_DMA_SIZE] =
					(rx_buf[rx_index] << 8) +
					(rx_buf[rx_index + 1]);
			}
		}
	}

exit:

	if (ret != 0)
		LOGE_SA("%s error = %d", __func__, ret);
	return ret;
}

int __unused elan_execute_calibration(void)
{
	int retry_time = 0;
	int ret = 0;

	while (retry_time < REK_TIMES) {
		elan_write_cmd(SRST);
		elan_write_cmd(FUSE_LOAD);
		elan_register_initialization();
		elan_set_hv_chip(false);
		elan_sensing_mode();

		ret = elan_calibration();
		if (ret == 0)
			break;

		retry_time++;
	}

	return ret;
}

int __unused elan_fp_maintenance(uint16_t *error_state)
{
	int rv;
	fp_sensor_info_t sensor_info;
	uint32_t start = k_ticks_to_ms_near32(k_uptime_ticks());
	uint32_t end;

	if (error_state == NULL)
		return -EINVAL;

	/* Initial status */
	*error_state &= 0xFC00;
	sensor_info.num_defective_pixels = 0;
	sensor_info.sensor_error_code = 0;
	rv = elan_fp_sensor_maintenance(&sensor_info);
	end = k_ticks_to_ms_near32(k_uptime_ticks());
	LOGE_SA("Maintenance took %d ms", end - start);

	if (rv != 0) {
		/*
		 * Failure can occur if any of the fingerprint detection zones
		 * are covered (i.e., finger is on sensor).
		 */
		LOGE_SA("Failed to run maintenance: %d", rv);
		return -ENOTSUP;
	}
	/*
	 * Reset the number of dead pixels before any update.
	 */
	*error_state &= ~FINGERPRINT_ERROR_DEAD_PIXELS_MASK;
	*error_state |= FINGERPRINT_ERROR_DEAD_PIXELS(
		MIN(sensor_info.num_defective_pixels,
		    FINGERPRINT_ERROR_DEAD_PIXELS_MAX));
	LOGE_SA("num_defective_pixels: %d", sensor_info.num_defective_pixels);
	LOGE_SA("sensor_error_code: %d", sensor_info.sensor_error_code);

	return 0;
}

int __unused elan_set_hv_chip(bool state)
{
	int ret = 0;

	memset(tx_buf, 0, ELAN_SPI_TX_BUF_SIZE);
	memset(rx_buf, 0, ELAN_SPI_RX_BUF_SIZE);

	if (state) {
		elan_write_cmd(FUSE_LOAD);
		k_usleep(1000);

		tx_buf[0] = 0x0B;
		tx_buf[1] = 0x02;

		ret = elan_spi_transaction_duplex(tx_buf, ELAN_SPI_TX_BUF_SIZE,
						  rx_buf, ELAN_SPI_TX_BUF_SIZE);

		if (ret != 0) {
			LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
				__func__, ret);
			return -EIO;
		}

		k_usleep(1000);
	} else {
		tx_buf[0] = 0x0B;
		tx_buf[1] = 0x00;

		ret |= elan_spi_transaction_duplex(tx_buf, ELAN_SPI_TX_BUF_SIZE,
						   rx_buf,
						   ELAN_SPI_TX_BUF_SIZE);

		if (ret != 0) {
			LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
				__func__, ret);
			return -EIO;
		}

		k_usleep(1000);

		const uint8_t charge_pump[] = { 0x00,
						(uint8_t)CHARGE_PUMP_HVIC };

		elan_write_reg_vector(charge_pump, ((int)sizeof(charge_pump)));

		const uint8_t disable_hv[] = { 0x01, VOLTAGE_HVIC };

		elan_write_reg_vector(disable_hv, ((int)sizeof(disable_hv)));

		tx_buf[0] = 0x0B;
		tx_buf[1] = 0x02;

		ret |= elan_spi_transaction_duplex(tx_buf, ELAN_SPI_TX_BUF_SIZE,
						   rx_buf,
						   ELAN_SPI_TX_BUF_SIZE);

		if (ret != 0) {
			LOG_ERR("spi_write FAILED: in func: %s with retval = %d\n",
				__func__, ret);
			return -EIO;
		}
		k_usleep(1000);
	}
	return ret;
}

int __unused elan_usleep(unsigned int us)
{
	return k_usleep(us);
}

void *__unused elan_malloc(uint32_t size)
{
	void *p = k_heap_aligned_alloc(&fp_driver_heap, sizeof(void *), size,
				       K_NO_WAIT);

	if (p == NULL) {
		LOG_ERR("Error - %s of size %u failed.", __func__, size);
		k_oops();
		CODE_UNREACHABLE;
	}

	return p;
}

void __unused elan_free(void *data)
{
	k_heap_free(&fp_driver_heap, data);
}

char printf_buffer[256];

void __unused elan_log_var(const char *format, ...)
{
	va_list vl;
	va_start(vl, format);
	vsnprintf(printf_buffer, sizeof(printf_buffer), format, vl);
	va_end(vl);

	// Remove newline character at the end of printf_buffer because LOG_INF
	// will add it.
	size_t len = strlen(printf_buffer);
	if (len > 0 && printf_buffer[len - 1] == '\n') {
		printf_buffer[len - 1] = '\0';
	}

	LOG_INF("%s", printf_buffer);
}

uint32_t __unused elan_get_tick(void)
{
	return k_ticks_to_ms_near32(k_uptime_ticks());
}

void __unused elan_sensor_set_rst(bool state)
{
	const struct elan80sg_cfg *cfg = fp_sensor_dev->config;
	int ret = gpio_pin_set_dt(&cfg->reset_pin, state ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("Failed to set FP reset pin, status: %d", ret);
	}
}
