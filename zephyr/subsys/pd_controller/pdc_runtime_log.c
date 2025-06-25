/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"

#include <stdbool.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_PLATFORM_EC_USB_PDC_RUNTIME_LOG
static int command_pdc_log(int argc, const char **argv)
{
	if (argc != 2) {
		return EC_ERROR_PARAM_COUNT;
	}
	static const char *levels[] = {
		"NONE", "ERR", "WRN", "INF", "DBG",
	};
	int level = -1;
	for (size_t i = 0; i < sizeof(levels); ++i) {
		if (strcmp(argv[1], levels[i]) == 0) {
			level = i;
			break;
		}
	}
	if (level == -1) {
		return EC_ERROR_PARAM1;
	}
	ccprintf("pdc_log module:%s LOG_LEVEL_%s", STRINGIFY(pdc_power_mgmt),
		 argv[1]);
	log_filter_set(NULL, 0, log_source_id_get(STRINGIFY(pdc_power_mgmt)),
		       level);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pdc_log, command_pdc_log, "<NONE|ERR|WRN|INF|DBG>",
			"Control pd log at runtime.");
#endif /* CONFIG_PLATFORM_EC_USB_PDC_RUNTIME_LOG */
