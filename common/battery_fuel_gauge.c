/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery fuel gauge parameters
 */

#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "console.h"
#include "hooks.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/*
 * Authenticate the battery connected.
 *
 * Compare the manufacturer name read from the fuel gauge to the
 * manufacturer names defined in the board_battery_info table. If
 * a device name has been specified in the board_battery_info table,
 * then both the manufacturer and device name must match.
 */
static bool authenticate_battery_type(int index, char *manuf_name)
{
	char device_name[32];

	const struct fuel_gauge_info * const fuel_gauge =
			&board_battery_info[index].fuel_gauge;
	int len = 0;

	/* check for valid index */
	if (index >= BATTERY_TYPE_COUNT)
		return false;

	/* manufacturer name mismatch */
	if (strcasecmp(manuf_name, fuel_gauge->manuf_name))
		return false;

	/* device name is specified in table */
	if (fuel_gauge->device_name != NULL) {

		/* Get the device name */
		if (battery_device_name(device_name,
				sizeof(device_name)))
			return false;

		len = strlen(fuel_gauge->device_name);

		/* device name mismatch */
		if (strncasecmp(device_name, fuel_gauge->device_name,
					len))
			return false;
	}

	CPRINTS("found batt:%s", fuel_gauge->manuf_name);
	return true;
}

#ifdef CONFIG_BATTERY_TYPE_NO_AUTO_DETECT

/* Variable to decide the battery type */
static int fixed_battery_type = BATTERY_TYPE_UNINITIALIZED;

/*
 * Function to get the fixed battery type.
 */
static int battery_get_fixed_battery_type(void)
{
	if (fixed_battery_type == BATTERY_TYPE_UNINITIALIZED) {
		CPRINTS("Warning: Battery type is not Initialized! "
				"Setting to default battery type.\n");
		fixed_battery_type = DEFAULT_BATTERY_TYPE;
	}

	return fixed_battery_type;
}

/*
 * Function to set the battery type, when auto-detection cannot be used.
 */
void battery_set_fixed_battery_type(int type)
{
	if (type < BATTERY_TYPE_COUNT)
		fixed_battery_type = type;
}
#endif /* CONFIG_BATTERY_TYPE_NO_AUTO_DETECT */

