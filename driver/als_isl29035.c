/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Intersil ILS29035 light sensor driver
 */

#include "als.h"
#include "accelgyro.h"
#include "console.h"
#include "driver/als_isl29035.h"
#include "i2c.h"

#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/* I2C interface */
static inline int isl29035_i2c_read(const int port, const int addr,
				    const int reg, int *data_ptr)
{
	return i2c_read8(port, addr, reg, data_ptr);
}

static inline int isl29035_i2c_write(const int port, const int addr,
				     const int reg, int data)
{
	return i2c_write8(port, addr, reg, data);
}

static int isl29035_init(const struct motion_sensor_t *s)
{
	/*
	 * Tell it to read continually. This uses 70uA, as opposed to nearly
	 * zero, but it makes the hook/update code cleaner (we don't want to
	 * wait 90ms to read on demand while processing hook callbacks).
	 */
	return isl29035_i2c_write(s->port, s->addr,
				  ILS29035_REG_COMMAND_I, 0xa0);
}

static int isl29035_read_lux(const struct motion_sensor_t *s, vector_3_t v)
{
	int rv, lsb, msb, data;
	struct isl29035_drv_data_t *drv_data =
		(struct isl29035_drv_data_t *) s->drv_data;

	/*
	 * NOTE: It is necessary to read the LSB first, then the MSB. If you do
	 * it in the opposite order, the results are not correct. This is
	 * apparently an undocumented "feature". It's especially noticeable in
	 * one-shot mode.
	 */

	/* Read lsb */
	rv = isl29035_i2c_read(s->port, s->addr, ILS29035_REG_DATA_LSB, &lsb);
	if (rv)
		return rv;

	/* Read msb */
	rv = isl29035_i2c_read(s->port, s->addr, ILS29035_REG_DATA_LSB, &msb);
	if (rv)
		return rv;

	data = (msb << 8) | lsb;

	/*
	 * The default power-on values will give 16 bits of precision:
	 * 0x0000-0xffff indicates 0-1000 lux. We multiply the sensor value by
	 * a scaling factor to account for attentuation by glass, tinting, etc.
	 *
	 * Caution: Don't go nuts with the attentuation factor. If it's
	 * greater than 32, the signed int math will roll over and you'll get
	 * very wrong results. Of course, if you have that much attenuation and
	 * are still getting useful readings, you probably have your sensor
	 * pointed directly into the sun.
	 */
	v[0] = (int) ((uint64_t)
			(data * drv_data->attenuation_factor * 1000) / 0xffff);
	v[1] = 0;
	v[2] = 0;

	return EC_SUCCESS;
}

#ifdef HAS_TASK_ALS
const struct als_driver isl29035_drv = {
	.name = "ISL29035",
	.port = I2C_PORT_ALS,
	.addr = ILS29035_I2C_ADDR,
	.init = &isl29035_init,
	.read = &isl29035_read_lux,
};
#else
static int isl29035_set_range(const struct motion_sensor_t *s, int range,
			      int rnd)
{
	return EC_SUCCESS;
}

static int isl29035_get_range(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

static int isl29035_set_data_rate(const struct motion_sensor_t *s,
				int rate, int roundup)
{
	return EC_SUCCESS;
}

static int isl29035_get_data_rate(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

const struct accelgyro_drv isl29035_drv = {
	.init = isl29035_init,
	.read = isl29035_read_lux,
	.set_range = isl29035_set_range,
	.get_range = isl29035_get_range,
	.set_data_rate = isl29035_set_data_rate,
	.get_data_rate = isl29035_get_data_rate,
};
#endif

struct isl29035_drv_data_t g_isl29035_data = {
	.attenuation_factor = 1,
};
