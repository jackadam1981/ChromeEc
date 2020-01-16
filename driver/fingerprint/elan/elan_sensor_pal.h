/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* ELAN Platform Abstraction Layer callbacks */

#ifndef ELAN_SENSOR_PAL_H_
#define ELAN_SENSOR_PAL_H_

#define WRITE_ERR	-1
#define SCAN_ERR	-2
static volatile char tx_buf[CONFIG_SPI_TX_BUF_SIZE] __uncached;
static volatile char rx_buf[CONFIG_SPI_RX_BUF_SIZE] __uncached;

/**
 * @brief Write fp command to sensor
 *
 * @param[in] FP_CMD             One byte fp command to write
 *
 * @return 0 on success.
 *         negative value on error.
 */
int elan_write_CMD(uint8_t FP_CMD);

/**
 * @brief Read fp register data from sensor
 *
 * @param[in]	FP_CMD      One byte fp command to read
 * @param[out]	RegData     One byte data where register data will be stored.
 *
 * @return 0 on success.
 *         negative value on error.
 */
int elan_read_CMD(uint8_t FP_CMD, uint8_t *RegData);

/**
 * @brief Tranfers and recives SPI data.
 *
 * @param[in]     tx		The buffer to tranfer.
 * @param[in]	  tx_len	The len to tranfer.
 * @param[out]    rx		The buffer where read data will be stored.
 * @param[in]     rx_len	The len to recive.
 * @return 0 on success.
 *         negative value on error.
 */
int elan_spi_transaction(uint8_t *tx, int tx_len, uint8_t *rx, int rx_len);

/**
 * @brief Write fp register data to sensor
 *
 * @param[in]	RegAddr            One byte register address to write
 * @param[in]	RegData            Data to write to register
 *
 * @return 0 on success.
 *         negative value on error.
 */
int elan_write_register(uint8_t RegAddr, uint8_t RegData);

/**
 * @brief Switch sensor register mode
 *
 * @param[in]   page            The page to switch register mode
 *
 * @return 0 on success.
 *         negative value on error.
 */
int elan_write_Page(uint8_t page);

/**
 * @brief Write register table to fp sensor
 *
 * Using a table to write data to sensor register.
 * This table contains multiple pairs of address and data to
 * be written.
 *
 * @param[in]   reg_table         The register address to write
 * @param[in]   length            The data to write to register
 *
 * @return 0 on success.
 *         negative value on error.
 */
int elan_write_reg_vector(const uint8_t *reg_table, int length);


/**
 * get 14bits raw image data from ELAN fingerprint sensor
 *
 * @param[out] pShortRaw	the memory buffer to receive fingerprint image
 *                          raw data, buffer length is
 *                          (_imageWidth*_imageHeight)*sizeof(unsigned short)
 *
 * @return 0 on success.
 *         negative value on error.
 */
int RawCapture(unsigned short *pShortRaw);

/**
 * Excute calibrate ELAN fingerprint sensor flow.
 *
 * @return 0 on success.
 *         negative value on error.
 */
int ElanFP_ExcuteCalibration(void);
#endif