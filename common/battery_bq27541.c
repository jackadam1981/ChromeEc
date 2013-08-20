/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery driver for BQ27541.
 */

#include "battery.h"
#include "console.h"
#include "i2c.h"
#include "util.h"

#define BQ27541_ADDR                0xaa
#define BQ27541_TYPE_ID             0x0541

#define REG_CTRL                    0x00
#define REG_AT_RATE                 0x02
#define REG_AT_RATE_TIME_TO_EMPTY   0x04
#define REG_TEMPERATURE             0x06
#define REG_VOLTAGE                 0x08
#define REG_FLAGS                   0x0a
#define REG_NOMINAL_CAPACITY        0x0c
#define REG_FULL_AVAILABLE_CAPACITY 0x0e
#define REG_REMAINING_CAPACITY      0x10
#define REG_FULL_CHARGE_CAPACITY    0x12
#define REG_AVERAGE_CURRENT         0x14
#define REG_TIME_TO_EMPTY           0x16
#define REG_TIME_TO_FULL            0x18
#define REG_STANDBY_CURRENT         0x1a
#define REG_STANDBY_TIME_TO_EMPTY   0x1c
#define REG_MAX_LOAD_CURRENT        0x1e
#define REG_MAX_LOAD_TIME_TO_EMPTY  0x20
#define REG_AVAILABLE_ENERGY        0x22
#define REG_AVERAGE_POEWR           0x24
#define REG_TT_EAT_CONSTANT_POWER   0x26
#define REG_CYCLE_COUNT             0x2a
#define REG_STATE_OF_CHARGE         0x2c
#define REG_DESIGN_CAPACITY         0x3c
#define REG_DEVICE_NAME             0x63
#define REG_DEVICE_NAME_LENGTH      7

static int bq27541_read(int offset, int *data)
{
	return i2c_read16(I2C_PORT_HOST, BQ27541_ADDR, offset, data);
}

static int bq27541_read8(int offset, int *data)
{
	return i2c_read8(I2C_PORT_HOST, BQ27541_ADDR, offset, data);
}

static int bq27541_write(int offset, int data)
{
	return i2c_write16(I2C_PORT_HOST, BQ27541_ADDR, offset, data);
}

int bq27541_probe(void)
{
	int rv;
	int dev_type;

	rv = bq27541_write(REG_CTRL, 0x1);
	rv |= bq27541_read(REG_CTRL, &dev_type);

	if (rv)
		return rv;
	return (dev_type == BQ27541_TYPE_ID) ? EC_SUCCESS : EC_ERROR_UNKNOWN;
}

int battery_device_name(char *device_name, int buf_size)
{
	int rv = EC_SUCCESS;
	int len = MIN(REG_DEVICE_NAME_LENGTH, buf_size - 1);
	int i;
	int val;

	for (i = 0; i < len; ++i) {
		rv |= bq27541_read8(REG_DEVICE_NAME + i, &val);
		device_name[i] = val;
	}
	device_name[i] = '\0';

	return rv;
}

int battery_temperature(int *deci_kelvin)
{
	return bq27541_read(REG_TEMPERATURE, deci_kelvin);
}

int battery_voltage(int *voltage)
{
	return bq27541_read(REG_VOLTAGE, voltage);
}

int battery_state_of_charge(int *percent)
{
	return bq27541_read(REG_STATE_OF_CHARGE, percent);
}

int battery_state_of_charge_abs(int *percent)
{
	return battery_state_of_charge(percent);
}

int battery_remaining_capacity(int *capacity)
{
	return bq27541_read(REG_REMAINING_CAPACITY, capacity);
}

int battery_full_charge_capacity(int *capacity)
{
	return bq27541_read(REG_FULL_CHARGE_CAPACITY, capacity);
}

int battery_time_to_empty(int *minutes)
{
	return bq27541_read(REG_TIME_TO_EMPTY, minutes);
}

int battery_time_to_full(int *minutes)
{
	return bq27541_read(REG_TIME_TO_FULL, minutes);
}

