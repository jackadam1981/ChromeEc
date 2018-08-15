/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "isl923x.h"

static volatile uint32_t test_Item;
static volatile uint32_t count;
uint8_t test[6];

void DCStress_task(void *u)
{
	char device_name[32];
	char manu_name[32];
	int capacity, voltage, sb_chgcurrent, bat_status, percent;
	uint8_t i;

	for (i = 0; i < 6; i++)
		test[i] = 0;

	while (1) {
		ccprintf("zzzz, test_Item=%d, count=%d\n", test_Item, count);

		if (test_Item == 1) {
			while (count < 100000) {
				battery_device_name(device_name,
					sizeof(device_name));
				if (!strcasecmp(device_name,
								"AC15A3J")) {
					ccprintf("device name %s count %d\n",
						device_name, count);
				} else {
					ccprintf("NG device name %s\n",
						device_name);
					test[0] = 1;
					break;
				}
				task_wait_event(500*MSEC); //500ms
				count++;
			}
			count = 0;
			test_Item = 2;
		} else if (test_Item == 2) {
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
					test[1] = 1;
					break;
				}
				task_wait_event(500*MSEC); //500ms
				count++;

			}
			count = 0;
			test_Item = 3;
		} else if (test_Item == 3) {
			//design capacity check
			while (count < 100000) {
				if (sb_read(SB_DESIGN_CAPACITY, &capacity)) {
					test[2] = 1;
					ccprintf("Design C GG\n\r");
					break;
				}
				task_wait_event(500*MSEC); //500ms
				ccprintf("Batter design capacity = %d\n\r",
					capacity);
				count++;
			}
			count = 0;
			test_Item = 4;
		} else if (test_Item == 4) {
			//design voltage check
			while (count < 100000) {

				if (sb_read(SB_DESIGN_VOLTAGE, &voltage)) {
					test[3] = 1;
					ccprintf("Design v GG\n\r");
					break;
				}
				task_wait_event(500*MSEC); //500ms
				ccprintf("Batter design Voltage = %d\n\r",
					voltage);
				count++;
			}
			count = 0;
			test_Item = 5;

		} else if (test_Item == 5) {
			//record CHGCC , BT1C
			sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &percent);
			while (percent >= 4) {
				sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &percent);
				//if (count == 0)
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
				//if (count != 2)
				charger_discharge_on_ac(0); //charge
				sb_read(SB_CHARGING_CURRENT, &sb_chgcurrent);
				task_wait_event(500*MSEC); //500ms
				ccprintf("SB request charge current %d\n\r",
					sb_chgcurrent);
				ccprintf("SB real AVG Current %d\n\r",
					battery_get_avg_current());
				count = 2;
				cflush();
			}
			count = 0;
			test_Item = 6;
		} else if (test_Item == 6) {
			//record CHGCC , BT1C
			sb_read(SB_BATTERY_STATUS, &bat_status);
			task_wait_event(500*MSEC); //500ms
			ccprintf("Full Charged Bit %x\n\r",
				bat_status);
			test_Item = 0;
		} else {
			for (i = 0; i < 6; i++)
				ccprintf("Test Item %d\n",
				test[i]);
			task_wait_event(-1);
		}
	}
}

int choice_stress_item(int argc, char **argv)
{
	ccprintf("Strees Item: %s\n", argv[1]);
	if (argc < 1) {
		ccprintf("Input type Error !!\n");
		return EC_ERROR_INVAL;
	}
	if (!strcasecmp(argv[1], "device_name")) {
		test_Item = 1;
		count = 0;
		ccprintf(" ok device name, test_Item=%d, count=%d\n",
			test_Item, count);
		task_wake(TASK_ID_DEBUG);
	} else if (!strcasecmp(argv[1], "manu_name")) {
		test_Item = 2;
		count = 0;
		ccprintf("manu_name,test_Item=%d, count=%d\n",
			test_Item, count);
		task_wake(TASK_ID_DEBUG);
	} else if (!strcasecmp(argv[1], "design_C")) {
		test_Item = 3;
		count = 0;
		ccprintf(" ok design_C\n");
		task_wake(TASK_ID_DEBUG);
	} else if (!strcasecmp(argv[1], "design_V")) {
		test_Item = 4;
		count = 0;
		ccprintf(" ok design_V\n");
		task_wake(TASK_ID_DEBUG);
	} else if (!strcasecmp(argv[1], "log_CC")) {
		test_Item = 5;
		count = 0;
		ccprintf(" ok log_C\n");
		task_wake(TASK_ID_DEBUG);
	} else if (!strcasecmp(argv[1], "BatStatus")) {
		test_Item = 6;
		count = 0;
		ccprintf(" ok FULLCharge_Chk\n");
		task_wake(TASK_ID_DEBUG);
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(streeItem, choice_stress_item,
		NULL, "Stress Battery Test");
