/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_ELAN80SG_PAL_SENSOR_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_ELAN80SG_PAL_SENSOR_H_

#include "fingerprint_elan80sg_private.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/toolchain.h>

/* The hardware ID information and FW version */
#define PID 0x0903 /* USB product ID */
#define MID 0x01 /* Elan doesn't track model, so this has no meaning. */
#define VERSION 0x100B /* Elan internal firmware version */

/**
 * Elan sensor operation is controlled by sending commands and receiving
 * through the SPI interface. There are several SPI command codes for
 * controlling FP sensor:
 *
 * - START_SCAN            Start scan
 * - START_READ_IMAGE      Start read the image
 * - SRST                  Software reset
 * - FUSE_LOAD             Load OTP trims data to control registers
 * - READ_REG_HEAD         Register single read
 * - WRITE_REG_HEAD        Register burst write
 * - READ_SERIER_REG_HEAD  Register burst read
 * - PAGE_SEL              Register page selection
 * - SENSOR_STATUS         Read sensor status
 * - HWID_LO               Hardware ID low register
 * - HWID_HI               Hardware ID high register
 * - IMG_READY             Image ready register
 * - HV_ENABLE             High voltage enable register
 * - HV_CONTROL            High voltage control Register
 */
#define START_SCAN 0x01
#define START_READ_IMAGE 0x10
#define SRST 0x31
#define FUSE_LOAD 0x04
#define READ_REG_HEAD 0x40
#define WRITE_REG_HEAD 0x80
#define READ_SERIER_REG_HEAD 0xC0
#define PAGE_SEL 0x07
#define SENSOR_STATUS 0x03
#define HWID_LO 0x04
#define HWID_HI 0x02
#define IMG_READY 0x04
#define HV_ENABLE 0x01
#define HV_CONTROL 0x00

/* Sensor type name */
#define EFSA80SG 3

/* Sensor pixel resolution */
#define IMAGE_WIDTH 80
#define IMAGE_HEIGHT 80

/**
 * Sensor real image size:
 * ((IMAGE_HEIGHT * ONE_PIXEL_BYTE) + FP_DUMMY_BYTE) * IMAGE_WIDTH
 */
#define FP_DUMMY_BYTE 2
#define ONE_PIXEL_BYTE 2
#define IMAGE_TOTAL_PIXEL (IMAGE_WIDTH * IMAGE_HEIGHT)
#define RAW_PIXEL_SIZE (IMAGE_WIDTH * ONE_PIXEL_BYTE)
#define RAW_DATA_SIZE (RAW_PIXEL_SIZE + FP_DUMMY_BYTE)
#define IMG_BUF_SIZE (RAW_DATA_SIZE * IMAGE_HEIGHT)

/* SPI tx and rx buffer size */
#define ELAN_DMA_LOOP 4
#define ELAN_DMA_SIZE (IMAGE_TOTAL_PIXEL / ELAN_DMA_LOOP)
#define ELAN_SPI_TX_BUF_SIZE 2
#define ELAN_SPI_RX_BUF_SIZE (IMG_BUF_SIZE / ELAN_DMA_LOOP)

/* These are only supported on the EFSA80SG. */
#define CHARGE_PUMP_HVIC 0x83
#define VOLTAGE_HVIC 0x00

/* Polling scan status counter */
#define POLLING_SCAN_TIMER 10000

/* Re-calibration timer */
#define REK_TIMES 3

/* Console output macros */
#define LOGE_SA(format, args...) elan_log_var(format, ##args)

/**
 * @brief Write fp command to the sensor
 *
 * Unused by staticlib.
 *
 * @param[in] fp_cmd     One byte fp command to write
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_write_cmd(uint8_t fp_cmd);

/**
 * @brief Read fp register data from the sensor
 *
 * @param[in]   fp_cmd   One byte fp command to read
 * @param[out]  regdata  One byte data where register's data will be stored
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_read_cmd(uint8_t fp_cmd, uint8_t *regdata);

/**
 * @brief Transfers and receives SPI data.
 *
 * Unused by staticlib.
 *
 * @param[in]   tx              The buffer to transfer
 * @param[in]   tx_len          The length to transfer
 * @param[out]  rx              The buffer where read data will be stored
 * @param[in]   rx_len          The length to receive
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_spi_transaction(uint8_t *tx_data, int tx_len,
				  uint8_t *rx_data, int rx_len);

/**
 * @brief Write fp register data to sensor
 *
 * @param[in]    regaddr  One byte register address to write
 * @param[in]    regdata  Data to write to register
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_write_register(uint8_t regaddr, uint8_t regdata);

/**
 * @brief Read fp register data from sensor
 *
 * @param[in]    regaddr  One byte register address to read
 * @param[out]   regdata  Single byte value read from register
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_read_register(uint8_t regaddr, uint8_t *regdata);

/**
 * @brief Select sensor RAM page of register
 *
 * @param[in]   page    The number of RAM page control registers
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_write_page(uint8_t page);

/**
 * @brief Write register table to fp sensor
 *
 * Using a table to write data to sensor register.
 * This table contains multiple pairs of address and data to
 * be written.
 *
 * @param[in]    reg_table       The register address to write
 * @param[in]    length          The data to write to register
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_write_reg_vector(const uint8_t *reg_table, int length);

/**
 * Get 14bits raw image data from ELAN fingerprint sensor
 *
 * @param[out] short_raw    The memory buffer to receive fingerprint image
 *                          raw data, buffer length is:
 *                          (IMAGE_WIDTH*IMAGE_HEIGHT)*sizeof(uint16_t)
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_raw_capture(uint16_t *short_raw);

/**
 * Execute calibrate ELAN fingerprint sensor flow.
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_execute_calibration(void);

/**
 * Runs a test for defective pixels.
 *
 * @param[out] fp_sensor_info  Structure containing output data.
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_fp_sensor_maintenance(fp_sensor_info_t *fp_sensor_info);

/**
 * @brief Set sensor reset state.
 *
 * Set sensor reset state.
 *
 * @param[in] state Reset state.
 *                  true  => reset sensor, i.e. low GPIO state
 *                  false => normal operation, i.e. high GPIO state
 */
void __unused elan_sensor_set_rst(bool state);

/**
 * Enable or disable settings of high voltage chip for ELAN fingerprint sensor
 *
 * This can only be used on the EFSA80SG.
 *
 * @param[in] state The state of high voltage chip
 *                  true  => enable settings of high voltage chip
 *                  false => disable settings of high voltage chip
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_set_hv_chip(bool state);

/**
 * @brief Sleep
 *
 * @param[in] us    Number of microseconds to sleep.
 * @return 0 on success, negative on error
 */
int __unused elan_usleep(unsigned int us);

/**
 * @brief Allocate memory
 *
 * @param size Allocation size
 * @return Address on successful allocation, panic otherwise
 */
void *__unused elan_malloc(uint32_t size);

/**
 * @brief Free previously allocated memory
 *
 * @param data Pointer to buffer that should be freed
 */
void __unused elan_free(void *data);

/**
 * @brief Output console message
 *
 * @param format Pointer to buffer that should be output console
 */
void __unused elan_log_var(const char *format, ...);

/**
 * @brief Reads the system tick counter.
 *
 * @return Tick count since system startup. [ms]
 */
uint32_t __unused elan_get_tick(void);

/**
 * Set ELAN fingerprint sensor register initialization
 *
 * @return 0 on success.
 *         negative value on error.
 */
int __unused elan_register_initialization(void);

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_ELAN80SG_PAL_SENSOR_H_ */
