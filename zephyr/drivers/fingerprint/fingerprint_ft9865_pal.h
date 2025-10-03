/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_FT9865_PAL_SENSOR_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_FT9865_PAL_SENSOR_H_

#include <drivers/fingerprint.h>

/**
 * delay function, unit in ms.
 *
 * @param[in] the time need to delay, unit in ms
 */
void ft_delay_ms(uint32_t ms);

/**
 * hardware reset the fp sensor by active and deactivate the fp's reset pin
 *
 * @return 0 on success.
 *         negative value on error.
 */
int ft_sensor_hw_reset(void);

/**
 * @brief spi write function for fp sensor
 *
 * write data to fp sensor by spi
 *
 * @param[in]    buffer       the data need to write
 * @param[in]    len          data length in bytes
 *
 * @return 0 on success.
 *         negative value on error.
 */
int ft_spi_write(uint8_t *buffer, uint32_t len);

/**
 * @brief spi write read function for fp sensor
 *
 * write data to fp sensor by spi, and then read data.
 *
 * @param[in]    tx_buffer       the data need to write
 * @param[in]    tx_len          write data length in bytes
 * @param[out]    rx_buffer      the received data
 * @param[out]    rx_len         received data length in bytes
 *
 * @return 0 on success.
 *         negative value on error.
 */
int ft_spi_write_then_read(uint8_t *tx_buffer, uint32_t tx_len,
			   uint8_t *rx_buffer, uint32_t rx_len);

#endif
