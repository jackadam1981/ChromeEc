/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fingerprint_egis_ec660.h"
#include "fingerprint_egis_ec660_pal.h"
#include "fingerprint_egis_ec660_private.h"

#include <drivers/cros_flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/fingerprint.h>

/* Platform Abstraction Layer for EGIS binary */

LOG_MODULE_REGISTER(egis_ec660_pal, LOG_LEVEL_INF);

#if !DT_HAS_CHOSEN(cros_fp_fingerprint_sensor)
#error "cros-fp,fingerprint-sensor device must be chosen"
#else
#define fp_sensor_dev DEVICE_DT_GET(DT_CHOSEN(cros_fp_fingerprint_sensor))
#endif

K_HEAP_DEFINE(fp_driver_heap, CONFIG_FINGERPRINT_SENSOR_EGIS_EC660_HEAP_SIZE);

/* it's a workaround for leave_cs_asserted where write_pending occupies a maximum of 1 bytes */
static uint8_t write_pending[8];
static uint8_t write_pending_size = 0;
static uint8_t read_pending_size = 0;
static uint8_t *data_tmp = NULL;

int __unused egis_sensor_spi_write_read(uint8_t *data, size_t write_size,
				        size_t read_size, bool leave_cs_asserted)
{
	int err = 0;
	const struct ec660_cfg *cfg = fp_sensor_dev->config;
	struct spi_buf tx_buf[] = { { .buf = data, .len = write_size },
				    { .buf = NULL, .len = read_size } };
	struct spi_buf rx_buf[] = { { .buf = NULL, .len = write_size },
				    { .buf = data + write_size, .len = read_size } };
	struct spi_buf_set tx = { .buffers = tx_buf, .count = ARRAY_SIZE(tx_buf) };
	struct spi_buf_set rx = { .buffers = rx_buf, .count = ARRAY_SIZE(rx_buf) };

	if (leave_cs_asserted) {
		/* the write size and read size is 1 on this condition */
		memcpy(write_pending + write_pending_size, data, write_size);
		write_pending_size += write_size;
		for (int i = 0; i < read_size; i++) {
			*(data + write_size + i) = 0xff;
		}
		read_pending_size += read_size;
	} else {
		if (write_pending_size) {
			memcpy(data + write_size, write_pending, write_pending_size);
			tx_buf[0].len += write_pending_size;
			rx_buf[0].len += write_pending_size;
		}
		if (read_pending_size) {
			tx_buf[1].len += read_pending_size;
			rx_buf[1].len += read_pending_size;
			data_tmp = egis_malloc(rx_buf[1].len);
			if (data_tmp == NULL) {
				LOG_ERR("fail to malloc %d bytes", rx_buf[1].len);
				return EGIS_BEP_RESULT_NO_MEMORY;
			}
			rx_buf[1].buf = data_tmp;
		}
		fp_sensor_lock(fp_sensor_dev);
		err = spi_transceive_dt(&cfg->spi, &tx, &rx);
		if (read_pending_size) {
			memcpy(data + write_size, data_tmp + read_pending_size, read_size);
			egis_free(data_tmp);
			data_tmp = NULL;
			read_pending_size = 0;
		}
		write_pending_size = 0;
	}

	/*
	 * De-asserting the sensor chip-select will clear the sensor
	 * internal command state. To run multiple sensor transactions
	 * in the same command state (typically image capture), leave
	 * chip-select asserted. Make sure chip-select is de-asserted
	 * when all transactions are finished.
	 */
	if (!leave_cs_asserted) {
		/* Release CS line */
		spi_release_dt(&cfg->spi);
		fp_sensor_unlock(fp_sensor_dev);
	}

	if (err != 0) {
		LOG_ERR("spi_transceive_dt() failed, result %d", err);
		return EGIS_BEP_RESULT_IO_ERROR;
	}

	return EGIS_BEP_RESULT_OK;
}

int __unused egis_sensor_spi_get_duplex_mode(void)
{
	return 0;
}

bool __unused egis_sensor_spi_check_irq(void)
{
	const struct ec660_cfg *cfg = fp_sensor_dev->config;
	int ret = gpio_pin_get_dt(&cfg->interrupt);

	if (ret < 0) {
		LOG_ERR("Failed to get FP interrupt pin, status: %d", ret);
		return false;
	}

	return (ret == 1);
}

bool __unused egis_sensor_spi_read_irq(void)
{
	bool pin = egis_sensor_spi_check_irq();
	return pin;
}

void __unused egis_sensor_spi_reset(bool state)
{
	const struct ec660_cfg *cfg = fp_sensor_dev->config;
	int ret = gpio_pin_set_dt(&cfg->reset_pin, state ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("Failed to set FP reset pin, status: %d", ret);
	}
}

uint32_t __unused egis_timebase_get_tick(void)
{
	return k_uptime_get_32();
}

void __unused egis_timebase_delay_ms(uint32_t delay)
{
	k_msleep(delay);
}

void __unused *egis_malloc(uint32_t size)
{
	void *p = k_heap_aligned_alloc(&fp_driver_heap, sizeof(void *), size,
				       K_NO_WAIT);

	if (p == NULL) {
		LOG_ERR("Failed to allocate %d bytes", size);
		k_oops();
		CODE_UNREACHABLE;
	}

	return p;
}

void __unused egis_free(void *data)
{
	k_heap_free(&fp_driver_heap, data);
}

void __unused egis_log_var(const char *source, uint8_t level, const char *format,
			  ...)
{
	va_list args;

	va_start(args, format);
	vprintk(format, args);
	va_end(args);
}

/* LCOV_EXCL_START - These functions are required by EGIS library.
 * but they are doing nothing.
 */

void __unused egis_assert_fail(const char *file, uint32_t line, const char *func,
			      const char *expr)
{
	/* If need to debug library, implements this */
}

void __unused egis_sensor_spi_init(uint32_t speed_hz)
{
	/* Keep empty because spi is already initialised at other place */
}

int __unused egis_sensor_wfi(uint16_t timeout_ms, egis_wfi_check_t enter_wfi,
			    bool enter_wfi_mode)
{
	/* Always OK */
	return EGIS_BEP_RESULT_OK;
}

static const egis_storage_info_t storage_info =
{
    .read_align         = 1, // min read unit
    .write_align        = 128, // min write unit
    .erase_align        = 4096, // min erase unit
    .is_memory_mapped   = false // default
};

#define INSTANCE_DATA_ADDR 0x800f3000 //DT_REG_ADDR(DT_NODELABEL(instance_data))
#define INSTANCE_DATA_SIZE DT_REG_SIZE(DT_NODELABEL(instance_data))

int __unused egis_instance_data_erase(uint32_t offset, size_t size)
{
    return crec_flash_physical_erase(INSTANCE_DATA_ADDR + offset, size);
}

int __unused egis_instance_data_read(uint32_t offset, size_t size, void *data)
{
    return crec_flash_physical_read(INSTANCE_DATA_ADDR + offset, size, (char *)data);
}

int __unused egis_instance_data_write(uint32_t offset, const void *data, size_t size)
{
    return crec_flash_physical_write(INSTANCE_DATA_ADDR + offset, size, (const char *)data);
}

const egis_storage_info_t __unused *egis_instance_storage_get_info(void)
{
    return &storage_info;
}
/* LCOV_EXCL_STOP */
