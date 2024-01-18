/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "driver/mp2964.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "throttle_ap.h"
#include "motion_sense.h"
#include "driver/als_veml3328.h"

/* Must come after other header files and interrupt handler declarations */
#include "gpio_list.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

const static struct mp2964_reg_val rail_a[] = {
	{ 0x28, 0x000c }, { 0x29, 0x0002 }, { 0x2c, 0x0384 }, { 0x38, 0x0060 },
	{ 0x3c, 0x00d1 }, { 0x3d, 0x2b01 }, { 0x3f, 0xe883 }, { 0x40, 0x034d },
	{ 0x41, 0x0153 }, { 0x42, 0x014d }, { 0x44, 0x0053 }, { 0x45, 0x0053 },
	{ 0x46, 0x00d0 }, { 0x48, 0x0151 }, { 0x4d, 0xe13f }, { 0x53, 0x0050 },
	{ 0x60, 0x64b0 }, { 0x62, 0x0cb4 }, { 0x96, 0x1e05 }, { 0xd2, 0x00d0 },
	{ 0xd4, 0x0063 }, { 0xd6, 0x003f }, { 0xd8, 0x002d }, { 0xe0, 0x0012 },
	{ 0xe2, 0x00d0 }, { 0xe8, 0x009a }, { 0xe9, 0x009a }, { 0xea, 0x009a },
	{ 0xeb, 0x009a }, { 0xef, 0x00b3 }, { 0xf0, 0x00b3 },
};
const static struct mp2964_reg_val rail_b[] = {
	{ 0x28, 0x000c }, { 0x29, 0x0001 }, { 0x2c, 0x032b }, { 0x38, 0x0038 },
	{ 0x3c, 0x00d1 }, { 0x3d, 0x2b01 }, { 0x3f, 0xe883 }, { 0x40, 0x034d },
	{ 0x41, 0x0153 }, { 0x42, 0x014d }, { 0x44, 0x0053 }, { 0x45, 0x0053 },
	{ 0x46, 0x00d0 }, { 0x4d, 0xe13f }, { 0x53, 0x0028 }, { 0x60, 0x32b0 },
	{ 0x62, 0x0cb4 }, { 0x96, 0x1e05 },
};

/* VEML3328 private data */
struct veml3328_drv_data_t g_veml3328_data = {
    .calib = {
        .per_model = {
            // Lux
            .LG = 1.4143,
            .LC = 0.0,
            .Lh0 = 1.0,
            .Lh1 = 0.0,
            .Ll0 = 1.0,
            .Ll1 = 0.0,
            .Ch_min = 1.0,
            .Ch_max = 1.0,
            .Cl_min = 1.0,
            .Cl_max = 1.0,
            .Jh = 1.0,
            .Jl = 1.0,
            // CCT
            .Lccti0 = 1.0,
            .Lccti1 = 0.0,
            .X1 = 2.0,
            .X2 = 1.0,
            .Y1 = 1.0,
            // xy
            .A0 = 0.1914,
            .A1 = 0.321,
            .A2 = 0.0,
            .B0 = 0.3339,
            .B1 = 0.0873,
            .B2 = 0.0,
            .Dx_min = 0.27,
            .Dx_max = 0.55,
            .Dy_min = 0.1,
            .Dy_max = 0.65
        },
        .per_system = {
            // Lux
            .C_lux = 1.0,
            // CCT
            .C_ccti = 1.0,
            // xy
            .Cx0 = 0.0,
            .Cx1 = 0.0,
            .Cx2 = 0.0,
            .Cy0 = 0.0,
            .Cy1 = 0.0,
            .Cy2 = 0.0
        }
    },
};

struct motion_sensor_t motion_sensors[] = {
	[BASE_ALS] = {
		.name = "Light",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_VEML3328,
		.type = MOTIONSENSE_TYPE_LIGHT,
		.location = MOTIONSENSE_LOC_LID,
		.drv = &veml3328_drv,
		.drv_data = &g_veml3328_data,
		.port = I2C_PORT_SENSOR,
		.i2c_spi_addr_flags = VEML3328_I2C_ADDR,
		.rot_standard_ref = NULL,
		.default_range = 65535,
		.min_frequency = VEML3328_MIN_FREQ,
		.max_frequency = VEML3328_MAX_FREQ,
		.config = {
			[SENSOR_CONFIG_EC_S0] = {
				.odr = VEML3328_10000_MHZ,
			},
		},
	},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

/* ALS instances when LPC mapping is needed. Each entry directs to a sensor. */
const struct motion_sensor_t *motion_als_sensors[] = {
	&motion_sensors[BASE_ALS],
};
BUILD_ASSERT(ARRAY_SIZE(motion_als_sensors) == ALS_COUNT);

static void mp2964_on_startup(void)
{
	static int chip_updated;
	int status;

	if (chip_updated)
		return;

	CPRINTF("[mp2964] attempting to tune MP2964\n");

	status = mp2964_tune(rail_a, ARRAY_SIZE(rail_a), rail_b,
			     ARRAY_SIZE(rail_b));

	if (status == EC_SUCCESS) {
		chip_updated = 1;
		CPRINTF("[mp2964] mp2964 is already updated\n");
	} else
		CPRINTF("[mp2964] try to tune MP2964 (%d)\n", status);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, mp2964_on_startup, HOOK_PRIO_FIRST);