int battery_cycle_count(int *count)
{
	return bq27541_read(REG_CYCLE_COUNT, count);
}

int battery_design_capacity(int *capacity)
{
	return bq27541_read(REG_DESIGN_CAPACITY, capacity);
}

int battery_average_current(int *current)
{
	return bq27541_read(REG_AVERAGE_CURRENT, current);
}

int battery_time_at_rate(int rate, int *minutes)
{
	int rv;

	rv = bq27541_write(REG_AT_RATE, rate);
	if (rv)
		return rv;
	return bq27541_read(REG_AT_RATE_TIME_TO_EMPTY, minutes);
}

/*****************************************************************************/
/* Console commands */

static int print_battery_info(void)
{
	int value;
	int hour, minute;
	char text[32];
	int rv;

	rv = battery_temperature(&value);
	if (rv)
		return rv;

	ccprintf("  Temp:      0x%04x = %.1d K (%.1d C)\n",
		 value, value, value - 2731);
#if 0
	ccprintf("  Manuf:     %s\n",
		 battery_manufacturer_name(text, sizeof(text)) == EC_SUCCESS ?
		 text : "(error)");
#endif
	ccprintf("  Device:    %s\n",
		 battery_device_name(text, sizeof(text)) == EC_SUCCESS ?
		 text : "(error)");
#if 0
	ccprintf("  Chem:      %s\n",
		 battery_device_chemistry(text, sizeof(text)) == EC_SUCCESS ?
		 text : "(error)");

	battery_serial_number(&value);
	ccprintf("  Serial:    0x%04x\n", value);
#endif
	battery_voltage(&value);
	ccprintf("  V:         0x%04x = %d mV\n", value, value);
#if 0
	battery_desired_voltage(&value);
	ccprintf("  V-desired: 0x%04x = %d mV\n", value, value);

	battery_design_voltage(&value);
	ccprintf("  V-design:  0x%04x = %d mV\n", value, value);
#endif
	battery_average_current(&value);
	ccprintf("  Avg-I:     0x%04x = %d mA",
		value & 0xffff, value);
	if (value > 0)
		ccputs("(CHG)");
	else if (value < 0)
		ccputs("(DISCHG)");
	ccputs("\n");

#if 0
	battery_desired_current(&value);
	ccprintf("  I-desired: 0x%04x = %d mA\n", value, value);

	battery_get_battery_mode(&value);
	ccprintf("  Mode:      0x%04x\n", value);
	unit = (value & MODE_CAPACITY) ? "0 mW" : " mAh";
#endif
	battery_state_of_charge(&value);
	ccprintf("  Charge:    %d %%\n", value);

	battery_remaining_capacity(&value);
	ccprintf("  Remaining: %d mAh\n", value);

	battery_full_charge_capacity(&value);
	ccprintf("  Cap-full:  %d mAh\n", value);

	battery_design_capacity(&value);
	ccprintf("    Design:  %d mAh\n", value);

	battery_time_to_full(&value);
	if (value == 65535) {
		hour   = 0;
		minute = 0;
	} else {
		hour   = value / 60;
		minute = value % 60;
	}
	ccprintf("  Time-full: %dh:%d\n", hour, minute);

	battery_time_to_empty(&value);
	if (value == 65535) {
		hour   = 0;
		minute = 0;
	} else {
		hour   = value / 60;
		minute = value % 60;
	}
	ccprintf("    Empty:   %dh:%d\n", hour, minute);

	return 0;
}

static int command_battery(int argc, char **argv)
{
	int repeat = 1;
	int rv = 0;
	int loop;
	char *e;

	if (argc > 1) {
		repeat = strtoi(argv[1], &e, 0);
		if (*e) {
			ccputs("Invalid repeat count\n");
			return EC_ERROR_INVAL;
		}
	}

	for (loop = 0; loop < repeat; loop++)
		rv = print_battery_info();

	if (rv)
		ccprintf("Failed - error %d\n", rv);

	return rv ? EC_ERROR_UNKNOWN : EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(battery, command_battery,
			"<repeat_count>",
			"Print battery info",
			NULL);
