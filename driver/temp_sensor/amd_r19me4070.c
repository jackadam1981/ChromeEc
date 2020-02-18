/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* R19ME4070 temperature sensor module for Chrome EC */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "tablet_mode.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "util.h"
#include "i2c.h"
#include "power.h"
#include "amd_r19me4070.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static int gpu_init_flag;
static int temp_value;
static int temp;
static int read_gpu_flag;

static int read_gpu_temp(int tmp)
{
	return i2c_read32(I2C_PORT_GPU, GPU_ADDR_FLAGS,
	GPU_TEMPERATURE_OFFSET, &tmp);
}

static int init_gpu(void)
{
	return i2c_write32(I2C_PORT_GPU, GPU_ADDR_FLAGS,
	GPU_INIT_OFFSET, GPU_INIT_WRITE_VALUE);
}

void gpu_init(void)
{
	if (init_gpu() == EC_SUCCESS)
		gpu_init_flag = 1;
	else {
		gpu_init_flag = 0;
		CPRINTS("init GPU fail");
	}
}

/* INIT GPU first before read the GPU's die tmeperature. */
int get_temp_R19M14017(int idx, int *temp_ptr)
{
	/* read GPU temperature in S0. if not in S0, clear init flag. */
	if ((power_get_state()) != POWER_S0) {
		gpu_init_flag = 0;
		return EC_ERROR_BUSY;
	}
	/* if no INIT GPU, must init it first and wait 1 sec. */
	if (gpu_init_flag == 0) {
		gpu_init();
		return EC_ERROR_UNIMPLEMENTED;
	}
	if (read_gpu_temp(temp) == EC_SUCCESS) {
		/*
		 * for the four-byte read back, bit[17:9] is
		 * represent GPU temperature.
		 * 0x000 : 0	ﾟC
		 * 0x001 : 1	ﾟC
		 * 0x002 : 2	ﾟC
		 * ...
		 * 0x1FF : 511	ﾟC
		 */
		temp_value = C_TO_K((temp >> 9) && (0x1ff));
		read_gpu_flag = 1;
	} else
		CPRINTS("read GPU Temperature fail");
	if (read_gpu_flag) {
		*temp_ptr = temp_value;
		/* hand over temperature value to *tmp_ptr ,clear flag. */
		read_gpu_flag = 0;
		return EC_SUCCESS;
	}
	return EC_ERROR_UNKNOWN;
}
