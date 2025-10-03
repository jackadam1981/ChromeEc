/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_FT9865_PAL_SENSOR_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_FT9865_PAL_SENSOR_H_

#include <drivers/fingerprint.h>

void ft_delay_ms(uint32_t ms);

int ft_sensor_hw_reset(void);

int ft_spi_write(uint8_t *buffer, uint32_t len);

int ft_spi_write_then_read(uint8_t *tx_buffer, uint32_t tx_len,
			   uint8_t *rx_buffer, uint32_t rx_len);

#endif
