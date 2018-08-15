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
#include "hooks.h"
#include "battery_fuel_gauge.h"
#include "common.h"

#define YES	1
#define NO	0

uint32_t test_Item;
uint32_t count;
uint8_t Result_Item[7];

enum battery_Test_index {
	BATT_INIT,
	BATT_DEVICE_NAME,
	BATT_MANU_NAME,
	BATT_DESIGN_CAP,
	BATT_DESIGN_VOLTAGE,
	BATT_LOG_CC,
	BATT_ALARM,
	BATT_ITEM_COUNT,
};

static char *batt_item_names[] = {
	"BATT_INIT		",
	"BATT_DEVICE_NAME	",
	"BATT_MANU_NAME		",
	"BATT_DESIGN_CAP		",
	"BATT_DESIGN_VOLTAGE	",
	"BATT_LOG_CC		",
	"BATT_ALARM		",
	"BATT_ITEM_ERROR	",	/* BATT_ITEM_COUNT */
};

enum batt_test_res {
	RES_NOT_YET,
	RES_GOING,
	RES_PASS,
	RES_FAIL,
};

static char *res_text[] = {
	"RES_NOT_YET",
	"RES_GOING",
	"RES_PASS",
	"RES_FAIL",
};

static struct batt_test_item {
	int run_this;
	enum batt_test_res run_result;
} test_item[BATT_ITEM_COUNT];

static int test_good_to_go = NO;

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

static void Strees_DEVICE_NAME(void)
{// TODO: BATT_DEVICE_NAME 100,000 cycles testing request from ACER
	char device_name[32];

	static int i;

	uint8_t j = 0;

	battery_device_name(device_name,
					sizeof(device_name));
	ccprintf("Current Battery device name %s\n", device_name);

	if (i++ < 100000) {
		while (strcasecmp(device_name,
			board_battery_info[j].fuel_gauge.device_name)) {
			j++;

			if (j == BATTERY_TYPE_COUNT) {
				ccprintf("Battery Device name not match!!\n");
				//return EC_ERROR_UNKNOWN;
				test_item[BATT_DEVICE_NAME].run_this = NO;
			test_item[BATT_DEVICE_NAME].run_result = RES_FAIL;
				return;
			}
		}
		ccprintf("Find match %d %s!!\n", j,
		board_battery_info[j].fuel_gauge.device_name);
	} else {
		test_item[BATT_DEVICE_NAME].run_this = NO;
		test_item[BATT_DEVICE_NAME].run_result = RES_PASS;
		return;
	}

	test_item[BATT_DEVICE_NAME].run_result = RES_GOING;
	task_wait_event(10 * MSEC);
}

static void Strees_MANU_NAME(void)
{// TODO: BATT_MANU_NAME 100,000 cycles testing request from ACER

	char manu_name[32];

	static int i;

	uint8_t j = 0;

	battery_manufacturer_name(manu_name,
					sizeof(manu_name));

	ccprintf("Current Battery manufacture name %s\n", manu_name);

	if (i++ < 100000) {
		while (strcasecmp(manu_name,
			board_battery_info[j].fuel_gauge.manuf_name)) {
			j++;

			if (j == BATTERY_TYPE_COUNT) {
				ccprintf("Battery manu name not match!!\n");
				test_item[BATT_MANU_NAME].run_this = NO;
				test_item[BATT_MANU_NAME].run_result = RES_FAIL;
				return;
			}
		}
		ccprintf("Find match %d %s!!\n", j,
		board_battery_info[j].fuel_gauge.manuf_name);
	} else {
		test_item[BATT_MANU_NAME].run_this = NO;
		test_item[BATT_MANU_NAME].run_result = RES_PASS;
		return;
	}

	test_item[BATT_MANU_NAME].run_result = RES_GOING;
	task_wait_event(10 * MSEC);
}

static void Stress_DesignCap(void)
{// TODO: BATT_DESIGN Capacity 100,000 cycles testing request from ACER

	static int i, diff_count;
	static int capacity_BAK;
	int capacity;

	if (i++ < 100000) {
		sb_read(SB_DESIGN_CAPACITY, &capacity);
		if (capacity != capacity_BAK) {
			capacity_BAK = capacity;
			diff_count++;
			if (diff_count >= 2) {
				test_item[BATT_DESIGN_CAP].run_this = NO;
				test_item[BATT_DESIGN_CAP].run_result
				= RES_FAIL;
				return;
			}
		}
	} else {
		test_item[BATT_DESIGN_CAP].run_this = NO;
		test_item[BATT_DESIGN_CAP].run_result = RES_PASS;
		return;
	}
	test_item[BATT_DESIGN_CAP].run_result = RES_GOING;
	task_wait_event(10 * MSEC);
}

