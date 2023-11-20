/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "power.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

#define CHARGING_CURRENT_MA_SAFE 5000
#define COL_NUM 5
#define ROW_NUM 2

static int thermals[COL_NUM], time[ROW_NUM][COL_NUM];
static int thermal_cyc;
static int current;
static int charger_temp_ave_bef, charger_temp_ave = 0;

enum {
	TEMP_ZONE_0, /* not limit */
	TEMP_ZONE_1, /* 2500mA */
	TEMP_ZONE_2, /* 1800mA */
	TEMP_ZONE_3, /* 1000mA */
	TEMP_ZONE_COUNT,
	TEMP_OUT_OF_RANGE = TEMP_ZONE_COUNT /* Not charging */
} temp_zone = TEMP_ZONE_0;

static void clean_array(int arr[][COL_NUM], int row) {
    int i, j;
    for (i = 0; i < row; i++) {
        for (j = 0; j < COL_NUM; j++) {
            arr[i][j] = 0;
		}
	}
}

/*Except time[exceptrow][exceptcol], everything else is cleared to 0*/
static void initializearray(int arr[][COL_NUM], int row, int exceptrow, int exceptcol) {  
    int i, j;

	time[exceptrow][exceptcol]++;

    for (i = 0; i < row; i++) {
        if (i != exceptrow) {
            for (j = 0; j < COL_NUM; j++) {
                if (j != exceptcol) {
                    arr[i][j] = 0;
                }
            }
        }
    }
}

/* Called by hook task every hook second (1 sec) */
static void average_tempature(void)
{
	int charger_temp, charger_temp_c;

	int charger_temp_sum = 0;
	enum power_state chipset_state = power_get_state();

	/*
	 * Keep track of battery temperature range:
	 *
	 *     ZONE_0  ZONE_1   ZONE_2  ZONE_3
	 * --->------>-------->-------->------>--- Temperature (C)
	 *    0      50       53       56     80
	 *     ZONE_0  ZONE_1   ZONE_2  ZONE_3
	 * ---<------<--------<--------<------<--- Temperature (C)
	 *    0      45        50       54     80
	 */
	temp_sensor_read(
		TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(charger_bc12_port1)),
		&charger_temp);

	charger_temp_c = K_TO_C(charger_temp);

	thermals[thermal_cyc] = charger_temp_c;
	thermal_cyc = (thermal_cyc + 1) % 5;
	for (int i = 0; i < 5; i++)
		charger_temp_sum += thermals[i];

	charger_temp_ave_bef = charger_temp_ave;
	charger_temp_ave = (charger_temp_sum + 2.5) / 5;

	if (thermals[4]) {
		if (chipset_state != POWER_S0) {
			if (charger_temp_ave >= 85 && temp_zone != TEMP_OUT_OF_RANGE)
				initializearray(time, ROW_NUM, 0, 4);
			if (charger_temp_ave < 75 && temp_zone != TEMP_ZONE_0)
				initializearray(time, ROW_NUM, 0, 0);
		} else {
			if (charger_temp_ave_bef < charger_temp_ave) {
				if (charger_temp_ave >= 90 && temp_zone <= TEMP_ZONE_3) 
					initializearray(time, ROW_NUM, 0, 4);
				else if (charger_temp_ave >= 85 && temp_zone <= TEMP_ZONE_2)
					initializearray(time, ROW_NUM, 0, 3);
				else if (charger_temp_ave >= 75 && temp_zone <= TEMP_ZONE_1)
					initializearray(time, ROW_NUM, 0, 2);
				else if (charger_temp_ave >= 65 && temp_zone <= TEMP_ZONE_0)
					initializearray(time, ROW_NUM, 0, 1);
			} else if (charger_temp_ave_bef > charger_temp_ave) {
				if (charger_temp_ave < 60 && temp_zone >= TEMP_ZONE_1)
					initializearray(time, ROW_NUM, 1, 0);
				else if (charger_temp_ave < 66 && temp_zone >= TEMP_ZONE_2)
					initializearray(time, ROW_NUM, 1, 1);
				else if (charger_temp_ave < 76 && temp_zone >= TEMP_ZONE_3)
					initializearray(time, ROW_NUM, 1, 2);
				else if (charger_temp_ave < 86 && temp_zone >= TEMP_OUT_OF_RANGE)
					initializearray(time, ROW_NUM, 1, 3);
			}
		}
	}

	for (int i = 0; i < 5; i++) {
		if (time[0][i] == 3 || time [1][i] == 3)
		{
			temp_zone = i;
			clean_array(time,ROW_NUM);
		}
	}

	switch (temp_zone) {
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

		current = 0;
		break;
	}
	CPRINTS("---temp_zone : %d, ave_bef : %d, charger_temp_ave : %d---",temp_zone, charger_temp_ave_bef, charger_temp_ave);
}
DECLARE_HOOK(HOOK_SECOND, average_tempature, HOOK_PRIO_DEFAULT);

int charger_profile_override(struct charge_state_data *curr)
{
	/*
	 * Precharge must be executed when communication is failed on
	 * dead battery.
	 */
	if (!(curr->batt.flags & BATT_FLAG_RESPONSIVE))
		return 0;
		
	/* Don't charge if outside of allowable temperature range */
	if (current == 0){
		curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
		if (curr->state != ST_DISCHARGE)
			curr->state = ST_IDLE;
	}

	curr->requested_current = MIN(curr->requested_current, current);

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
