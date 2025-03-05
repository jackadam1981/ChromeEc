// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_EGIS630_PAL_SENSOR_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_EGIS630_PAL_SENSOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/toolchain.h>

/**
 * @brief Issues a SPI transaction. Assumes SPI port has already been enabled.
 *
 * Transmits @p tx_len bytes from @p tx_addr, throwing away the corresponding
 * received data, then transmits @p rx_len bytes, saving the received data in @p
 * rx_buf.
 *
 * @param[in] tx_addr Pointer to the transmit buffer containing the data to be
 * sent.
 * @param[in] tx_len Length of the transmit buffer in bytes.
 * @param[out] rx_buf Pointer to the receive buffer where received data will be
 * stored.
 * @param[in] rx_len Length of the receive buffer in bytes.
 *
 * @return 0 on success.
 * @return A value from @ref ec_error_list enum.
 */
int __unused periphery_spi_write_read(uint8_t *tx_addr, uint32_t tx_len,
				      uint8_t *rx_buf, uint32_t rx_len);

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_EGIS630_PAL_SENSOR_H_ */
