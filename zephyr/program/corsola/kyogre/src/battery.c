/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "charge_state.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

/* Vendor command parameter */
#define SB_LOT_DATA 0x31

#define UPDATED_LE01 BIT(0)
#define UPDATED_LE02 BIT(1)
#define UPDATED_LE03 BIT(2)

static uint32_t sb_cmd_01 = 0, sb_data_01 = 0;
static uint32_t sb_cmd_02 = 0, sb_data_02 = 0;
static uint32_t sb_cmd_03 = 0, sb_data_03 = 0;

static int updated;

__override bool board_batt_conf_enabled(void)
{
	return false;
}

static int board_bcfg_search_in_cbi(void)
{
	int rv;
	uint8_t buf[3];
	uint8_t size = sizeof(buf);

	rv = cbi_get_board_info(CBI_TAG_BATTERY_CONFIG, buf, &size);
	if (rv) {
		CPRINTS("No config #12 (%d)", rv);
		return rv;
	} else {
		sb_cmd_01 = buf[0];
		sb_data_01 = buf[1] + buf[2] * 0x100;
	}

	rv = cbi_get_board_info(CBI_TAG_BATTERY_CONFIG + 1, buf, &size);
	if (rv) {
		CPRINTS("No config #13 (%d)", rv);
		return rv;
	} else {
		sb_cmd_02 = buf[0];
		sb_data_02 = buf[1] + buf[2] * 0x100;
	}

	rv = cbi_get_board_info(CBI_TAG_BATTERY_CONFIG + 2, buf, &size);
	if (rv) {
		CPRINTS("No config #14 (%d)", rv);
		return rv;
	} else {
		sb_cmd_03 = buf[0];
		sb_data_03 = buf[1] + buf[2] * 0x100;
	}

	return EC_SUCCESS;
}

void init_board_battery_type(void)
{
	board_bcfg_search_in_cbi();
	return;
}
DECLARE_HOOK(HOOK_INIT, init_board_battery_type, HOOK_PRIO_BATTERY_INIT);

int charger_profile_override(struct charge_state_data *curr)
{
	uint32_t value;
	int rv;

	if (battery_is_present() != BP_YES)
		return EC_ERROR_UNCHANGED;
	if (charge_get_percent() >= 90)
		return EC_ERROR_UNCHANGED;

	if (updated != (UPDATED_LE01 | UPDATED_LE02 | UPDATED_LE03)) {
		if (sb_cmd_01 != 0 && sb_data_01 != 0) {
			rv = battery_get_vendor_param(sb_cmd_01, &value);
			if (rv)
				return rv;
			if (value == sb_data_01) {
				updated |= UPDATED_LE01;
			} else {
				return battery_set_vendor_param(sb_cmd_01,
								sb_data_01);
			}
		}

		if (sb_cmd_02 != 0 && sb_data_02 != 0) {
			rv = battery_get_vendor_param(sb_cmd_02, &value);
			if (rv)
				return rv;
			if (value == sb_data_02) {
				updated |= UPDATED_LE02;
			} else {
				return battery_set_vendor_param(sb_cmd_02,
								sb_data_02);
			}
		}

		if (sb_cmd_03 != 0 && sb_data_03 != 0) {
			rv = battery_get_vendor_param(sb_cmd_03, &value);
			if (rv)
				return rv;
			if (value == sb_data_03) {
				updated |= UPDATED_LE03;
			} else {
				return battery_set_vendor_param(sb_cmd_03,
								sb_data_03);
			}
		}

		CPRINTS("sb_cmd_01 (%x)", sb_cmd_01);
		CPRINTS("sb_data_01 (%x)", sb_data_01);
		CPRINTS("sb_cmd_02 (%x)", sb_cmd_02);
		CPRINTS("sb_data_02 (%x)", sb_data_02);
		CPRINTS("sb_cmd_03 (%x)", sb_cmd_03);
		CPRINTS("sb_data_03 (%x)", sb_data_03);
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

	if (cmd == SB_LOT_DATA) {
		if (param == SB_LOT_DATA) {
			rv = sb_read_string(SB_LOT_DATA, data,
					    sizeof(bs->vendor_param));
		} else {
			rv = EC_SUCCESS;
		}
		*value = data[param - SB_LOT_DATA];
		return rv;
	}

	if (cmd == sb_cmd_01) {
		rv = sb_read((int)sb_cmd_01, &tmp);
		*value = (uint32_t)tmp;
		return rv;
	}

	if (cmd == sb_cmd_02) {
		rv = sb_read((int)sb_cmd_02, &tmp);
		*value = (uint32_t)tmp;
		return rv;
	}

	if (cmd == sb_cmd_03) {
		rv = sb_read((int)sb_cmd_03, &tmp);
		*value = (uint32_t)tmp;
		return rv;
	}

	return EC_ERROR_INVAL;
}

__override int battery_set_vendor_param(uint32_t param, uint32_t value)
{
	if (param == sb_cmd_01)
		return sb_write((int)sb_cmd_01, (int)value);

	if (param == sb_cmd_02)
		return sb_write((int)sb_cmd_02, (int)value);

	if (param == sb_cmd_03)
		return sb_write((int)sb_cmd_03, (int)value);

	return EC_ERROR_INVAL;
}
