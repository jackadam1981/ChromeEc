/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"

#include <zephyr/console/console.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

int cbi_remote_get_board_info(enum cbi_data_tag tag, uint8_t *buffer,
			      uint8_t *buffer_size);

LOG_MODULE_REGISTER(ish_uart, LOG_LEVEL_INF);

static int configure_console_uart(void)
{
	uint8_t board_version;
	uint8_t buffer_size = sizeof(board_version);
	int ret = cbi_remote_get_board_info(CBI_TAG_BOARD_VERSION,
					    &board_version, &buffer_size);

	if (ret == 0 && board_version < 2) {
		/* Switch console to uart1 for board version < 2 */
		const struct device *uart1_dev =
			DEVICE_DT_GET(DT_NODELABEL(uart1));

		if (uart1_dev != NULL && device_is_ready(uart1_dev)) {
			LOG_ERR("Console switched to UART1 (board version %d < 2)",
				board_version);
			// Get the current shell instance
			const struct shell *shell_uart =
				shell_backend_uart_get_ptr();

			if (shell_uart) {
				// Stop current shell
				shell_stop(shell_uart);

				// Reinitialize shell with uart1
				struct shell_backend_config_flags cfg_flags = {
					.insert_mode = 0,
					.echo = 1,
					.obscure = 0,
					.mode_delete = 1,
					.use_colors = 1,
					.use_vt100 = 1
				};

				shell_init(shell_uart, uart1_dev, cfg_flags,
					   true, LOG_LEVEL_INF);
				shell_start(shell_uart);
			}

			LOG_ERR(" ##Console switched to UART1 (board version %d < 2)",
				board_version);
		} else {
			LOG_ERR("## UART1 device not ready, keeping console on UART0");
		}
	} else {
		/* Keep console on uart0 for board version >= 2 or CBI read
		 * failed */
		LOG_ERR("## Console remains on UART0 (board version %d >= 2 or CBI read failed)",
			ret == 0 ? board_version : 0);
	}

	return 0;
}

/* Call this function during system initialization */
SYS_INIT(configure_console_uart, POST_KERNEL,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);