/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * sx9310 Specific Absorbtion Rate module for Chrome EC
 * SAR sensor.
 */

#include "common.h"
#include "console.h"
#include "driver/sar_sx9310.h"
#include "hooks.h"
#include "i2c.h"
#include "sar.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

struct sar_ctrl {
	int reg;
	int reg_val;
};

static const struct sar_ctrl g_prox_ctrl[] = {
	{SX9310_CPS_CTRL_REG0, 0x2f}, /* 0x10 */
	{SX9310_CPS_CTRL_REG1, 0x00},
	{SX9310_CPS_CTRL_REG2, 0x84},
	{SX9310_CPS_CTRL_REG3, 0x0f},
	{SX9310_CPS_CTRL_REG4, 0x0d},
	{SX9310_CPS_CTRL_REG5, 0xc1},
	{SX9310_CPS_CTRL_REG6, 0x20},
	{SX9310_CPS_CTRL_REG7, 0x4c},
	{SX9310_CPS_CTRL_REG8, 0xad},
	{SX9310_CPS_CTRL_REG9, 0xad},
	{SX9310_CPS_CTRL_REG10, 0x10},
	{SX9310_CPS_CTRL_REG11, 0x00},
	{SX9310_CPS_CTRL_REG12, 0x00},
	{SX9310_CPS_CTRL_REG13, 0x00},
	{SX9310_CPS_CTRL_REG14, 0x00},
	{SX9310_CPS_CTRL_REG15, 0x00},
	{SX9310_CPS_CTRL_REG16, 0x00},
	{SX9310_CPS_CTRL_REG17, 0x00},
	{SX9310_CPS_CTRL_REG18, 0x00},
	{SX9310_CPS_CTRL_REG19, 0x00},/* 0x23 */
};

static const struct sar_ctrl g_sar_ctrl[] = {
	{SX9310_SAR_CTRL_REG0, 0x88}, /* 0x2A */
	{SX9310_SAR_CTRL_REG1, 0x76},
	{SX9310_SAR_CTRL_REG2, 0x41}, /* 0x2C */
};

/* Index number is the register value. */
static const int g_proxthresh[] = {
	2, 4, 6, 8, 12, 16, 20, 24,
	28, 32, 40, 48, 56, 64, 72, 80,
	88, 96, 112, 128, 144, 160, 192, 224,
	256, 320, 384, 512, 640, 768, 1024, 1536,
};

/* Reversed index number of the scan frequencies. */
static const int g_scan_freq[] = {
	15100, 15600, 16100, 16700,
	17200, 17900, 18500, 19200,
	20000, 20800, 21700, 22700,
	23800, 25000, 26300, 27800,
	29400, 31300, 33300, 35700,
	38500, 41700, 45500, 50000,
	55600, 62500, 71400, 83000,
	100000, 125000, 166700, 250000,
};

static const int g_resolution[] = {32, 64, 91, 128, 181, 256, 512, 1024};

static int get_reg_val(const int eng_val, const int round_up,
		const int *config, const int size)
{
	int i;
	for (i = 0; i < size - 1; i++) {
		if (eng_val <= config[i])
			break;

		if (eng_val < config[i + 1]) {
			if (round_up)
				i += 1;
			break;
		}
	}
	return i;
}

/**
 * Read register from SAR sensor.
 */
static inline int raw_read8(const int addr, const int reg, int *data_ptr)
{
	return i2c_read8(I2C_PORT_SAR, addr, reg, data_ptr);
}

/**
 * Write register to SAR sensor.
 */
static inline int raw_write8(const int addr, const int reg, int data)
{
	return i2c_write8(I2C_PORT_SAR, addr, reg, data);
}

static int set_sensitivity(const struct sar_sensor_t *s,
	int type, int eng_val, int rnd)
{
	int ret = 0, tmp;
	int sens = get_reg_val(eng_val, rnd, g_proxthresh,
			ARRAY_SIZE(g_proxthresh));

	mutex_lock(s->mutex);
	ret = raw_read8(s->i2c_addr, SX9310_CPS_CTRL_REG8 + type, &tmp);
	if (ret) {
		mutex_unlock(s->mutex);
		return EC_ERROR_UNKNOWN;
	}
	tmp &= 7;
	tmp |= (sens << 3);
	ret = raw_write8(s->i2c_addr, SX9310_CPS_CTRL_REG8 + type, tmp);
	mutex_unlock(s->mutex);

	return EC_SUCCESS;
}

