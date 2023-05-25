/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "charger.h"
#include "console.h"
#include "hooks.h"
#include "extpower.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

int thermals[6];
int thermal_cyc;

#define CHARGING_VOLTAGE_MV_SAFE 8800
#define CHARGING_CURRENT_MA_SAFE 5000

int charger_profile_override(struct charge_state_data *curr)
{
    //static int current_level;
	int charger_temp, charger_temp_c , charger_temp_ave;
    int charger_temp_sum = 0;
	
    /*
	 * Keep track of battery temperature range:
	 *
	 *     ZONE_0  ZONE_1   ZONE_2  ZONE_3
	 * ---+------+--------+--------+------+--- Temperature (C)
	 *    0      5        12       45     50
	 */
    enum {
		TEMP_ZONE_0, /* 0 <= bat_temp_c <= 50 */
		TEMP_ZONE_1, /* 50 < bat_temp_c <= 59 */
		TEMP_ZONE_2, /* 59 < bat_temp_c <= 62 */
		TEMP_ZONE_3, /* 62 < bat_temp_c j*/
		TEMP_ZONE_COUNT,
		TEMP_OUT_OF_RANGE = TEMP_ZONE_COUNT
	} temp_zone;
    
    if (!(curr->batt.flags & BATT_FLAG_RESPONSIVE))
		return 0;

    int current = curr->requested_current;

    temp_sensor_read(TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(charger_bc12_port1)),
        &charger_temp);

    charger_temp_c = K_TO_C(charger_temp);
    //CPRINTS(" charger_temp_c %d", charger_temp_c);

    if(thermal_cyc != 6) {
        thermals[thermal_cyc] = charger_temp_c;
        thermal_cyc ++;
    } else {
        thermal_cyc = 0;
        thermals[thermal_cyc] = charger_temp_c;
    }

    for (int i = 0;i < 6 ;i++)
        charger_temp_sum +=  thermals[i];
    
    charger_temp_ave = charger_temp_sum / 6 ;
    
    if ((curr->batt.flags & BATT_FLAG_BAD_TEMPERATURE) ||
	    (charger_temp_ave < 0) || (charger_temp_ave > 500))
		temp_zone = TEMP_OUT_OF_RANGE;
    else if (charger_temp_ave <= 50)
        temp_zone = TEMP_ZONE_0;
	else if (charger_temp_ave <= 53)
		temp_zone = TEMP_ZONE_1;
	else if (charger_temp_ave <= 56)
		temp_zone = TEMP_ZONE_2;
	else
		temp_zone = TEMP_ZONE_3;

    switch (temp_zone)
    {
        case TEMP_ZONE_0:
		    current = CHARGING_CURRENT_MA_SAFE;
		break;

        case TEMP_ZONE_1:
            current = 2500;
            break;

        case TEMP_ZONE_2:
            current = 1800;
            break;

        case TEMP_ZONE_3:
            current = 1000;
            break;

        case TEMP_OUT_OF_RANGE:
            /* Don't charge if outside of allowable temperature range */
            current = 0;
            curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
            if (curr->state != ST_DISCHARGE)
                curr->state = ST_IDLE;
            break;
    }

    curr->requested_current = MIN(curr->requested_current, current);
    //CPRINTS("curr->requested_current %d", curr->requested_current);

    return 0;
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