/* Get type of the battery connected on the board */
static int get_battery_type(void)
{
	char manuf_name[32];
	int i;
	static enum battery_type battery_type = BATTERY_TYPE_COUNT;

	/*
	 * If battery_type is not the default value, then can return here
	 * as there is no need to query the fuel gauge.
	 */
	if (battery_type != BATTERY_TYPE_COUNT)
		return battery_type;

	/* Get the manufacturer name. If can't read then just exit */
	if (battery_manufacturer_name(manuf_name, sizeof(manuf_name)))
		return battery_type;

<<<<<<< HEAD   (8c53b8 battery: Set EC_BATT_FLAG_INVALID_DATA correctly)
	/*
	 * Compare the manufacturer name read from the fuel gauge to the
	 * manufacturer names defined in the board_battery_info table. If
	 * a device name has been specified in the board_battery_info table,
	 * then both the manufacturer and device name must match.
	 */
	for (i = 0; i < BATTERY_TYPE_COUNT; i++) {
		const struct fuel_gauge_info * const fuel_gauge =
			&board_battery_info[i].fuel_gauge;

		if (strcasecmp(manuf_name, fuel_gauge->manuf_name))
			continue;

		if (fuel_gauge->device_name != NULL) {

			if (battery_device_name(device_name,
						sizeof(device_name)))
				continue;

			if (strcasecmp(device_name, fuel_gauge->device_name))
				continue;
		}

		CPRINTS("found batt:%s", fuel_gauge->manuf_name);
=======
#if defined(CONFIG_BATTERY_TYPE_NO_AUTO_DETECT)
	i = battery_get_fixed_battery_type();
	if (authenticate_battery_type(i, manuf_name))
>>>>>>> CHANGE (eb25e8 Merge remote-tracking branch cros/main into firmware-dedede-)
		battery_type = i;
#else
	for (i = 0; i < BATTERY_TYPE_COUNT; i++) {
		if (authenticate_battery_type(i, manuf_name)) {
			battery_type = i;
			break;
		}
	}
#endif

	return battery_type;
}

/*
 * Initialize the battery type for the board.
 *
 * The first call to battery_get_info() is when the charger task starts, so
 * initialize the battery type as soon as I2C is initialized.
 */
static void init_battery_type(void)
{
	if (get_battery_type() == BATTERY_TYPE_COUNT)
		CPRINTS("battery not found");
}
DECLARE_HOOK(HOOK_INIT, init_battery_type, HOOK_PRIO_INIT_I2C + 1);

static inline const struct board_batt_params *get_batt_params(void)
{
	int type = get_battery_type();

	return &board_battery_info[type == BATTERY_TYPE_COUNT ?
		DEFAULT_BATTERY_TYPE : type];
}

const struct battery_info *battery_get_info(void)
{
	return &get_batt_params()->batt_info;
}

int board_cut_off_battery(void)
{
	int rv;
	int cmd;
	int data;
	int type = get_battery_type();

	/* If battery type is unknown can't send ship mode command */
	if (type == BATTERY_TYPE_COUNT)
		return EC_RES_ERROR;

	/* Ship mode command requires writing 2 data values */
	cmd = board_battery_info[type].fuel_gauge.ship_mode.reg_addr;
	data = board_battery_info[type].fuel_gauge.ship_mode.reg_data[0];
	rv = sb_write(cmd, data);
	if (rv != EC_SUCCESS)
		return EC_RES_ERROR;

	data = board_battery_info[type].fuel_gauge.ship_mode.reg_data[1];
	rv = sb_write(cmd, data);

	return rv ? EC_RES_ERROR : EC_RES_SUCCESS;
}

/*
 * This function checks the charge/discharge FET status bits. Each battery type
 * supported provides the register address, mask, and disconnect value for these
 * 2 FET status bits. If the FET status matches the disconnected value, then
 * BATTERY_DISCONNECTED is returned. This function is required to handle the
 * cases when the fuel gauge is awake and will return a non-zero state of
 * charge, but is not able yet to provide power (i.e. discharge FET is not
 * active). By returning BATTERY_DISCONNECTED the AP will not be powered up
 * until either the external charger is able to provided enough power, or
 * the battery is able to provide power and thus prevent a brownout when the
 * AP is powered on by the EC.
 */
enum battery_disconnect_state battery_get_disconnect_state(void)
{
	int rv;
	int reg;
	uint8_t data[6];
	int type = get_battery_type();

	/* If battery type is not known, can't check CHG/DCHG FETs */
	if (type == BATTERY_TYPE_COUNT) {
		/* Still don't know, so return here */
		return BATTERY_DISCONNECT_ERROR;
	}

	/* Read the status of charge/discharge FETs */
	if (board_battery_info[type].fuel_gauge.fet.mfgacc_support == 1) {
		rv = sb_read_mfgacc(PARAM_OPERATION_STATUS,
				SB_ALT_MANUFACTURER_ACCESS, data, sizeof(data));
		/* Get the lowest 16bits of the OperationStatus() data */
		reg = data[2] | data[3] << 8;
	} else
		rv = sb_read(board_battery_info[type].fuel_gauge.fet.reg_addr,
					&reg);

	if (rv)
		return BATTERY_DISCONNECT_ERROR;

	if ((reg & board_battery_info[type].fuel_gauge.fet.reg_mask) ==
	    board_battery_info[type].fuel_gauge.fet.disconnect_val) {
		CPRINTS("Batt disconnected: reg 0x%04x mask 0x%04x disc 0x%04x",
			reg,
			board_battery_info[type].fuel_gauge.fet.reg_mask,
			board_battery_info[type].fuel_gauge.fet.disconnect_val);
		return BATTERY_DISCONNECTED;
	}

	return BATTERY_NOT_DISCONNECTED;
}

#ifdef CONFIG_BATTERY_MEASURE_IMBALANCE
int battery_imbalance_mv(void)
{
	int type = get_battery_type();

	/*
	 * If battery type is unknown, we cannot safely access non-standard
	 * registers.
	 */
	return (type == BATTERY_TYPE_COUNT) ? 0 :
		board_battery_info[type].fuel_gauge.imbalance_mv();
}

int battery_default_imbalance_mv(void)
{
	return 0;
}
#endif /* CONFIG_BATTERY_MEASURE_IMBALANCE */
