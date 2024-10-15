/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_FINGERPRINT_EGIS_PLATFORM_INC_PLAT_SPI_H_
#define __CROS_EC_DRIVER_FINGERPRINT_EGIS_PLATFORM_INC_PLAT_SPI_H_

#include "spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Issue a SPI transaction.  Assumes SPI port has already been enabled.
 *
 * Transmits <txlen> bytes from <txdata>, throwing away the corresponding
 * received data, then transmits <rxlen> bytes, saving the received data in
 * <rxdata>.
 *
 * @param tx_addr A pointer to the transmit buffer containing the data to be
 * sent.
 * @param tx_len The length of the transmit buffer in bytes.
 * @param rx_buf A pointer to the receive buffer where the received data will be
 * stored.
 * @param rx_len The length of the receive buffer in bytes.
 *
 * @return 0 on success.
 * @return negative value on error.
 */
int periphery_spi_write_read(uint8_t *tx_addr, uint32_t tx_len, uint8_t *rx_buf,
			     uint32_t rx_len);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_DRIVER_FINGERPRINT_EGIS_PLATFORM_INC_PLAT_SPI_H_ */
