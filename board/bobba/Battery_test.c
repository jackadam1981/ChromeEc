/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "charger.h"

#include "battery_fuel_gauge.h"
#include "common.h"

static volatile uint32_t test_Item;
static volatile uint32_t count;
uint8_t Result_Item[7];

enum battery_Test_index {
	BATT_INIT,
	BATT_DEVICE_NAME,
	BATT_MANU_NAME,
	BATT_DESIGN_CAP,
	BATT_DESIGN_VOLTAGE,
	BATT_LOG_CC,
	BATT_ALARM,
};

int device_name_Chk(void)
{
	char device_name[32];
	uint8_t j = 0;

	battery_device_name(device_name,
					sizeof(device_name));
	ccprintf("Current Battery device name %s\n", device_name);
	while (strcasecmp(device_name,
		board_battery_info[j].fuel_gauge.device_name)) {
		j++;
		if (j == BATTERY_TYPE_COUNT) {
			ccprintf("Battery Device name not match!!\n");
			return EC_ERROR_UNKNOWN;
		}
	}
	ccprintf("Find match %d %s!!\n", j,
		board_battery_info[j].fuel_gauge.device_name);
	return EC_SUCCESS;
}

void DCStress_task(void *u)
{
	//char device_name[32];
	char manu_name[32];
	int capacity, voltage, sb_chgcurrent, bat_status, percent;
	uint8_t i;

	for (i = 0; i < 6; i++)
		Result_Item[i] = 0;

	while (1) {
		if (test_Item == BATT_DEVICE_NAME) {
			//device_name_Chk();
			count = 0;
			test_Item = 0;
			while (count < 10) {
				if (device_name_Chk() != EC_SUCCESS) {
					Result_Item[BATT_DEVICE_NAME] = 1;
					break;
				}
				task_wait_event(500*MSEC);
				count++;
			}
			count = 0;
			test_Item = BATT_INIT;
		} else if (test_Item == BATT_MANU_NAME) {
			//manufacture name check
			while (count < 100000) {

				battery_manufacturer_name(manu_name,
					sizeof(manu_name));

				if (!strcasecmp(manu_name,
								"PANASONIC")) {
					ccprintf("manu name %s %d\n",
						manu_name, count);
				} else {
					ccprintf("NG_manu_name %s\n",
						manu_name);
					Result_Item[BATT_MANU_NAME] = 1;
					break;
				}
				task_wait_event(500*MSEC); //500ms
				count++;

			}
			count = 0;
			test_Item = BATT_INIT;
		} else if (test_Item == BATT_DESIGN_CAP) {
			//design capacity check
			while (count < 100000) {
				if (sb_read(SB_DESIGN_CAPACITY, &capacity)) {
					Result_Item[BATT_DESIGN_CAP] = 1;
					ccprintf("Design C GG\n\r");
					break;
				}
				task_wait_event(500*MSEC); //500ms
				ccprintf("Batter design capacity = %d\n\r",
					capacity);
				count++;
			}
			count = 0;
			test_Item = BATT_INIT;
		} else if (test_Item == BATT_DESIGN_VOLTAGE) {
			//design voltage check
			while (count < 100000) {

				if (sb_read(SB_DESIGN_VOLTAGE, &voltage)) {
					Result_Item[BATT_DESIGN_VOLTAGE] = 1;
					ccprintf("Design v GG\n\r");
					break;
				}
				task_wait_event(500*MSEC); //500ms
				ccprintf("Batter design Voltage = %d\n\r",
					voltage);
				count++;
			}
			count = 0;
			test_Item = BATT_INIT;

		} else if (test_Item == BATT_LOG_CC) {
			//record CHGCC , BT1C
			sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &percent);
			while (percent >= 4) {
				sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &percent);
				if (count == 0)
					charger_discharge_on_ac(1); // discharge
				ccprintf("Now discharge RSOC= %d to 0\n\r",
					percent);
				cflush();
				ccprintf("SB real AVG Current %d\n\r",
					battery_get_avg_current());
				count = 1;
				task_wait_event(500*MSEC); //500ms
				cflush();
			} while (percent != 100) {
				sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &percent);
				if (count != 2)
					charger_discharge_on_ac(0); //charge
				sb_read(SB_CHARGING_CURRENT, &sb_chgcurrent);
				task_wait_event(500*MSEC); //500ms
				ccprintf("SB request charge current %d\n\r",
					sb_chgcurrent);
				ccprintf("SB real AVG Current %d\n\r",
					battery_get_avg_current());
				ccprintf("SB RSOC %d\n\r",
					percent);
				count = 2;
				cflush();
			}
			count = 0;
			test_Item = BATT_INIT;
		} else if (test_Item == BATT_ALARM) {
			//record CHGCC , BT1C
			while (1) {
				sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &percent);
				if (percent > 95) {
					charger_discharge_on_ac(1); //discharge
				} else {
					sb_read(SB_BATTERY_STATUS, &bat_status);
				if (!(bat_status & STATUS_FULLY_CHARGED))
					count += 0;
				else
					count += 2;
					break;
				}
				task_wait_event(1000*MSEC); //1000ms
			}

			while (1) {
				sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &percent);
				if (percent < 100) {
					charger_discharge_on_ac(0); // charge
				} else {
					sb_read(SB_BATTERY_STATUS, &bat_status);
				if (bat_status & STATUS_FULLY_CHARGED)
					count += 0;
				else
					count += 20;
					break;
				}
				task_wait_event(1000*MSEC); //1000ms
			}
			Result_Item[BATT_ALARM] = count;
			count = 0;
			test_Item = BATT_INIT;
		} else {
			for (i = 1; i < 7; i++)
				ccprintf("\n Result Item [%d] =%d\n",
				i, Result_Item[i]);
			task_wait_event(-1);
		}
	}
}

int choice_stress_item(int argc, char **argv)
{
	ccprintf("Strees Item: %s\n", argv[1]);
	if (argc < 1) {
		ccprintf("\nInput type Error !!\n");
		return EC_ERROR_INVAL;
	}
	if (!strcasecmp(argv[1], "device_name")) {
		test_Item = BATT_DEVICE_NAME;
		count = 0;
		ccprintf("\nDevice name, test_Item=%d, count=%d\n",
			test_Item, count);
		task_wake(TASK_ID_BATTEST);
	} else if (!strcasecmp(argv[1], "manu_name")) {
		test_Item = BATT_MANU_NAME;
		count = 0;
		ccprintf("\nmanu_name,test_Item=%d, count=%d\n",
			test_Item, count);
		task_wake(TASK_ID_BATTEST);
	} else if (!strcasecmp(argv[1], "design_C")) {
		test_Item = BATT_DESIGN_CAP;
		count = 0;
		ccprintf("\ndesign_C\n");
		task_wake(TASK_ID_BATTEST);
	} else if (!strcasecmp(argv[1], "design_V")) {
		test_Item = BATT_DESIGN_VOLTAGE;
		count = 0;
		ccprintf("\ndesign_V Test\n");
		task_wake(TASK_ID_BATTEST);
	} else if (!strcasecmp(argv[1], "log_CC")) {
		test_Item = BATT_LOG_CC;
		count = 0;
		ccprintf("\nlog_CC Test\n");
		task_wake(TASK_ID_BATTEST);
	} else if (!strcasecmp(argv[1], "BatStatus")) {
		test_Item = BATT_ALARM;
		count = 0;
		ccprintf("\nFULLCharge_Chk\n");
		task_wake(TASK_ID_BATTEST);
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(streeItem, choice_stress_item,
		NULL, "Stress Battery Test");
