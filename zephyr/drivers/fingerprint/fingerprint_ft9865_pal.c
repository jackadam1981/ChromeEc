/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fingerprint_ft9865_pal.h"

#include <stdint.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define SENSOR_RST_PIN 2
#define SENSOR_INT_PIN 0

static const struct device *spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi2));
static struct spi_config config;

static const struct device *gpio_dev5 = DEVICE_DT_GET(DT_NODELABEL(eport5));

LOG_MODULE_REGISTER(ft9865_pal, LOG_LEVEL_INF);

void ft_delay_ms(uint32_t ms)
{
	k_msleep(ms);
}

void ft_gpio_init(void)
{
	// gpio_dev5 = DEVICE_DT_GET(DT_NODELABEL(eport5));
	if (device_is_ready(gpio_dev5)) {
		printf("eport5 device initialized!\n");
	}

	gpio_pin_configure(gpio_dev5, SENSOR_RST_PIN, GPIO_OUTPUT); // GINT42
								    // OUTPUT
	gpio_pin_configure(gpio_dev5, SENSOR_INT_PIN,
			   GPIO_INPUT | GPIO_PULL_UP); // GINT40 input

	/* Configure GPIO interrupts */
	gpio_pin_interrupt_configure(gpio_dev5, SENSOR_INT_PIN,
				     GPIO_INT_MODE_EDGE |
					     GPIO_INT_TRIG_HIGH); // GINT40

	gpio_pin_set_raw(gpio_dev5, SENSOR_RST_PIN, 1);
}

int ft_sensor_hw_reset(void)
{
	gpio_pin_set_raw(gpio_dev5, SENSOR_RST_PIN, 0);
	k_msleep(5);
	gpio_pin_set_raw(gpio_dev5, SENSOR_RST_PIN, 1);
	return 0;
}

void ft_spi_init(void)
{
	if (!device_is_ready(spi_dev)) {
		printf("SPI device %s is not ready\n", spi_dev->name);
		return;
	} else {
		printf("SPI device %s is ready\n", spi_dev->name);
	}

	config.frequency = 6 * 1000 * 1000U;
	config.operation = SPI_TRANSFER_MSB | SPI_WORD_SET(8) |
			   SPI_OP_MODE_MASTER;
}

int ft_spi_write(uint8_t *buffer, uint32_t len)
{
	int ret;

	struct spi_buf tx_buf[1] = {
		{ .buf = buffer, .len = len },
	};

	struct spi_buf_set tx_set = { .buffers = tx_buf, .count = 1 };

	ret = spi_transceive(spi_dev, &config, &tx_set, NULL);

	return ret;
}

int ft_spi_write_then_read(uint8_t *tx_buffer, uint32_t tx_len,
			   uint8_t *rx_buffer, uint32_t rx_len)
{
	int ret;
	uint8_t *tx_buffer_dummy = rx_buffer;
	uint32_t tx_len_demmy = rx_len;

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

	ret = spi_transceive(spi_dev, &config, &tx_set, &rx_set);

	return ret;
}