static int get_sensitivity(struct sar_sensor_t *s,
	int type, int *sens)
{
	int ret = 0, tmp;
	ret = raw_read8(s->i2c_addr, SX9310_CPS_CTRL_REG8 + type, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;
	tmp >>= 3;
	(*sens) = g_proxthresh[tmp];

	return EC_SUCCESS;
}

static int set_interrupt(const struct sar_sensor_t *s, unsigned int threshold)
{
	int ret = 0, tmp;

	mutex_lock(s->mutex);

	/* We are enabling all interrupts. */
	tmp = 0xff;
	ret = raw_write8(s->i2c_addr, SX9310_IRQ_ENABLE_REG, tmp);
	if (ret) {
		mutex_unlock(s->mutex);
		return EC_ERROR_UNKNOWN;
	}

	ret = raw_read8(s->i2c_addr, SX9310_IRQFUNC_REG, &tmp);
	if (ret) {
		mutex_unlock(s->mutex);
		return EC_ERROR_UNKNOWN;
	}

	tmp |= SX9310_IRQ_POLARITY_INV | SX9310_IRQ_PROXSTATANY;

	ret = raw_write8(s->i2c_addr, SX9310_IRQFUNC_REG, tmp);
	if (ret) {
		mutex_unlock(s->mutex);
		return EC_ERROR_UNKNOWN;
	}

	mutex_unlock(s->mutex);

	return EC_SUCCESS;
}

static int set_resolution(const struct sar_sensor_t *s, int res, int rnd)
{
	int ret = 0, tmp;
	res = get_reg_val(res, rnd, g_resolution,
			ARRAY_SIZE(g_resolution));

	mutex_lock(s->mutex);
	ret = raw_read8(s->i2c_addr, SX9310_CPS_CTRL_REG4, &tmp);
	if (ret) {
		mutex_unlock(s->mutex);
		return EC_ERROR_UNKNOWN;
	}
	tmp &= ~7;
	tmp |= (res & 7);
	ret = raw_write8(s->i2c_addr, SX9310_CPS_CTRL_REG4, tmp);
	mutex_unlock(s->mutex);

	return EC_SUCCESS;
}

static int get_resolution(const struct sar_sensor_t *s, int *res)
{
	int ret = 0, tmp;

	ret = raw_read8(s->i2c_addr, SX9310_CPS_CTRL_REG4, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;
	tmp &= 7;
	*res = g_resolution[tmp];

	return EC_SUCCESS;
}

static int set_scanfreq(const struct sar_sensor_t *s, int freq, int rnd)
{
	int ret = 0, tmp, regval;

	regval = get_reg_val(freq, rnd, g_scan_freq,
			ARRAY_SIZE(g_scan_freq));
	mutex_lock(s->mutex);
	ret = raw_read8(s->i2c_addr, SX9310_CPS_CTRL_REG4, &tmp);
	if (ret) {
		mutex_unlock(s->mutex);
		return EC_ERROR_UNKNOWN;
	}
	tmp &= 7;
	/*
	* Scan frequencies have been placed in a reversed order
	* therefore we need to calculate exact register value.
	*/
	regval = ARRAY_SIZE(g_scan_freq) - regval - 1;
	tmp |= regval << 3;
	ret = raw_write8(s->i2c_addr, SX9310_CPS_CTRL_REG4, tmp);
	mutex_unlock(s->mutex);

	return EC_SUCCESS;
}

static int get_scanfreq(const struct sar_sensor_t *s, int *freq)
{
	int ret = 0, tmp;

	ret = raw_read8(s->i2c_addr, SX9310_CPS_CTRL_REG4, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	tmp = (tmp & 0xff) >> 3;
	/*
	* Scan frequencies have been placed in a reversed order
	* therefore we need to calculate exact register value.
	*/
	tmp = ARRAY_SIZE(g_scan_freq) - tmp - 1;
	*freq = g_scan_freq[tmp];

	return EC_SUCCESS;
}

static int read(const struct sar_sensor_t *s, int *status, int *irq_src)
{
	int ret = 0;
	uint8_t data;

	uint8_t reg = SX9310_STAT1_REG;

	i2c_lock(I2C_PORT_SAR, 1);
	ret = i2c_xfer(I2C_PORT_SAR, s->i2c_addr,
			&reg, 1, &data, 1, I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_SAR, 0);
	if (ret != EC_SUCCESS)
		CPRINTF("[%T %s type:0x%X RD Status Error]", s->name);
	(*status) = data;

	return EC_SUCCESS;
}

static int sar_read(int argc , char *argv[])
{
	int x = 0, tmp;
	int rv;
	int iter, delay;
	char *e;

	iter  = 1;
	delay = 100;

	if (argc >= 4)
		return EC_ERROR_OVERFLOW;

	if (argc >= 2) {
		iter = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		if (iter < 0 || iter > 1000) {
			CPRINTF("sx9310_drv: in %s | invalid iteration\n",
				__func__);
			return EC_ERROR_UNKNOWN;
		}
	}

	if (argc == 3) {
		delay = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;
		if (delay < 10 || delay > 1000) {
			CPRINTF("sx9310_drv: in %s | invalid delay\n",
				 __func__);
			return EC_ERROR_UNKNOWN;
		}
	}

	while (iter) {
		rv = read(&sar_sensors[0], &x, &tmp);
		if (rv != EC_SUCCESS)
			break;
		x = (x & SX9310_PROXSTATANY) == SX9310_PROXSTATANY;
		CPRINTF("sx9310_drv: in %s | body: %d\n", __func__, x);
		if (iter > 1)
			msleep(delay);
		iter--;
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(sarread, sar_read,
	"sarread iteration (max 1000) delay (max 1000)",
	"read reg status of SAR controller", NULL);

static int init(const struct sar_sensor_t *s)
{
	int ret = 0, tmp, ctrl;

	ret = raw_read8(s->i2c_addr, SX9310_WHO_AM_I_REG, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (tmp != SX9310_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	ret = raw_write8(s->i2c_addr, SX9310_SOFTRESET_REG, SX9310_SOFTRESET);
	if (ret)
		return EC_ERROR_UNKNOWN;

	/* We need to wait for a while to settle down. */
	msleep(300);

	for (ctrl = 0; ctrl < ARRAY_SIZE(g_prox_ctrl); ctrl++) {
		ret = raw_write8(s->i2c_addr, g_prox_ctrl[ctrl].reg,
			g_prox_ctrl[ctrl].reg_val);
		if (ret)
			return EC_ERROR_UNKNOWN;
	}

	for (ctrl = 0; ctrl < ARRAY_SIZE(g_sar_ctrl); ctrl++) {
		ret = raw_write8(s->i2c_addr, g_sar_ctrl[ctrl].reg,
			g_sar_ctrl[ctrl].reg_val);
		if (ret)
			return EC_ERROR_UNKNOWN;
	}

	ret = set_sensitivity(s, PROXTHRESH0, s->sens, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ret = set_sensitivity(s, PROXTHRESH12, s->sens, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ret = set_resolution(s, s->resolution, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ret =  set_interrupt(s, s->sens);
	if (ret)
		return EC_ERROR_UNKNOWN;

	/* We are forcing for compensation. */
	ret = raw_write8(s->i2c_addr, SX9310_IRQSTAT_REG, SX9310_STS_COMPDONE);
	if (ret)
		return EC_ERROR_UNKNOWN;

	CPRINTF("[%T %s: SAR Done Init sensitivity:%d, resolution:%d]\n",
			s->name, s->sens, s->resolution);

	return EC_SUCCESS;
}

static int sar_init(int argc, char *argv[])
{
	return init(&sar_sensors[0]);
}
DECLARE_CONSOLE_COMMAND(sarinit, sar_init, NULL, NULL, NULL);

static int sar_test(int argc, char *argv[])
{
	int ret = 0, tmp;

	struct sar_sensor_t *s = &sar_sensors[0];

	ret = set_resolution(s, 256, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ret = get_resolution(s, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	CPRINTF("set/get resolution: %d\n", tmp);

	ret = set_sensitivity(s, PROXTHRESH0, 192, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ret = get_sensitivity(s, PROXTHRESH0, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	CPRINTF("set/get sensitivity 0: %d\n", tmp);

	ret = set_sensitivity(s, PROXTHRESH12, 768, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ret = get_sensitivity(s, PROXTHRESH12, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	CPRINTF("set/get sensitivity 12: %d\n", tmp);

	ret = set_scanfreq(s, 33300, 1);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ret = get_scanfreq(s, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	CPRINTF("set/get scanfreq: %d\n", tmp);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sartest, sar_test, NULL, NULL, NULL);

const struct sar_drv sx9310_drv = {
	.init = init,
	.read = read,
	.set_resolution = set_resolution,
	.get_resolution = get_resolution,
	.set_sensitivity = set_sensitivity,
	.get_sensitivity = get_sensitivity,
	.set_scanfreq = set_scanfreq,
	.get_scanfreq = get_scanfreq,
	.set_interrupt = set_interrupt,
};

