/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"

#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>

/*
We need the to register one log module for unknown reason. Otherwise, all logs
will disappear.
*/
LOG_MODULE_REGISTER(pdc_runtime_log);

static int command_pdc_log(int argc, const char **argv)
{
	if (argc != 2) {
		return EC_ERROR_PARAM_COUNT;
	}
	const char *input_level = argv[1];
	static const char *levels[] = {
		"NONE", "ERR", "WRN", "INF", "DBG",
	};
	static const uint32_t level_values[] = {
		LOG_LEVEL_NONE, LOG_LEVEL_ERR, LOG_LEVEL_WRN,
		LOG_LEVEL_INF,	LOG_LEVEL_DBG,
	};
	uint32_t level = -1u;
	for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
		if (strcmp(input_level, levels[i]) == 0) {
			level = level_values[i];
			break;
		}
	}
	if (level == -1u) {
		return EC_ERROR_PARAM1;
	}
	static const char *log_modules[] = {
		STRINGIFY(pdc_power_mgmt),
#ifdef CONFIG_USBC_PDC_RTS54XX
		STRINGIFY(pdc_rts54),
#endif /* CONFIG_USBC_PDC_RTS54XX */
#ifdef CONFIG_USBC_PDC_TPS6699X
		STRINGIFY(tps6699x),
#endif /* CONFIG_USBC_PDC_TPS6699X */
	};
	for (size_t i = 0; i < sizeof(log_modules) / sizeof(log_modules[0]);
	     ++i) {
		const char *module = log_modules[i];
		int source_id = log_source_id_get(module);
		if (source_id < 0) {
			ccprintf("pdc_log module:%s skipped\n", module);
			continue;
		}
		ccprintf("pdc_log module:%s try to set log level to %" PRIu32
			 "\n",
			 module, level);
		uint32_t current_level =
			log_filter_set(NULL, 0, source_id, level);
		ccprintf("pdc_log module:%s log level becomes %" PRIu32 "\n",
			 module, current_level);
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pdc_log, command_pdc_log, "<NONE|ERR|WRN|INF|DBG>",
			"Control pdc log at runtime.");
