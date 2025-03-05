// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_EGIS630_PAL_SENSOR_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_EGIS630_PAL_SENSOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/toolchain.h>

typedef enum {
	LOG_VERBOSE = 2,
	LOG_DEBUG = 3,
	LOG_INFO = 4,
	LOG_WARN = 5,
	LOG_ERROR = 6,
	LOG_ASSERT = 7,
} LOG_LEVEL;

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

/**
 * @brief Sets the global debug level, controlling which log messages are
 * output.
 *
 * @param[in] level The desired debug level.
 *
 */
void set_debug_level(LOG_LEVEL level);

/**
 * @brief Formats and outputs a log message based on the provided level, tag,
 * file information, and format string.
 *
 * @param[in] level The log level of the message.
 * @param[in] tag A tag or category for the message.
 * @param[in] file_name The file path where the log message originates.
 * @param[in] func The function name where the log message originates.
 * @param[in] line The line number where the log message originates.
 * @param[in] format A printf-style format string for the message.
 * @param[in] ... Variable number of arguments to be formatted according
 * to the format string.
 *
 */
void __unused output_log(LOG_LEVEL level, const char *tag,
			 const char *file_name, const char *func, int line,
			 const char *format, ...);

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_EGIS630_PAL_SENSOR_H_ */
