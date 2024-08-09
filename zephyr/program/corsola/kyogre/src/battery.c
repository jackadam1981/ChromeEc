/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "charge_state.h"
#include "console.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

/* Vendor command parameter */
#define SB_LOT_DATA 0x31
#define SB_LE01 0x96
#define SB_LE02 0x97
#define SB_LE03 0x98

static int updated;

int charger_profile_override(struct charge_state_data *curr)
{
	uint32_t value;
	int rv;

	if (curr->state != ST_CHARGE)
		return 0;

	if (!updated) {
		rv = battery_get_vendor_param(SB_LE01, &value);
		CPRINTS("SB_LE01 : %d", value);

		rv = battery_get_vendor_param(SB_LE02, &value);
		CPRINTS("SB_LE02 : %d", value);

		rv = battery_get_vendor_param(SB_LE03, &value);
		CPRINTS("SB_LE03 : %d", value);

		updated = true;
	}

	return EC_SUCCESS;
}

enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value)
{
	return EC_RES_INVALID_PARAM;
}

enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value)
{
	return EC_RES_INVALID_PARAM;
}
__override int battery_get_vendor_param(uint32_t param, uint32_t *value)
{
	struct battery_static_info *bs = &battery_static[BATT_IDX_MAIN];
	uint8_t *data = bs->vendor_param;
	int rv, tmp;
	uint32_t cmd = param;

	if ((SB_LOT_DATA <= cmd) && (cmd < SB_LOT_DATA + 18))
		cmd = SB_LOT_DATA;

	switch (cmd) {
	case SB_LOT_DATA:
		if (param == SB_LOT_DATA) {
			rv = sb_read_string(SB_LOT_DATA, data,
					    sizeof(bs->vendor_param));
		} else {
			rv = EC_SUCCESS;
		}
		*value = data[param - SB_LOT_DATA];
		break;
	case SB_LE01:
		rv = sb_read(SB_LE01, &tmp);
		*value = (uint32_t)tmp;
		break;
	case SB_LE02:
		rv = sb_read(SB_LE02, &tmp);
		*value = (uint32_t)tmp;
		break;
	case SB_LE03:
		rv = sb_read(SB_LE03, &tmp);
		*value = (uint32_t)tmp;
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return rv;
}

__override int battery_set_vendor_param(uint32_t param, uint32_t value)
{
	int rv, tmp;
	tmp = (int)value;

	switch (param) {
	case SB_LE01:
		rv = sb_write(SB_LE01, tmp);
		break;
	case SB_LE02:
		rv = sb_write(SB_LE02, tmp);
		break;
	case SB_LE03:
		rv = sb_write(SB_LE03, tmp);
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return rv;
}
