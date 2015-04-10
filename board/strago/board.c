/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Strago board-specific configuration */

#include "driver/sar_sx9310.h"
#include "extpower.h"
#include "gpio.h"
#include "i2c.h"
#include "include/sar.h"
#include "include/sar_sense.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#define GPIO_KB_INPUT (GPIO_INPUT | GPIO_PULL_UP)
#define GPIO_KB_OUTPUT (GPIO_ODR_HIGH)
#define GPIO_KB_OUTPUT_COL2 (GPIO_OUT_LOW)

#include "gpio_list.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_ALL_SYS_PGOOD,     1, "ALL_SYS_PWRGD"},
	{GPIO_RSMRST_L_PGOOD,    1, "RSMRST_N_PWRGD"},
	{GPIO_PCH_SLP_S3_L,      1, "SLP_S3#_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,      1, "SLP_S4#_DEASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

const struct i2c_port_t i2c_ports[]  = {
	{"batt_chg",	0, 100},
	{"sensors",	1, 100},
	{"pd_mcu",	2, 100},
	{"thermal",	3, 100}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
static struct mutex g_sar_mutex;
struct sar_sensor_t sar_sensors[] = {
	{SENSOR_ACTIVE_S0, "LTE Sar", SENSOR_CHIP_SX9310,
		&sx9310_drv, &g_sar_mutex, NULL,
		SX9310_ADDR, 1024, 160},
};
const unsigned int sar_sensor_count = ARRAY_SIZE(sar_sensors);
