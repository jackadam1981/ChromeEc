
/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "fw_config.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

#ifndef CONFIG_CROS_BOARD_INFO
BUILD_ASSERT(false, "fw_config utils depends on CONFIG_CROS_BOARD_INFO");
#endif

static uint32_t cached_fw_config;
static bool cached_fw_config_is_valid;

/*
 * Initialize fw_config cache. This has priority HOOK_PRIO_FIRST so it will
 * be initialized before other tasks read it.
 */
static void cbi_fw_config_init(void)
{
	if (cbi_get_fw_config(&cached_fw_config) != EC_SUCCESS) {
		cached_fw_config_is_valid = false;
		CPRINTS("FW_CONFIG failed to initialize");
		return;
	}
	CPRINTS("FW_CONFIG: 0x%04X", cached_fw_config);

	if (cached_fw_config == 0xFFFFFFFF) {
		cached_fw_config_is_valid = false;
		CPRINTS("FW_CONFIG is not valid");
		return;
	}

#ifdef CONFIG_FW_CONFIG_ZERO_IS_NOT_VALID
	if (cached_fw_config == 0) {
		cached_fw_config_is_valid = false;
		CPRINTS("FW_CONFIG is not valid");
		return;
	}
#endif
	cached_fw_config_is_valid = true;
}
DECLARE_HOOK(HOOK_INIT, cbi_fw_config_init, HOOK_PRIO_FIRST);

inline bool is_fw_config_valid(void)
{
	return cached_fw_config_is_valid;
}

inline int get_fw_config_field(const struct _fw_config_field field)
{
	if (!is_fw_config_valid())
		return -1;
	return (cached_fw_config >> field.offset) & ((1<<field.width) - 1);
}

inline bool is_fw_config_value(const struct _fw_config_value value)
{
	return get_fw_config_field(value.field) == value.value;
}
