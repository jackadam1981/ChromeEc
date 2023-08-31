/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "ec_i2c.h"
#include "ap_ec_i2c_config.h"

/** 
 * Reads and returns the 16-bit value from a register `addr`
 */
uint16_t read_reg(uint8_t addr)
{
	return 0;
}

/**
 * @param addr		
 * @param data		
 * @param mask		bits in register to set
 */
void write_reg(uint8_t addr, uint16_t data, uint16_t mask)
{
	return;
}

/**
 * Sets GPIO `pin` to the specified `val`
 */
void gpio_set_level(int pin, bool val)
{
	return;
}

/**
 * @return		0 if lid interrupt disabled, else 1
 */
bool is_lid_state_needed(void)
{
	return read_reg(CONTROL_REG) & 0b01;
}

 /**
  * Pulls default high INT line low
  *
  * @param val		value to set interrupt line to
  */
void trigger_interrupt(bool val)
{
	gpio_set_level(EC_ISH_INT, val);
}

enum lid_sensor_val {LID_OPEN, LID_CLOSE};

/** 
 * Reads lid sensor value
 *
 * @return		0: lid open, 1: lid closed
 */
bool read_lid_sensor_value(void)
{
	return 0;
}

enum tablet_mode {LAPTOP_MODE, TABLET_MODE};

/** 
 * Reads tablet mode sensor value
 *
 * @return		0: laptop mode, 1: tablet mode
 */
bool read_tablet_sensor_value(void)
{
	return 0;
}

/**
 * Interrupt service outine to hook into task priority table
 */
static void lid_state_ish(void)
{
	if (is_lid_state_needed()) {
		int lid_sensor_val = read_lid_sensor_value();
		write_reg(STATUS_REG, lid_sensor_val, 0x0001);
		trigger_interrupt(0);
	}
}
// todo, get rid of this and do it properly
#define DECLARE_HOOK(HOOK_ID, func, HOOK_PRIORITY) (func)
#define HOOK_PRIORITY_FIRST 0
DECLARE_HOOK(HOOK_LID_CHANGE, lid_state_ish, HOOK_PRIORITY_FIRST);

// TODO: AP needs to have a thread monitoring the INT line and 
// read the status register upon an interrupt being fired, 
// and then clear the register.
// after the register is read the EC needs to raise the INT line back to 1
void is_status_reg_read(void)
{
    trigger_interrupt(1);
}