static void Stress_DesignVolt(void)
{// TODO: BATT_DESIGN Voltage 100,000 cycles testing request from ACER

	static int i, diff_count;
	static int voltage_BAK;
	int voltage;

	if (i++ < 10) {
		sb_read(SB_DESIGN_VOLTAGE, &voltage);
		//ccprintf("Design Voltage= %d,BAK=%d\n",voltage,voltage_BAK);
		if (voltage != voltage_BAK) {
			voltage_BAK = voltage;
			diff_count++;
			if (diff_count >= 2) {
				test_item[BATT_DESIGN_VOLTAGE].run_this = NO;
				test_item[BATT_DESIGN_VOLTAGE].run_result
				= RES_FAIL;
				return;
			}
		}
	} else {
		test_item[BATT_DESIGN_VOLTAGE].run_this = NO;
		test_item[BATT_DESIGN_VOLTAGE].run_result
		= RES_PASS;
		return;
	}

	test_item[BATT_DESIGN_VOLTAGE].run_result = RES_GOING;
	task_wait_event(10 * MSEC);
}

static void Stress_log_CC(void)
{ // Log Battery RSOC 0% ~ 100% real CC , request CC value

	int rsoc, bat_status, sb_chgcurrent;

	static int count, flag;

	sb_read(SB_ABSOLUTE_STATE_OF_CHARGE, &rsoc);

	sb_read(SB_BATTERY_STATUS, &bat_status);

	if (rsoc >= 4 && flag == 0) {
		if (count == 0)
			charger_discharge_on_ac(1); // discharge
				ccprintf("Now discharge RSOC= %d to 0\n\r",
					rsoc);
			ccprintf("SB real AVG Current %d\n\r",
					battery_get_avg_current());
				count = 1;
			cflush();
	} else if (rsoc < 100) {
		flag = 1;
		if (count != 2)
			charger_discharge_on_ac(0); //charge
		sb_read(SB_CHARGING_CURRENT, &sb_chgcurrent);
		ccprintf("SB request charge current %d\n\r",
					sb_chgcurrent);
		ccprintf("SB real AVG Current %d\n\r",
					battery_get_avg_current());
		ccprintf("SB RSOC %d\n\r",
					rsoc);
		count = 2;
			cflush();
	} else if (rsoc == 100) {
		test_item[BATT_LOG_CC].run_this = NO;
		test_item[BATT_LOG_CC].run_result = RES_PASS;
		sb_read(SB_CHARGING_CURRENT, &sb_chgcurrent);
		ccprintf("SB request charge current %d\n\r",
					sb_chgcurrent);
		ccprintf("SB real AVG Current %d\n\r",
					battery_get_avg_current());
		ccprintf("SB RSOC %d\n\r",
					rsoc);
		return;
	}

	test_item[BATT_LOG_CC].run_result = RES_GOING;
	task_wait_event(1000 * MSEC);
}

