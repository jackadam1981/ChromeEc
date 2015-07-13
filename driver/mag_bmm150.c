/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMM150 compass behing a BMI160
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/mag_bmm150.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)


/****************************************************************************
* Copyright (C) 2011 - 2014 Bosch Sensortec GmbH
*
****************************************************************************/
/***************************************************************************
* License:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   Redistributions of source code must retain the above copyright
*   notice, this list of conditions and the following disclaimer.
*
*   Redistributions in binary form must reproduce the above copyright
*   notice, this list of conditions and the following disclaimer in the
*   documentation and/or other materials provided with the distribution.
*
*   Neither the name of the copyright holder nor the names of the
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*
* The information provided is believed to be accurate and reliable.
* The copyright holder assumes no responsibility for the consequences of use
* of such information nor for any infringement of patents or
* other rights of third parties which may result from its use.
* No license is granted by implication or otherwise under any patent or
* patent rights of the copyright holder.
*/

#include "mag_bmm150.h"

#define BMI150_READ_16BIT_COM_REG(store_, addr_) do { \
	int val; \
	raw_mag_read8(s->i2c_addr, (addr_), &val); \
	store_ = val; \
	raw_mag_read8(s->i2c_addr, (addr_) + 1, &val); \
	store_ |= (val << 8); \
} while (0)


int bmm150_init(const struct motion_sensor_t *s)
{
	int ret;
	int val;
	struct bmm150_comp_registers *regs = BMM150_COMP_REG(s);

	/* Set the compass from Suspend to Sleep */
	ret = raw_mag_write8(s->i2c_addr, BMM150_PWR_CTRL, BMM150_PWR_ON);
	/* Now we can read the device id */
	ret = raw_mag_read8(s->i2c_addr, BMM150_CHIP_ID, &val);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (val != BMM150_CHIP_ID_MAJOR)
		return EC_ERROR_ACCESS_DENIED;

	/* Read the private registers for compensation */
	ret = raw_mag_read8(s->i2c_addr, BMM150_REGA_DIG_X1, &val);
	if (ret)
		return EC_ERROR_UNKNOWN;
	regs->dig_x1 = val;
	raw_mag_read8(s->i2c_addr, BMM150_REGA_DIG_Y1, &val);
	regs->dig_x2 = val;
	raw_mag_read8(s->i2c_addr, BMM150_REGA_DIG_X2, &val);
	regs->dig_y1 = val;
	raw_mag_read8(s->i2c_addr, BMM150_REGA_DIG_Y2, &val);
	regs->dig_y2 = val;

	raw_mag_read8(s->i2c_addr, BMM150_REGA_DIG_XY1, &val);
	regs->dig_xy1 = val;

	raw_mag_read8(s->i2c_addr, BMM150_REGA_DIG_XY2, &val);
	regs->dig_xy2 = val;

	BMI150_READ_16BIT_COM_REG(regs->dig_z1, BMM150_REGA_DIG_Z1_LSB);
	BMI150_READ_16BIT_COM_REG(regs->dig_z2, BMM150_REGA_DIG_Z2_LSB);
	BMI150_READ_16BIT_COM_REG(regs->dig_z3, BMM150_REGA_DIG_Z3_LSB);
	BMI150_READ_16BIT_COM_REG(regs->dig_z4, BMM150_REGA_DIG_Z4_LSB);
	BMI150_READ_16BIT_COM_REG(regs->dig_xyz1, BMM150_REGA_DIG_XYZ1_LSB);

	/*
	 * Set the compass forced mode, to sleep after each measure.
	 */
	ret = raw_mag_write8(s->i2c_addr, BMM150_OP_CTRL,
			BMM150_OP_MODE_FORCED << BMM150_OP_MODE_OFFSET);

	return ret;
}

void bmm150_normalize(const struct motion_sensor_t *s,
		      vector_3_t v,
		      uint8_t *data)
{

	v[X] = ((int16_t)(data[0] | (data[1] << 8))) >> 3;
	v[Y] = ((int16_t)(data[2] | (data[3] << 8))) >> 3;
	v[Z] = ((int16_t)(data[4] | (data[5] << 8))) >> 1;

#if 0
	uint16_t        r;

	int16_t         tc_x;
	int16_t         tc_y;
	int32_t         tc_z;
	r = (data[6] | (data[7] << 8)) >> 2;

	tc_x = bmm150_temp_compensate_x(dev, v[X], r);
	if (BMM150_OVERFLOW_OUTPUT != tc_x)
		data->x = tc_x;
	else
		data->x = MM150_OVERFLOW;

	tc_y = bmm150_temp_compensate_y(dev, adc_y, r);
	if (BMM150_OVERFLOW_OUTPUT != tc_y)
		data->y = tc_y;
	else
		data->y = BMM150_OVERFLOW;

	tc_z = bmm150_temp_compensate_z_s32(dev, adc_z, r);
	if (BMM150_OVERFLOW_OUTPUT_S32 != tc_z)
		data->z = tc_z;
	else
		data->y = BMM150_OVERFLOW;
#endif
}

