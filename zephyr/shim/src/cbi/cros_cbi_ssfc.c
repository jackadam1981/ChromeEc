/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <logging/log.h>

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "cros_cbi_ssfc.h"

#define DT_DRV_COMPAT named_cbi_ssfc_value

LOG_MODULE_REGISTER(cros_cbi_ssfc, LOG_LEVEL_ERR);

static const uint8_t ssfc_values[] = {
	DT_INST_FOREACH_STATUS_OKAY(CBI_SSFC_VALUE_ARRAY)
};

static union cbi_ssfc cached_ssfc;

void cros_cbi_ssfc_init(void)
{
	if (cbi_get_ssfc(&cached_ssfc.raw_value) != EC_SUCCESS) {
		DT_INST_FOREACH_STATUS_OKAY_VARGS(CBI_SSFC_INIT_DEFAULT,
						  cached_ssfc)
	}

	LOG_INF("Read CBI SSFC : 0x%08X\n", cached_ssfc.raw_value);
}

static int cros_cbi_ssfc_get_parent_field_value(union cbi_ssfc cached_ssfc,
						enum cbi_ssfc_value_id value_id,
						uint32_t *value)
{
	switch (value_id) {
		DT_INST_FOREACH_STATUS_OKAY_VARGS(CBI_SSFC_PARENT_VALUE_CASE,
						  cached_ssfc, value)
	default:
		LOG_ERR("CBI SSFC parent field value not found: %d\n",
		        value_id);
		return -EINVAL;
	}
	return 0;
}

bool cros_cbi_ssfc_check_match(enum cbi_ssfc_value_id value_id)
{
	int rc;
	uint32_t value;

	rc = cros_cbi_ssfc_get_parent_field_value(cached_ssfc, value_id,
						  &value);
	if (rc) {
		return false;
	}
	return value == ssfc_values[value_id];
}
