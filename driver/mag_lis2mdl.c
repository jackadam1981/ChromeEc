/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LIS2MDL accelerometer module for Chrome EC.
 * This driver manage Mag as stand alone device, not cascaded with other
 * device (on I2C master interface)
 */

#include "common.h"
#include "hooks.h"
#include "math_util.h"
#include "task.h"
#include "util.h"
#include "driver/mag_lis2mdl.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

#ifdef CONFIG_MAG_BMI160_LIS2MDL
#include "driver/accelgyro_bmi160.h"
#define raw_mag_read8 bmi160_sec_raw_read8
#define raw_mag_write8 bmi160_sec_raw_write8
#else
#error "Not tested."
#define raw_mag_read8 i2c_read8
#define raw_mag_write8 i2c_write8
#endif

void lis2mdl_normalize(const struct motion_sensor_t *s,
		       intv3_t v,
		       uint8_t *data)
{
	struct mag_cal_t *cal = LIS2MDL_CAL(s);
#ifdef CONFIG_MAG_BMI160_LIS2MDL
	/*
	 * When behind another MEMS, we use single shot mode and must do the
	 * offset compensation in driver.
	 */
	struct lis2mdl_private_data *private = LIS2MDL_DATA(s);
	int i;
	intv3_t hn1;

	hn1[X] = ((int16_t)((data[1] << 8) | data[0]));
	hn1[Y] = ((int16_t)((data[3] << 8) | data[2]));
	hn1[Z] = ((int16_t)((data[5] << 8) | data[4]));

	if (private->hn_valid) {
		for (i = X; i <= Z; i++)
			v[i] = (hn1[i] + private->hn[i]) / 2;
		memcpy(private->hn, hn1, sizeof(intv3_t));
	} else {
		private->hn_valid = 1;
		memcpy(v, hn1, sizeof(intv3_t));
	}
#else
	/* Offset compensation already done in hardware. */
	v[X] = ((int16_t)((data[1] << 8) | data[0]));
	v[Y] = ((int16_t)((data[3] << 8) | data[2]));
	v[Z] = ((int16_t)((data[5] << 8) | data[4]));
#endif
	mag_cal_update(cal, v);
}

/**
 * init - Init mag in case of stand alone solution
 * @s: Motion sensor pointer
 */
int lis2mdl_init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp, timeout = 0, status;
#ifndef CONFIG_MAG_BMI160_LIS2MDL
	struct stprivate_data *data = s->drv_data;
#endif

	ret = raw_mag_read8(s->port, s->addr, LIS2MDL_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LIS2MDL_WHOAMI_VAL)
		return EC_ERROR_ACCESS_DENIED;

	/*
	 * This sensor can be powered through an EC reboot, so the state of
	 * the sensor is unknown here. Initiate software reset to restore
	 * sensor to default.
	 */
	mutex_lock(s->mutex);

	/* Reset component. */
	ret = raw_mag_write8(s->port, s->addr, LIS2MDL_CFG_REG_A,
			     LIS2MDL_SOFT_RST);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Check End of Reset (should be less than 11 ms in any case). */
	do {
		if (timeout > 15) {
			ret = EC_RES_TIMEOUT;
			goto err_unlock;
		}

		msleep(5);
		timeout += 5;
		ret = raw_mag_read8(s->port, s->addr, LIS2MDL_CFG_REG_A,
				    &status);
		if (ret != EC_SUCCESS)
			continue;
	} while ((status & LIS2MDL_REBOOT) != 0);


#ifndef CONFIG_MAG_BMI160_LIS2MDL
	/* Set sensor resolution in bit. */
	data->resol = LIS2MDL_RESOLUTION;

	/* Enable BDU. */
	raw_mag_write8(s->port, s->addr, LIS2MDL_CFG_REG_C, LIS2MDL_BDU);

	/* Offset compensation */
	raw_mag_write8(s->port, s->addr, LIS2MDL_CFG_REG_B, LIS2MDL_OFF_CANC);

	/* Set continuous MODE, Temperature Compensation. */
	ret = raw_mag_write8(s->port, s->addr, LIS2MDL_CFG_REG_A,
			     LIS2MDL_COMP_TEMP_EN | LIS2MDL_MD_CONTINUOUS_MODE);
#else
	/* Enable offset compensation in single shot mode. */
	raw_mag_write8(s->port, s->addr, LIS2MDL_CFG_REG_B,
		       LIS2MDL_OFF_CANC | LIS2MDL_OFF_CANC_ONE_SHOT);

	raw_mag_write8(s->port, s->addr, LIS2MDL_INT_CRTL_REG, 0);

	/*
	 * Set single mode: direct write for telling BMI160 how to enter force
	 * mode.
	 * This is the last write access to the magnetometer before BMI160
	 * takes over.
	 */
	ret = raw_mag_write8(s->port, s->addr, LIS2MDL_CFG_REG_A,
			     LIS2MDL_COMP_TEMP_EN | LIS2MDL_MD_SINGLE_MODE);
#endif
	if (ret != EC_SUCCESS)
		goto err_unlock;

err_unlock:
	mutex_unlock(s->mutex);

	return ret;
}