static void Stress_BST_ALRAM(void)
{
	int rsoc;
	int bat_status;
	static int unlock_result, lock_result, finish;
	static int dischgflag, count;

	sb_read(SB_RELATIVE_STATE_OF_CHARGE, &rsoc);

	ccprintf("RSOC =%d\n\r", rsoc);

	if (rsoc >= 95 && dischgflag == 0) {
		sb_read(SB_BATTERY_STATUS, &bat_status);
		if (count == 0) {
			unlock_result = -1;
			lock_result = -1;
			ccprintf("#1Stress_BST_ALRAM status=0x%04x\n\r",bat_status);
			charger_discharge_on_ac(1); //discharge
		}	
		count = 1;
	} else if (rsoc < 95) {
		sb_read(SB_BATTERY_STATUS, &bat_status);
		ccprintf("#2Stress_BST_ALRAM status=0x%04x\n\r",bat_status);
			if (!(bat_status & STATUS_FULLY_CHARGED)) {
				unlock_result = 0;
				ccprintf("RSOC = %d ,Bat_status STATUS_FULLY_CHARGED has cleared !!\n",rsoc);
			}
			else
				unlock_result = 99;
		charger_discharge_on_ac(0); // charge
		dischgflag = 1;
	} else if (rsoc >= 100) {
		sb_read(SB_BATTERY_STATUS, &bat_status);
		ccprintf("#3Stress_BST_ALRAM status=0x%04x ,rsoc =%d\n\r",bat_status,rsoc);
			if ((bat_status & STATUS_FULLY_CHARGED)) {
				lock_result = 0;
				ccprintf("RSOC = %d ,Bat_status STATUS_FULLY_CHARGED has Set !!\n",rsoc);
			}
			else
				lock_result = 99;
			finish = 1;
	}

	if (finish == 1) {
		ccprintf("#4/n");
		if (lock_result == 0 && unlock_result == 0) {
			test_item[BATT_ALARM].run_this = NO;
			test_item[BATT_ALARM].run_result = RES_PASS;
			ccprintf("FULL charge Flag TEST PASS\n\r");
		} else {
			test_item[BATT_ALARM].run_this = NO;
			test_item[BATT_ALARM].run_result = RES_FAIL;
			ccprintf("FULL charge Flag TEST FAIL\n\r");
			ccprintf("unlock_result =%d ,lock_result =%d\n\r",unlock_result,lock_result);
		}
		return;
	}

	test_item[BATT_ALARM].run_result = RES_GOING;
	task_wait_event(1000 * MSEC);
}

static int get_next_test_item(void)
{
	int i;

	for (i = 0; i < BATT_ITEM_COUNT; ++i) {
		if ((test_item[i].run_this == YES) &&
		    (test_item[i].run_result == RES_GOING))
			return i;
	}

	for (i = 0; i < BATT_ITEM_COUNT; ++i) {
		if ((test_item[i].run_this == YES) &&
		    (test_item[i].run_result == RES_NOT_YET))
			break;
	}

	return i;
}

void DCStress_task_v2(void *u)
{
	int who_is_next = -1;
	int previous_next = -1;

	while (1) {

		who_is_next = get_next_test_item();

		if (who_is_next != previous_next)
			ccprintf("v2 running, who_is_next=%s\n",
				batt_item_names[who_is_next]);

		if ((who_is_next >= BATT_ITEM_COUNT) ||
		    (test_good_to_go != YES))
			task_wait_event(-1);

		switch (who_is_next) {
		case BATT_DEVICE_NAME:
			Strees_DEVICE_NAME();
			break;
		case BATT_MANU_NAME:
			Strees_MANU_NAME();
			break;
		case BATT_DESIGN_CAP:
			Stress_DesignCap();
			break;
		case BATT_DESIGN_VOLTAGE:
			Stress_DesignVolt();
			break;
		case BATT_LOG_CC:
			Stress_log_CC();
			break;
		case BATT_ALARM:
			Stress_BST_ALRAM();
			break;
		default:
			task_wait_event(-1);
			break;
		}

		previous_next = who_is_next;
	}
}

static void show_result(void)
{
	int i;

	ccprintf("----------------------------\n");
	ccprintf("item\t\t\t run?\t\t\tresult\n");
	for (i = 0; i < BATT_ITEM_COUNT; ++i)
		ccprintf("%s%s\t\t\t%s\n"
			, batt_item_names[i]
			, (test_item[i].run_this == YES ? "YES" : "NO")
			, res_text[test_item[i].run_result]);

	ccprintf("\n\n"); cflush();
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

/* Initialization of Battery Test Item */
static void setup_test_config(void)
{
	test_item[BATT_DEVICE_NAME].run_this = YES;
	test_item[BATT_MANU_NAME].run_this = YES;
	test_item[BATT_DESIGN_CAP].run_this = YES;
	test_item[BATT_DESIGN_VOLTAGE].run_this = YES;
	test_item[BATT_LOG_CC].run_this = YES;
	test_item[BATT_ALARM].run_this = YES;
}
DECLARE_HOOK(HOOK_INIT, setup_test_config, HOOK_PRIO_DEFAULT);

static int batt_auto_stress_test_v2(int argc, char **argv)
{
	show_result();

	if (argc > 1 && !strcasecmp(argv[1], "go")) {
		task_wake(TASK_ID_BATTEST2);
		test_good_to_go = YES;
	}

	if (argc > 1 && !strcasecmp(argv[1], "end")) {
		// TODO kill task
		test_good_to_go = NO;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(stresstest, batt_auto_stress_test_v2, NULL, NULL);
