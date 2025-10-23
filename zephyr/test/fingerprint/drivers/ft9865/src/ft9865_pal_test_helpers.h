/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_FT9865_SRC_TEST_HELPERS_H_
#define ZEPHYR_TEST_DRIVERS_FT9865_SRC_TEST_HELPERS_H_

#include <zephyr/kernel.h>

#include <fingerprint_ft9865_pal.h>

__syscall int ft9865_sensor_hw_reset(void);
__syscall int ft9865_spi_write(uint8_t *buffer, uint32_t len);
__syscall int ft9865_spi_write_then_read(uint8_t *tx_buffer, uint32_t tx_len,
			   uint8_t *rx_buffer, uint32_t rx_len);

#include <zephyr/syscalls/ft9865_pal_test_helpers.h>

#endif /* ZEPHYR_TEST_DRIVERS_FT9865_SRC_TEST_HELPERS_H_ */
