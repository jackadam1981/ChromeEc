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

/**
 * @brief Gets the current time in milliseconds.
 *
 * @return The current time in milliseconds.
 *
 */
unsigned long long __unused plat_get_time(void);

/**
 * @brief Calculates the time difference in milliseconds between the current
 * time and a given starting time.
 *
 * @param[in] begin The starting time in milliseconds.
 *
 * @return The time difference in milliseconds.
 *
 */
unsigned long __unused plat_get_diff_time(unsigned long long begin);

/**
 * @brief Busy-wait.
 *
 * @param[in] msecs The delay time in milliseconds.
 *
 */
void __unused plat_wait_time(unsigned long msecs);

/**
 * @brief Sleep.
 *
 * The current task will be de-scheduled for at least the specified delay (and
 * perhaps longer, if a higher-priority task is running when the delay expires).
 *
 * @param[in] timeInMs The sleep time in milliseconds.
 *
 */
void __unused plat_sleep_time(unsigned long timeInMs);

#ifdef EGIS_SPEED_DBG
#include "plat_log.h"
#define TIME_MEASURE_START(name) \
	unsigned long long timeMeasureStart##name = plat_get_time();
#define TIME_MEASURE_STOP(name, x)                                       \
	unsigned long name = plat_get_diff_time(timeMeasureStart##name); \
	egislog_d(x SPEED_TEST_STR, name);
#define TIME_MEASURE_STOP_INFO(name, x)                                  \
	unsigned long name = plat_get_diff_time(timeMeasureStart##name); \
	egislog_i(x SPEED_TEST_STR, name);
#define TIME_MEASURE_STOP_AND_RESTART(name, x)                         \
	{                                                              \
		egislog_d(x SPEED_TEST_STR,                            \
			  plat_get_diff_time(timeMeasureStart##name)); \
		timeMeasureStart##name = plat_get_time();              \
	}
#define TIME_MEASURE_RESET(name) timeMeasureStart##name = plat_get_time();
#else
#define TIME_MEASURE_START(name)
#define TIME_MEASURE_STOP(name, x)
#define TIME_MEASURE_STOP_INFO(name, x)
#define TIME_MEASURE_STOP_AND_RESTART(name, x)
#define TIME_MEASURE_RESET(name)
#endif

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_EGIS630_PAL_SENSOR_H_ */
