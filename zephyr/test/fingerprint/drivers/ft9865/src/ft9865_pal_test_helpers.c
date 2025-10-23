/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ft9865_pal_test_helpers.h"

int z_impl_ft9865_sensor_hw_reset(void)
{
	return ft_sensor_hw_reset();
}

int z_impl_ft9865_spi_write(uint8_t *buffer, uint32_t len)
{
	return ft_spi_write(buffer, len);
}

int z_impl_ft9865_spi_write_then_read(uint8_t *tx_buffer, uint32_t tx_len,
			   uint8_t *rx_buffer, uint32_t rx_len)
{
	return ft_spi_write_then_read(tx_buffer, tx_len, rx_buffer, rx_len);
}
