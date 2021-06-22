/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT zephyr_bmi

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(emul_bmi);

#include <device.h>
#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

#include "emul/emul_bmi.h"

#include "driver/accelgyro_bmi160.h"
#include "driver/accelgyro_bmi260.h"
#include "driver/accelgyro_bmi_common.h"

/**
 * Describe if there is no ongoing I2C message or if there is message handled
 * at the moment (last message doesn't ended with stop or write is not followed
 * by read).
 */
enum bmi_emul_msg_state {
	BMI_EMUL_NONE_MSG,
	BMI_EMUL_IN_WRITE,
	BMI_EMUL_IN_READ
};

struct bmi_emul_frame {
	uint8_t type;
	uint8_t tag;
	uint8_t config;
	uint8_t acc_range;
	uint8_t gyr_range;
	int32_t acc_x;
	int32_t acc_y;
	int32_t acc_z;
	int32_t gyr_x;
	int32_t gyr_y;
	int32_t gyr_z;
	int32_t mag_x;
	int32_t mag_y;
	int32_t mag_z;
	int32_t rhall;

	struct bmi_emul_frame *next;
};

/** Run-time data used by the emulator */
struct bmi_emul_data {
	/** I2C emulator detail */
	struct i2c_emul emul;
	/** BMI device being emulated */
	const struct device *i2c;
	/** Configuration information */
	const struct bmi_emul_cfg *cfg;

	/** Current state of all emulated BMI registers */
	uint8_t reg[0x7f];
	/** Current state of NVM where offset and configuration can be saved */
	uint8_t nvm_conf;
	uint8_t nvm_acc_x;
	uint8_t nvm_acc_y;
	uint8_t nvm_acc_z;
	uint8_t nvm_gyr_x;
	uint8_t nvm_gyr_y;
	uint8_t nvm_gyr_z;
	uint8_t nvm_gyr98;
	/** Internal offset values used in calculations */
	int16_t off_acc_x;
	int16_t off_acc_y;
	int16_t off_acc_z;
	int16_t off_gyr_x;
	int16_t off_gyr_y;
	int16_t off_gyr_z;
	/** Internal values of sensors */
	int32_t acc_x;
	int32_t acc_y;
	int32_t acc_z;
	int32_t gyr_x;
	int32_t gyr_y;
	int32_t gyr_z;

	/**
	 * Return error when trying to start offset compensation when not ready
	 * flag is set.
	 */
	bool error_on_cal_trg_nrdy;
	/**
	 * Return error when trying to start offset compensation with range
	 * set to value different than 2G.
	 */
	bool error_on_cal_trg_bad_range;
	/** Return error when trying to write to RO register */
	bool error_on_ro_write;
	/** Return error when trying to write 1 to reserved bit */
	bool error_on_rsvd_write;
	bool simulate_command_exec_time;

	/** Current state of I2C bus (if emulator is handling message) */
	enum bmi_emul_msg_state msg_state;
	/** Number of already handled bytes in ongoing message */
	int msg_byte;
	/** Register selected in last write command */
	uint8_t cur_reg;
	/** Value of data byte in ongoing write message */
	uint8_t write_byte;

	/** Custom write function called on I2C write opperation */
	bmi_emul_write_func write_func;
	/** Data passed to custom write function */
	void *write_func_data;
	/** Custom read function called on I2C read opperation */
	bmi_emul_read_func read_func;
	/** Data passed to custom read function */
	void *read_func_data;

	/** Control if read should fail on given register */
	int read_fail_reg;
	/** Control if write should fail on given register */
	int write_fail_reg;

	/** List of FIFO frames */
	struct bmi_emul_frame *fifo_frame;
	/** First FIFO frame in byte format */
	uint8_t fifo[21];
	/** Number of FIFO frames that were skipped */
	uint8_t fifo_skip;
	/** Currently accessed byte of first frame */
	int fifo_frame_byte;
	/** Length of first frame */
	int fifo_frame_len;

	/** Last time when emulator was resetted in sensor time units */
	int64_t zero_time;
	/** Time when current command should end */
	uint32_t cmd_end_time;

	/** Mutex used to control access to emulator data */
	struct k_mutex data_mtx;
};

/** Static configuration for the emulator */
struct bmi_emul_cfg {
	/** Label of the I2C bus this emulator connects to */
	const char *i2c_label;
	/** Pointer to run-time data */
	struct bmi_emul_data *data;
	/** Address of BMI on i2c bus */
	uint16_t addr;
};

/** Check description in emul_bmi255.h */
int bmi_emul_lock_data(struct i2c_emul *emul, k_timeout_t timeout)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	return k_mutex_lock(&data->data_mtx, timeout);
}

/** Check description in emul_bmi255.h */
int bmi_emul_unlock_data(struct i2c_emul *emul)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	return k_mutex_unlock(&data->data_mtx);
}

/** Check description in emul_bmi255.h */
void bmi_emul_set_write_func(struct i2c_emul *emul,
			     bmi_emul_write_func func, void *data)
{
	struct bmi_emul_data *emul_data;

	emul_data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	emul_data->write_func = func;
	emul_data->write_func_data = data;
}

/** Check description in emul_bmi255.h */
void bmi_emul_set_read_func(struct i2c_emul *emul,
			    bmi_emul_read_func func, void *data)
{
	struct bmi_emul_data *emul_data;

	emul_data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	emul_data->read_func = func;
	emul_data->read_func_data = data;
}

/** Check description in emul_bmi255.h */
void bmi_emul_set_reg(struct i2c_emul *emul, int reg, uint8_t val)
{
	struct bmi_emul_data *data;

	if (reg < 0 || reg > BMI160_CMD_REG) {
		return;
	}

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	data->reg[reg] = val;
}

/** Check description in emul_bmi255.h */
uint8_t bmi_emul_get_reg(struct i2c_emul *emul, int reg)
{
	struct bmi_emul_data *data;

	if (reg < 0 || reg > BMI160_CMD_REG) {
		return 0;
	}

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	return data->reg[reg];
}

/** Check description in emul_bmi255.h */
void bmi_emul_set_read_fail_reg(struct i2c_emul *emul, int reg)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	data->read_fail_reg = reg;
}

/** Check description in emul_bmi255.h */
void bmi_emul_set_write_fail_reg(struct i2c_emul *emul, int reg)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	data->write_fail_reg = reg;
}

/**
 * @brief Convert @p val to two's complement representation. It makes sure that
 *        bit representation is correct even on platforms which represent
 *        signed inteager in different format. Unsigned bit representation
 *        allows to use well defined bitwise operations on returned value.
 *
 * @param val Inteager that is converted
 *
 * @return two's complement representation of @p val
 */
static uint32_t bmi_emul_val_to_twos_comp(int32_t val)
{
	uint32_t twos_comp_val;

	/* Make sure that value is converted to twos compliment format */
	if (val < 0) {
		twos_comp_val = (uint32_t)(-val);
		twos_comp_val = ~twos_comp_val + 1;
	} else {
		twos_comp_val = (uint32_t)val;
	}

	return twos_comp_val;
}

// 1G == BIT(14)
// 62.5 */s == BIT(14)
/**
 * @brief Convert accelerometer value from NVM format (8bit, 0x01 == 3.9mg)
 *        to internal offset format (16bit, 0x01 == 0.061mg).
 *
 * @param nvm Value in NVM format (8bit, 0x01 == 3.9mg). This is binary
 *            representation of two's complement signed number.
 *
 * @return offset Internal representation of @p nvm (16bit, 0x01 == 0.061mg)
 */
static int16_t bmi_emul_acc_nvm_to_off(uint8_t nvm)
{
	int16_t offset;
	int8_t sign;

	if (nvm & BIT(7)) {
		sign = -1;
		/* NVM value is in two's complement format */
		nvm = ~nvm + 1;
	} else {
		sign = 1;
	}

	offset = (int16_t)nvm;
	/* LSB in NVM is 3.9mg, while LSB in internal offset is 0.061mg */
	offset *= sign * 64;

	return offset;
}

/**
 * @brief Convert gyroscope value from NVM format (10bit, 0x01 == 0.061 °/s)
 *        to internal offset format (16bit, 0x01 == 0.0038 °/s)
 *
 * @param nvm Value in NVM format (10bit, 0x01 == 0.061 °/s). This is binary
 *            representation of two's complement signed number.
 *
 * @return offset Internal representation of @p nvm (16bit, 0x01 == 0.0038 °/s)
 */
static int16_t bmi_emul_gyr_nvm_to_off(uint16_t nvm)
{
	int16_t offset;
	int8_t sign;

	/* Mask 10 bits which holds value */
	nvm &= 0x3ff;

	if (nvm & BIT(9)) {
		sign = -1;
		/* NVM value is in two's complement format */
		nvm = ~nvm + 1;
	} else {
		sign = 1;
	}

	offset = (int16_t)nvm;
	/* LSB in NVM is 0.061°/s, while LSB in internal offset is 0.0038°/s */
	offset *= sign * 16;

	return offset;
}

/**
 * @brief Convert accelerometer value from internal offset format
 *        (16bit, 0x01 == 0.061mg) to NVM format (8bit, 0x01 == 7.8mg).
 *        Function makes sure that NVM value is representation of two's
 *        complement signed number.
 *
 * @param val Value in internal offset format (16bit, 0x01 == 0.061mg).
 *
 * @return nvm NVM format representation of @p val (8bit, 0x01 == 3.9mg)
 */
static uint8_t bmi_emul_acc_off_to_nvm(int16_t off)
{
	uint32_t twos_comp_val;
	uint8_t nvm = 0;

	twos_comp_val = bmi_emul_val_to_twos_comp(off);

	/*
	 * LSB in internal representation has value 0.061mg, while in NVM
	 * LSB is 3.9mg. Skip 0.06mg, 0.12mg, 0.24mg, 0.48mg, 0.97mg and
	 * 1.9mg bits.
	 */
	nvm |= (twos_comp_val >> 6) & 0x7f;
	/* Set sign bit */
	nvm |= (twos_comp_val & BIT(31)) ? BIT(7) : 0x00;

	return nvm;
}

/**
 * @brief Convert gyroscope value from internal offset format
 *        (16bit, 0x01 == 0.0038°/s) to NVM format (10bit, 0x01 == 0.061°/s).
 *        Function makes sure that NVM value is representation of two's
 *        complement signed number.
 *
 * @param val Value in internal offset format (16bit, 0x01 == 0.0038°/s).
 *
 * @return nvm NVM format representation of @p val (10bit, 0x01 == 0.061°/s)
 */
static uint16_t bmi_emul_gyr_off_to_nvm(int16_t off)
{
	uint32_t twos_comp_val;
	uint16_t nvm = 0;

	twos_comp_val = bmi_emul_val_to_twos_comp(off);

	/*
	 * LSB in internal representation has value 0.0038°/s, while in NVM
	 * LSB is 0.061°/s. Skip 0.0038°/s, 0.0076°/s, 0.015°/s, and
	 * 0.03°/s bits.
	 */
	nvm |= (twos_comp_val >> 4) & 0x1ff;
	/* Set sign bit */
	nvm |= (twos_comp_val & BIT(31)) ? BIT(9) : 0x00;

	return nvm;
}

/** Check description in emul_bma255.h */
int16_t bmi_emul_get_off(struct i2c_emul *emul, int axis)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	switch (axis) {
	case BMI_EMUL_ACC_X:
		return data->off_acc_x;
	case BMI_EMUL_ACC_Y:
		return data->off_acc_y;
	case BMI_EMUL_ACC_Z:
		return data->off_acc_z;
	case BMI_EMUL_GYR_X:
		return data->off_gyr_x;
	case BMI_EMUL_GYR_Y:
		return data->off_gyr_y;
	case BMI_EMUL_GYR_Z:
		return data->off_gyr_z;
	}

	return 0;
}

/** Check description in emul_bma255.h */
void bmi_emul_set_off(struct i2c_emul *emul, int axis, int16_t val)
{
	struct bmi_emul_data *data;
	uint16_t gyr_off;
	uint8_t gyr98_shift;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	switch (axis) {
	case BMI_EMUL_ACC_X:
		data->off_acc_x = val;
		data->reg[BMI160_OFFSET_ACC70] = bmi_emul_acc_off_to_nvm(
							data->off_acc_x);
		break;
	case BMI_EMUL_ACC_Y:
		data->off_acc_y = val;
		data->reg[BMI160_OFFSET_ACC70 + 1] = bmi_emul_acc_off_to_nvm(
							data->off_acc_y);
		break;
	case BMI_EMUL_ACC_Z:
		data->off_acc_z = val;
		data->reg[BMI160_OFFSET_ACC70 + 2] = bmi_emul_acc_off_to_nvm(
							data->off_acc_z);
		break;
	case BMI_EMUL_GYR_X:
		data->off_gyr_x = val;
		gyr_off = bmi_emul_gyr_off_to_nvm(data->off_gyr_x);
		data->reg[BMI160_OFFSET_GYR70] = gyr_off & 0xff;
		gyr98_shift = 0;
		data->reg[BMI160_OFFSET_EN_GYR98] &= ~(0x3 << gyr98_shift);
		data->reg[BMI160_OFFSET_EN_GYR98] |= (gyr_off & 0x300) >>
						     (8 - gyr98_shift);
		break;
	case BMI_EMUL_GYR_Y:
		data->off_gyr_y = val;
		gyr_off = bmi_emul_gyr_off_to_nvm(data->off_gyr_y);
		data->reg[BMI160_OFFSET_GYR70 + 1] = gyr_off & 0xff;
		gyr98_shift = 2;
		data->reg[BMI160_OFFSET_EN_GYR98] &= ~(0x3 << gyr98_shift);
		data->reg[BMI160_OFFSET_EN_GYR98] |= (gyr_off & 0x300) >>
						     (8 - gyr98_shift);
		break;
	case BMI_EMUL_GYR_Z:
		data->off_gyr_z = val;
		gyr_off = bmi_emul_gyr_off_to_nvm(data->off_gyr_z);
		data->reg[BMI160_OFFSET_GYR70 + 2] = gyr_off & 0xff;
		gyr98_shift = 4;
		data->reg[BMI160_OFFSET_EN_GYR98] &= ~(0x3 << gyr98_shift);
		data->reg[BMI160_OFFSET_EN_GYR98] |= (gyr_off & 0x300) >>
						     (8 - gyr98_shift);
		break;
	}
}

/** Check description in emul_bma255.h */
int32_t bmi_emul_get_value(struct i2c_emul *emul, int axis)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	switch (axis) {
	case BMI_EMUL_ACC_X:
		return data->acc_x;
	case BMI_EMUL_ACC_Y:
		return data->acc_y;
	case BMI_EMUL_ACC_Z:
		return data->acc_z;
	case BMI_EMUL_GYR_X:
		return data->gyr_x;
	case BMI_EMUL_GYR_Y:
		return data->gyr_y;
	case BMI_EMUL_GYR_Z:
		return data->gyr_z;
	}

	return 0;
}

/** Check description in emul_bma255.h */
void bmi_emul_set_value(struct i2c_emul *emul, int axis, int32_t val)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	switch (axis) {
	case BMI_EMUL_ACC_X:
		data->acc_x = val;
		break;
	case BMI_EMUL_ACC_Y:
		data->acc_y = val;
		break;
	case BMI_EMUL_ACC_Z:
		data->acc_z = val;
		break;
	case BMI_EMUL_GYR_X:
		data->gyr_x = val;
		break;
	case BMI_EMUL_GYR_Y:
		data->gyr_y = val;
		break;
	case BMI_EMUL_GYR_Z:
		data->gyr_z = val;
		break;
	}
}

/** Check description in emul_bma255.h */
void bmi_emul_set_err_on_cal_nrdy(struct i2c_emul *emul, bool set)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	data->error_on_cal_trg_nrdy = set;
}

/** Check description in emul_bma255.h */
void bmi_emul_set_err_on_cal_bad_range(struct i2c_emul *emul, bool set)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	data->error_on_cal_trg_bad_range = set;
}

/** Check description in emul_bma255.h */
void bmi_emul_set_err_on_ro_write(struct i2c_emul *emul, bool set)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	data->error_on_ro_write = set;
}

/** Check description in emul_bma255.h */
void bmi_emul_set_err_on_rsvd_write(struct i2c_emul *emul, bool set)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	data->error_on_rsvd_write = set;
}

/** Mask reserved bits in each register of BMI160 */
static const uint8_t bmi_emul_160_rsvd_mask[] = {
	[BMI160_CHIP_ID]		= 0x00,
	[0x01]				= 0xff, /* Reserved */
	[BMI160_ERR_REG]		= 0x00,
	[BMI160_PMU_STATUS]		= 0xc0,
	[BMI160_MAG_X_L_G]		= 0x00,
	[BMI160_MAG_X_H_G]		= 0x00,
	[BMI160_MAG_Y_L_G]		= 0x00,
	[BMI160_MAG_Y_H_G]		= 0x00,
	[BMI160_MAG_Z_L_G]		= 0x00,
	[BMI160_MAG_Z_H_G]		= 0x00,
	[BMI160_RHALL_L_G]		= 0x00,
	[BMI160_RHALL_H_G]		= 0x00,
	[BMI160_GYR_X_L_G]		= 0x00,
	[BMI160_GYR_X_H_G]		= 0x00,
	[BMI160_GYR_Y_L_G]		= 0x00,
	[BMI160_GYR_Y_H_G]		= 0x00,
	[BMI160_GYR_Z_L_G]		= 0x00,
	[BMI160_GYR_Z_H_G]		= 0x00,
	[BMI160_ACC_X_L_G]		= 0x00,
	[BMI160_ACC_X_H_G]		= 0x00,
	[BMI160_ACC_Y_L_G]		= 0x00,
	[BMI160_ACC_Y_H_G]		= 0x00,
	[BMI160_ACC_Z_L_G]		= 0x00,
	[BMI160_ACC_Z_H_G]		= 0x00,
	[BMI160_SENSORTIME_0]		= 0x00,
	[BMI160_SENSORTIME_1]		= 0x00,
	[BMI160_SENSORTIME_2]		= 0x00,
	[BMI160_STATUS]			= 0x01,
	[BMI160_INT_STATUS_0]		= 0x00,
	[BMI160_INT_STATUS_1]		= 0x03,
	[BMI160_INT_STATUS_2]		= 0x00,
	[BMI160_INT_STATUS_3]		= 0x00,
	[BMI160_TEMPERATURE_0]		= 0x00,
	[BMI160_TEMPERATURE_1]		= 0x00,
	[BMI160_FIFO_LENGTH_0]		= 0x00,
	[BMI160_FIFO_LENGTH_1]		= 0xf8,
	[BMI160_FIFO_DATA]		= 0x00,
	[0x25 ... 0x3f]			= 0xff, /* Reserved */
	[BMI160_ACC_CONF]		= 0x00,
	[BMI160_ACC_RANGE]		= 0xf0,
	[BMI160_GYR_CONF]		= 0xc0,
	[BMI160_GYR_RANGE]		= 0xf8,
	[BMI160_MAG_CONF]		= 0xf0,
	[BMI160_FIFO_DOWNS]		= 0x00,
	[BMI160_FIFO_CONFIG_0]		= 0x00,
	[BMI160_FIFO_CONFIG_1]		= 0x01,
	[0x48 ... 0x4a]			= 0xff, /* Reserved */
	[BMI160_MAG_IF_0]		= 0x01,
	[BMI160_MAG_IF_1]		= 0x40,
	[BMI160_MAG_IF_2]		= 0x00,
	[BMI160_MAG_IF_3]		= 0x00,
	[BMI160_MAG_IF_4]		= 0x00,
	[BMI160_INT_EN_0]		= 0x08,
	[BMI160_INT_EN_1]		= 0x80,
	[BMI160_INT_EN_2]		= 0xf0,
	[BMI160_INT_OUT_CTRL]		= 0x00,
	[BMI160_INT_LATCH]		= 0xc0,
	[BMI160_INT_MAP_0]		= 0x00,
	[BMI160_INT_MAP_1]		= 0x00,
	[BMI160_INT_MAP_2]		= 0x00,
	[BMI160_INT_DATA_0]		= 0x77,
	[BMI160_INT_DATA_1]		= 0x7f,
	[BMI160_INT_LOW_HIGH_0]		= 0x00,
	[BMI160_INT_LOW_HIGH_1]		= 0x00,
	[BMI160_INT_LOW_HIGH_2]		= 0x3c,
	[BMI160_INT_LOW_HIGH_3]		= 0x00,
	[BMI160_INT_LOW_HIGH_4]		= 0x00,
	[BMI160_INT_MOTION_0]		= 0x00,
	[BMI160_INT_MOTION_1]		= 0x00,
	[BMI160_INT_MOTION_2]		= 0x00,
	[BMI160_INT_MOTION_3]		= 0xc0,
	[BMI160_INT_TAP_0]		= 0x38,
	[BMI160_INT_TAP_1]		= 0xe0,
	[BMI160_INT_ORIENT_0]		= 0x00,
	[BMI160_INT_ORIENT_1]		= 0x00,
	[BMI160_INT_FLAT_0]		= 0xc0,
	[BMI160_INT_FLAT_1]		= 0xc8,
	[BMI160_FOC_CONF]		= 0x80,
	[BMI160_CONF]			= 0xfd,
	[BMI160_IF_CONF]		= 0xce,
	[BMI160_PMU_TRIGGER]		= 0x80,
	[BMI160_SELF_TEST]		= 0xe0,
	[0x6e]				= 0xff, /* Reserved */
	[0x6f]				= 0xff, /* Reserved */
	[BMI160_NV_CONF]		= 0xf0,
	[BMI160_OFFSET_ACC70]		= 0x00,
	[BMI160_OFFSET_ACC70 + 1]	= 0x00,
	[BMI160_OFFSET_ACC70 + 2]	= 0x00,
	[BMI160_OFFSET_GYR70]		= 0x00,
	[BMI160_OFFSET_GYR70 + 1]	= 0x00,
	[BMI160_OFFSET_GYR70 + 2]	= 0x00,
	[BMI160_OFFSET_EN_GYR98]	= 0x00,
	[BMI160_STEP_CNT_0]		= 0x00,
	[BMI160_STEP_CNT_1]		= 0x00,
	[BMI160_STEP_CONF_0]		= 0x00,
	[BMI160_STEP_CONF_1]		= 0xf0,
	[0x7c]				= 0xff, /* Reserved */
	[0x7d]				= 0xff, /* Reserved */
	[BMI160_CMD_REG]		= 0x00,
};

/**
 * Sensor time in 39 us units
 */
static int64_t bmi_emul_get_sensortime(void)
{
	return k_uptime_ticks() * 1000000 / 39 / CONFIG_SYS_CLOCK_TICKS_PER_SEC;
}

static void bmi_emul_set_sensortime_reg(struct i2c_emul *emul, uint8_t *reg)
{
	struct bmi_emul_data *data;
	uint32_t twos_comp_val;
	int64_t time;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	time = bmi_emul_get_sensortime();

	twos_comp_val = bmi_emul_val_to_twos_comp(time - data->zero_time);

	*reg = twos_comp_val & 0xff;
	*(reg + 1) = (twos_comp_val >> 8) & 0xff;
	*(reg + 1) = (twos_comp_val >> 16) & 0xff;
}

/**
 * @brief Convert range in format of ACC_RANGE register to number of bits
 *        that should be shifted right to obtain 16 bit reported accelerometer
 *        value from internal 32 bit value
 *
 * @param range Value of ACC_RANGE register
 *
 * @return shift Number of LSB that should be ignored from internal
 *               accelerometer value
 */
static int bmi_emul_range_to_shift(uint8_t range)
{
	switch (range & 0xf) {
	case BMI160_GSEL_2G:
		return 0;
	case BMI160_GSEL_4G:
		return 1;
	case BMI160_GSEL_8G:
		return 2;
	case BMI160_GSEL_16G:
		return 3;
	default:
		return 0;
	}
}

/**
 * @brief Set given data register
 *
 * @param emul Pointer to BMI emulator
 * @param val Accelerometer or gyroscope vale in internal units
 * @param reg Register which should be updated
 * @param lsb Flag indicating if LSB or MSB is accessed
 * @param acc FLAG indicating if accelerometer or gyroscope value was provided
 */
static void bmi_emul_set_data_reg(struct i2c_emul *emul, int32_t val,
				  uint8_t *reg, int shift)
{
	struct bmi_emul_data *data;
	uint32_t twos_comp_val;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	twos_comp_val = bmi_emul_val_to_twos_comp(val);

	/* Shift unused bits because of selected range */
	twos_comp_val >>= shift;

	*reg = twos_comp_val & 0xff;
	*(reg + 1) = (twos_comp_val >> 8) & 0xff;
}


static uint8_t bmi_emul_get_frame_len(struct i2c_emul *emul,
				      struct bmi_emul_frame *frame)
{
	struct bmi_emul_data *data;
	uint8_t fifo_conf;
	int len;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	fifo_conf = data->reg[BMI160_FIFO_CONFIG_1];

	/* Empty fifo frame */
	if (frame == NULL) {
		if ((fifo_conf & BMI160_FIFO_TAG_TIME_EN) &&
		    (fifo_conf & BMI160_FIFO_HEADER_EN)) {
			/* Header of sensortime + sensortime + empty fifo */
			return 5;
		} else {
			/* Empty fifo */
			return 1;
		}
	}

	if (frame->type & BMI_EMUL_FRAME_CONFIG) {
		if (fifo_conf & BMI160_FIFO_HEADER_EN) {
			/* Header + byte of data */
			return 2;
		} else {
			/* This frame doesn't exist in headerless mode */
			return 0;
		}
	}

	if (fifo_conf & BMI160_FIFO_HEADER_EN) {
		len = 1;
	} else {
		len = 0;
	}

	if (frame->type & BMI_EMUL_FRAME_ACC) {
		len += 6;
	}
	if (frame->type & BMI_EMUL_FRAME_MAG) {
		len += 8;
	}
	if (frame->type & BMI_EMUL_FRAME_GYR) {
		len += 6;
	}

	return len;
}

static void bmi_emul_set_current_frame(struct i2c_emul *emul,
				       struct bmi_emul_frame *frame)
{
	struct bmi_emul_data *data;
	uint8_t range;
	uint8_t fifo_conf;
	int shift;
	int i = 0;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	fifo_conf = data->reg[BMI160_FIFO_CONFIG_1];

	/* Empty fifo frame */
	if (frame == NULL) {
		if ((fifo_conf & BMI160_FIFO_TAG_TIME_EN) &&
		    (fifo_conf & BMI160_FIFO_HEADER_EN)) {
			/* Header */
			data->fifo[0] = 0x44;
			bmi_emul_set_sensortime_reg(emul, &(data->fifo[1]));
			i = 4;
		}

		/* Empty header */
		data->fifo[i] = 0x80;

		return;
	}

	if (frame->type & BMI_EMUL_FRAME_CONFIG) {
		/* Header */
		data->fifo[0] = 0x48;
		data->fifo[1] = frame->config;
		return;
	}

	if (fifo_conf & BMI160_FIFO_HEADER_EN) {
		data->fifo[0] = 0x80;
		/* TODO */
		data->fifo[0] |= frame->type & BMI_EMUL_FRAME_MAG ? BIT(4) : 0;
		data->fifo[0] |= frame->type & BMI_EMUL_FRAME_GYR ? BIT(3) : 0;
		data->fifo[0] |= frame->type & BMI_EMUL_FRAME_ACC ? BIT(2) : 0;
		data->fifo[0] |= frame->tag & 0x3;
		i = 1;
	}

	if (frame->type & BMI_EMUL_FRAME_MAG) {
		bmi_emul_set_data_reg(emul, frame->mag_x, &(data->fifo[i]), 0);
		i += 2;
		bmi_emul_set_data_reg(emul, frame->mag_y, &(data->fifo[i]), 0);
		i += 2;
		bmi_emul_set_data_reg(emul, frame->mag_z, &(data->fifo[i]), 0);
		i += 2;
		bmi_emul_set_data_reg(emul, frame->rhall, &(data->fifo[i]), 0);
		i += 2;
	}

	if (frame->type & BMI_EMUL_FRAME_GYR) {
		if (frame->gyr_range == 0xff) {
			range = data->reg[BMI160_GYR_RANGE];
		} else {
			range = frame->gyr_range;
		}
		shift = 4 - (range & 0x7);
		bmi_emul_set_data_reg(emul, frame->gyr_x, &(data->fifo[i]),
				      shift);
		i += 2;
		bmi_emul_set_data_reg(emul, frame->gyr_y, &(data->fifo[i]),
				      shift);
		i += 2;
		bmi_emul_set_data_reg(emul, frame->gyr_z, &(data->fifo[i]),
				      shift);
		i += 2;
	}

	if (frame->type & BMI_EMUL_FRAME_ACC) {
		if (frame->acc_range == 0xff) {
			range = data->reg[BMI160_ACC_RANGE];
		} else {
			range = frame->acc_range;
		}
		shift = bmi_emul_range_to_shift(range);
		bmi_emul_set_data_reg(emul, frame->acc_x, &(data->fifo[i]),
				      shift);
		i += 2;
		bmi_emul_set_data_reg(emul, frame->acc_y, &(data->fifo[i]),
				      shift);
		i += 2;
		bmi_emul_set_data_reg(emul, frame->acc_z, &(data->fifo[i]),
				      shift);
		i += 2;
	}
}

/**
 * @brief Reset register values and internal representation of offset and two
 *        general purpose registers
 *
 * @param emul Pointer to BMI emulator
 */
static void bmi_emul_restore_nvm(struct i2c_emul *emul)
{
	struct bmi_emul_data *data;
	uint16_t gyr_nvm;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	/* Restore registers values */
	data->reg[BMI160_NV_CONF] = data->nvm_conf;
	data->reg[BMI160_OFFSET_ACC70] = data->nvm_acc_x;
	data->reg[BMI160_OFFSET_ACC70 + 1] = data->nvm_acc_y;
	data->reg[BMI160_OFFSET_ACC70 + 2] = data->nvm_acc_z;
	data->reg[BMI160_OFFSET_GYR70] = data->nvm_gyr_x;
	data->reg[BMI160_OFFSET_GYR70 + 1] = data->nvm_gyr_y;
	data->reg[BMI160_OFFSET_GYR70 + 2] = data->nvm_gyr_z;
	data->reg[BMI160_OFFSET_EN_GYR98] = data->nvm_gyr98;

	/* Restore internal offset values */
	data->off_acc_x = bmi_emul_acc_nvm_to_off(data->nvm_acc_x);
	data->off_acc_y = bmi_emul_acc_nvm_to_off(data->nvm_acc_y);
	data->off_acc_z = bmi_emul_acc_nvm_to_off(data->nvm_acc_z);

	gyr_nvm = ((data->nvm_gyr98 & 0x3) << 8) | data->nvm_gyr_x;
	data->off_gyr_x = bmi_emul_gyr_nvm_to_off(gyr_nvm);
	gyr_nvm = ((data->nvm_gyr98 & 0xc) << 6) | data->nvm_gyr_y;
	data->off_gyr_y = bmi_emul_gyr_nvm_to_off(gyr_nvm);
	gyr_nvm = ((data->nvm_gyr98 & 0x30) << 4) | data->nvm_gyr_z;
	data->off_gyr_z = bmi_emul_gyr_nvm_to_off(gyr_nvm);
}

static void bmi_emul_flush_fifo(struct i2c_emul *emul)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	data->fifo_skip = 0;
	data->fifo_frame = NULL;
	data->fifo_frame_len = bmi_emul_get_frame_len(emul, data->fifo_frame);
	bmi_emul_set_current_frame(emul, data->fifo_frame);
}

/**
 * @brief Reset registers to default values and restore registers backed by NVM
 *
 * @param emul Pointer to BMI emulator
 */
static void bmi_emul_reset(struct i2c_emul *emul)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	data->reg[BMI160_CHIP_ID]		= 0xd1;
	data->reg[BMI160_ERR_REG]		= 0x00;
	data->reg[BMI160_PMU_STATUS]		= 0x00;
	data->reg[BMI160_MAG_X_L_G]		= 0x00;
	data->reg[BMI160_MAG_X_H_G]		= 0x00;
	data->reg[BMI160_MAG_Y_L_G]		= 0x00;
	data->reg[BMI160_MAG_Y_H_G]		= 0x00;
	data->reg[BMI160_MAG_Z_L_G]		= 0x00;
	data->reg[BMI160_MAG_Z_H_G]		= 0x00;
	data->reg[BMI160_RHALL_L_G]		= 0x00;
	data->reg[BMI160_RHALL_H_G]		= 0x00;
	data->reg[BMI160_GYR_X_L_G]		= 0x00;
	data->reg[BMI160_GYR_X_H_G]		= 0x00;
	data->reg[BMI160_GYR_Y_L_G]		= 0x00;
	data->reg[BMI160_GYR_Y_H_G]		= 0x00;
	data->reg[BMI160_GYR_Z_L_G]		= 0x00;
	data->reg[BMI160_GYR_Z_H_G]		= 0x00;
	data->reg[BMI160_ACC_X_L_G]		= 0x00;
	data->reg[BMI160_ACC_X_H_G]		= 0x00;
	data->reg[BMI160_ACC_Y_L_G]		= 0x00;
	data->reg[BMI160_ACC_Y_H_G]		= 0x00;
	data->reg[BMI160_ACC_Z_L_G]		= 0x00;
	data->reg[BMI160_ACC_Z_H_G]		= 0x00;
	data->reg[BMI160_SENSORTIME_0]		= 0x00;
	data->reg[BMI160_SENSORTIME_1]		= 0x00;
	data->reg[BMI160_SENSORTIME_2]		= 0x00;
	data->reg[BMI160_STATUS]		= 0x01;
	data->reg[BMI160_INT_STATUS_0]		= 0x00;
	data->reg[BMI160_INT_STATUS_1]		= 0x00;
	data->reg[BMI160_INT_STATUS_2]		= 0x00;
	data->reg[BMI160_INT_STATUS_3]		= 0x00;
	data->reg[BMI160_TEMPERATURE_0]		= 0x00;
	data->reg[BMI160_TEMPERATURE_1]		= 0x00;
	data->reg[BMI160_FIFO_LENGTH_0]		= 0x00;
	data->reg[BMI160_FIFO_LENGTH_1]		= 0x00;
	data->reg[BMI160_FIFO_DATA]		= 0x00;
	data->reg[BMI160_ACC_CONF]		= 0x28;
	data->reg[BMI160_ACC_RANGE]		= 0x03;
	data->reg[BMI160_GYR_CONF]		= 0x28;
	data->reg[BMI160_GYR_RANGE]		= 0x00;
	data->reg[BMI160_MAG_CONF]		= 0x0b;
	data->reg[BMI160_FIFO_DOWNS]		= 0x88;
	data->reg[BMI160_FIFO_CONFIG_0]		= 0x80;
	data->reg[BMI160_FIFO_CONFIG_1]		= 0x10;
	data->reg[BMI160_MAG_IF_0]		= 0x20;
	data->reg[BMI160_MAG_IF_1]		= 0x80;
	data->reg[BMI160_MAG_IF_2]		= 0x42;
	data->reg[BMI160_MAG_IF_3]		= 0x4c;
	data->reg[BMI160_MAG_IF_4]		= 0x00;
	data->reg[BMI160_INT_EN_0]		= 0x00;
	data->reg[BMI160_INT_EN_1]		= 0x00;
	data->reg[BMI160_INT_EN_2]		= 0x00;
	data->reg[BMI160_INT_OUT_CTRL]		= 0x00;
	data->reg[BMI160_INT_LATCH]		= 0x00;
	data->reg[BMI160_INT_MAP_0]		= 0x00;
	data->reg[BMI160_INT_MAP_1]		= 0x00;
	data->reg[BMI160_INT_MAP_2]		= 0x00;
	data->reg[BMI160_INT_DATA_0]		= 0x00;
	data->reg[BMI160_INT_DATA_1]		= 0x00;
	data->reg[BMI160_INT_LOW_HIGH_0]	= 0x07;
	data->reg[BMI160_INT_LOW_HIGH_1]	= 0x30;
	data->reg[BMI160_INT_LOW_HIGH_2]	= 0x81;
	data->reg[BMI160_INT_LOW_HIGH_3]	= 0xdb;
	data->reg[BMI160_INT_LOW_HIGH_4]	= 0xc0;
	data->reg[BMI160_INT_MOTION_0]		= 0x00;
	data->reg[BMI160_INT_MOTION_1]		= 0x14;
	data->reg[BMI160_INT_MOTION_2]		= 0x14;
	data->reg[BMI160_INT_MOTION_3]		= 0x24;
	data->reg[BMI160_INT_TAP_0]		= 0x04;
	data->reg[BMI160_INT_TAP_1]		= 0xda;
	data->reg[BMI160_INT_ORIENT_0]		= 0x18;
	data->reg[BMI160_INT_ORIENT_1]		= 0x48;
	data->reg[BMI160_INT_FLAT_0]		= 0x08;
	data->reg[BMI160_INT_FLAT_1]		= 0x11;
	data->reg[BMI160_FOC_CONF]		= 0x00;
	data->reg[BMI160_CONF]			= 0x00;
	data->reg[BMI160_IF_CONF]		= 0x00;
	data->reg[BMI160_PMU_TRIGGER]		= 0x00;
	data->reg[BMI160_SELF_TEST]		= 0x00;
	data->reg[BMI160_STEP_CNT_0]		= 0x00;
	data->reg[BMI160_STEP_CNT_1]		= 0x00;
	data->reg[BMI160_STEP_CONF_0]		= 0x00;
	data->reg[BMI160_STEP_CONF_1]		= 0x15;
	data->reg[BMI160_CMD_REG]		= 0x03;

	/* Restore registers backed in NVM */
	bmi_emul_restore_nvm(emul);

	/* Flush FIFO */
	bmi_emul_flush_fifo(emul);

	/* Reset sensor timer */
	data->zero_time = bmi_emul_get_sensortime();
}

/**
 * @brief Clear all interrupt registers
 *
 * @param emul Pointer to BMI emulator
 */
static void bmi_emul_clear_int(struct i2c_emul *emul)
{
	struct bmi_emul_data *data;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	data->reg[BMI160_INT_STATUS_0] = 0x00;
	data->reg[BMI160_INT_STATUS_1] = 0x00;
	data->reg[BMI160_INT_STATUS_2] = 0x00;
	data->reg[BMI160_INT_STATUS_3] = 0x00;
}

/**
 * @brief Get offset value for given gyroscope value. If gyroscope value is
 *        above maximum (belowe minimum), then minimum -31,25°/s
 *        (maximum 31,25°/s) offset value is returned.
 *
 * @param gyr Gyroscope value
 */
static int16_t bmi_emul_get_gyr_target_off(int32_t gyr)
{
	if (gyr > BMI_EMUL_125_DEG_S / 4) {
		return -((int32_t)BMI_EMUL_125_DEG_S / 4);
	}

	if (gyr < -((int32_t)BMI_EMUL_125_DEG_S / 4)) {
		return BMI_EMUL_125_DEG_S / 4;
	}

	return -gyr;
}

/**
 * @brief Get offset value for given accelerometer value. If accelerometer
 *        value - target is above maximum (belowe minimum), then minimum -0.5g
 *        (maximum 0.5g) offset value is returned.
 *
 * @param acc Accelerometer value
 * @param target Target value in FOC configuration register format
 */
static int16_t bmi_emul_get_acc_target_off(int32_t acc, uint8_t target)
{
	switch (target) {
	case BMI160_FOC_ACC_PLUS_1G:
		acc -= BMI_EMUL_1G;
		break;
	case BMI160_FOC_ACC_MINUS_1G:
		acc += BMI_EMUL_1G;
		break;
	}

	if (acc > BMI_EMUL_1G / 2) {
		return -((int32_t)BMI_EMUL_1G / 2);
	}

	if (acc < -((int32_t)BMI_EMUL_1G / 2)) {
		return BMI_EMUL_1G / 2;
	}

	return -acc;
}
/**
 * @brief Handle fast offset compensation. Check FOC configuration register
 *        and sets gyroscope and/or accelerometer offset using current emulator
 *        state.
 *
 * @param emul Pointer to BMI emulator
 */
static void bmi_emul_handle_off_comp(struct i2c_emul *emul)
{
	struct bmi_emul_data *data;
	uint8_t target;
	int16_t off;

	int acc;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	if (data->reg[BMI160_FOC_CONF] & BMI160_FOC_GYRO_EN) {
		off = bmi_emul_get_gyr_target_off(data->gyr_x);
		bmi_emul_set_off(emul, BMI_EMUL_GYR_X, off);
		off = bmi_emul_get_gyr_target_off(data->gyr_y);
		bmi_emul_set_off(emul, BMI_EMUL_GYR_Y, off);
		off = bmi_emul_get_gyr_target_off(data->gyr_z);
		bmi_emul_set_off(emul, BMI_EMUL_GYR_Z, off);
	}

	target = (data->reg[BMI160_FOC_CONF] >> BMI160_FOC_ACC_X_OFFSET) & 0x3;
	if (target) {
		off = bmi_emul_get_acc_target_off(data->acc_x, target);
		bmi_emul_set_off(emul, BMI_EMUL_ACC_X, off);
	}

	target = (data->reg[BMI160_FOC_CONF] >> BMI160_FOC_ACC_Y_OFFSET) & 0x3;
	if (target) {
		off = bmi_emul_get_acc_target_off(data->acc_y, target);
		bmi_emul_set_off(emul, BMI_EMUL_ACC_Y, off);
	}

	target = (data->reg[BMI160_FOC_CONF] >> BMI160_FOC_ACC_Z_OFFSET) & 0x3;
	if (target) {
		off = bmi_emul_get_acc_target_off(data->acc_z, target);
		bmi_emul_set_off(emul, BMI_EMUL_ACC_Z, off);
	}
}

/**
 * @brief Execute first part of command. Emulate state of device which is
 *        during handling command (status bits etc). This function save time
 *        on which @ref bmi_emul_end_cmd should be called.
 *
 * @param emul Pointer to BMI emulator
 * @param cmd Command that is starting
 *
 * @return 0 on success
 * @return -EIO on failure
 */
static int bmi_emul_start_cmd(struct i2c_emul *emul, int cmd)
{
	struct bmi_emul_data *data;
	int time;
	int ret;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	switch (cmd) {
	case BMI160_CMD_SOFT_RESET:
		time = 1;
		break;
	case BMI160_CMD_START_FOC:
		if ((data->reg[BMI160_FOC_CONF] & BMI160_FOC_GYRO_EN) &&
		    ((data->reg[BMI160_PMU_STATUS] & 
		      (0x3 << BMI160_PMU_GYR_OFFSET)) !=
		     BMI160_PMU_NORMAL << BMI160_PMU_GYR_OFFSET)) {
			LOG_ERR("Starting gyroscope FOC in low power mode");
			return -EIO;
		}

		if ((data->reg[BMI160_FOC_CONF] & ~BMI160_FOC_GYRO_EN) &&
		    ((data->reg[BMI160_PMU_STATUS] & 
		      (0x3 << BMI160_PMU_ACC_OFFSET)) !=
		     BMI160_PMU_NORMAL << BMI160_PMU_ACC_OFFSET)) {
			LOG_ERR("Starting accelerometer FOC in low power mode");
			return -EIO;
		}

		data->reg[BMI160_STATUS] &= ~BMI160_FOC_RDY;
		time = 250;
		break;
	case BMI160_CMD_ACC_MODE_SUSP:
	case BMI160_CMD_GYR_MODE_SUSP:
	case BMI160_CMD_MAG_MODE_SUSP:
		time = 0;
		break;
	/* Real hardware probably switch faster if not in suspend mode */
	case BMI160_CMD_ACC_MODE_NORMAL:
	case BMI160_CMD_ACC_MODE_LOWPOWER:
		time = 4;
		break;
	case BMI160_CMD_GYR_MODE_NORMAL:
	case BMI160_CMD_GYR_MODE_FAST_STARTUP:
		time = 80;
		break;
	case BMI160_CMD_MAG_MODE_NORMAL:
	case BMI160_CMD_MAG_MODE_LOWPOWER:
		time = 1;
		break;
	case BMI160_CMD_FIFO_FLUSH:
		time = 0;
		break;
	case BMI160_CMD_INT_RESET:
		time = 0;
		break;
	default:
		LOG_ERR("Unknown command 0x%x", cmd);
		return -EIO;
	}

	data->reg[BMI160_CMD_REG] = cmd;
	data->cmd_end_time = k_uptime_get_32() + time;

	return 0;
}

static void bmi_emul_end_cmd(struct i2c_emul *emul)
{
	struct bmi_emul_data *data;
	uint8_t pmu_status;
	int cmd;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	/* There is no ongoing command */
	if (data->reg[BMI160_CMD_REG] == BMI160_CMD_NOOP) {
		return;
	}

	/* We are simulating command execution time and it doesn't expired */
	if (data->simulate_command_exec_time &&
	    data->cmd_end_time > k_uptime_get_32()) {
		return;
	}

	pmu_status = data->reg[BMI160_PMU_STATUS];
	cmd = data->reg[BMI160_CMD_REG];
	data->reg[BMI160_CMD_REG] = BMI160_CMD_NOOP;

	switch (cmd) {
	case BMI160_CMD_SOFT_RESET:
		bmi_emul_reset(emul);
		break;
	case BMI160_CMD_START_FOC:
		bmi_emul_handle_off_comp(emul);
		data->reg[BMI160_STATUS] |= BMI160_FOC_RDY;
		break;
	case BMI160_CMD_ACC_MODE_SUSP:
		pmu_status &= ~(0x3 << BMI160_PMU_ACC_OFFSET);
		pmu_status |= BMI160_PMU_SUSPEND << BMI160_PMU_ACC_OFFSET;
		break;
	case BMI160_CMD_ACC_MODE_NORMAL:
		pmu_status &= ~(0x3 << BMI160_PMU_ACC_OFFSET);
		pmu_status |= BMI160_PMU_NORMAL << BMI160_PMU_ACC_OFFSET;
		break;
	case BMI160_CMD_ACC_MODE_LOWPOWER:
		pmu_status &= ~(0x3 << BMI160_PMU_ACC_OFFSET);
		pmu_status |= BMI160_PMU_LOW_POWER << BMI160_PMU_ACC_OFFSET;
		break;
	case BMI160_CMD_GYR_MODE_SUSP:
		pmu_status &= ~(0x3 << BMI160_PMU_GYR_OFFSET);
		pmu_status |= BMI160_PMU_SUSPEND << BMI160_PMU_GYR_OFFSET;
		break;
	case BMI160_CMD_GYR_MODE_NORMAL:
		pmu_status &= ~(0x3 << BMI160_PMU_GYR_OFFSET);
		pmu_status |= BMI160_PMU_NORMAL << BMI160_PMU_GYR_OFFSET;
		break;
	case BMI160_CMD_GYR_MODE_FAST_STARTUP:
		pmu_status &= ~(0x3 << BMI160_PMU_GYR_OFFSET);
		pmu_status |= BMI160_PMU_FAST_STARTUP << BMI160_PMU_GYR_OFFSET;
		break;
	case BMI160_CMD_MAG_MODE_SUSP:
		pmu_status &= ~(0x3 << BMI160_PMU_MAG_OFFSET);
		pmu_status |= BMI160_PMU_SUSPEND << BMI160_PMU_MAG_OFFSET;
		break;
	case BMI160_CMD_MAG_MODE_NORMAL:
		pmu_status &= ~(0x3 << BMI160_PMU_MAG_OFFSET);
		pmu_status |= BMI160_PMU_NORMAL << BMI160_PMU_MAG_OFFSET;
		break;
	case BMI160_CMD_MAG_MODE_LOWPOWER:
		pmu_status &= ~(0x3 << BMI160_PMU_MAG_OFFSET);
		pmu_status |= BMI160_PMU_LOW_POWER << BMI160_PMU_MAG_OFFSET;
		break;
	case BMI160_CMD_FIFO_FLUSH:
		bmi_emul_flush_fifo(emul);
		break;
	case BMI160_CMD_INT_RESET:
		bmi_emul_clear_int(emul);
		break;
	}

	/* Clear FIFO on sensor on/off */
	if (pmu_status != data->reg[BMI160_PMU_STATUS]) {
		bmi_emul_flush_fifo(emul);
		data->reg[BMI160_PMU_STATUS] = pmu_status;
	}
}

/**
 * @brief Handle I2C write message. It is checked if accessed register isn't RO
 *        and reserved bits are set to 0. Write set value of reg field of bmi
 *        emulator data ignoring reserved bits and write only bits. Some
 *        commands are handled specialy. Before any handling, custom function
 *        is called if provided.
 *
 * @param emul Pointer to BMI emulator
 * @param reg Register which is written
 * @param val Value being written to @p reg
 *
 * @return 0 on success
 * @return -EIO on error
 */
static int bmi_emul_handle_write(struct i2c_emul *emul, int reg, uint8_t val)
{
	struct bmi_emul_data *data;
	int ret;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	if (data->write_func) {
		ret = data->write_func(emul, reg, val, data->write_func_data);
		if (ret < 0) {
			return -EIO;
		} else if (ret == 0) {
			return 0;
		}
	}

	if (data->write_fail_reg == reg ||
	    data->write_fail_reg == BMI_EMUL_FAIL_ALL_REG) {
		return -EIO;
	}

	if (reg <= BMI160_FIFO_DATA ||
	    (reg >= BMI160_STEP_CNT_0 && reg <= BMI160_STEP_CNT_1)) {
		if (data->error_on_ro_write) {
			LOG_ERR("Writing to reg 0x%x which is RO", reg);
			return -EIO;
		}

		return 0;
	}

	if (data->error_on_rsvd_write && bmi_emul_160_rsvd_mask[reg] & val) {
		LOG_ERR("Writing 0x%x to reg 0x%x with rsvd bits mask 0x%x",
			val, reg, bmi_emul_160_rsvd_mask[reg]);
		return -EIO;
	}

	/* Stop on going command if required */
	bmi_emul_end_cmd(emul);

	switch (reg) {
	case BMI160_CMD_REG:
		if (data->reg[BMI160_CMD_REG] != BMI160_CMD_NOOP) {
			LOG_ERR("Issued command before previous end");
			return -EIO;
		}

		return bmi_emul_start_cmd(emul, val);
	case BMI160_FIFO_CONFIG_1:
		/*
		 * Clear FIFO on transition between headerless and
		 * header mode
		 */
		if ((val & BMI160_FIFO_HEADER_EN) !=
		    (data->reg[reg] & BMI160_FIFO_HEADER_EN)) {
			bmi_emul_flush_fifo(emul);
		}
		break;
	}

	/* Ignore all reserved bits */
	val &= ~bmi_emul_160_rsvd_mask[reg];
	val |= data->reg[reg] & bmi_emul_160_rsvd_mask[reg];

	data->reg[reg] = val;

	return 0;
}

static void bmi_emul_get_data(struct i2c_emul *emul) {
	struct bmi_emul_data *data;
	int shift;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	shift = 4 - (data->reg[BMI160_GYR_RANGE] & 0x7);
	bmi_emul_set_data_reg(emul, data->gyr_x, &(data->reg[BMI160_GYR_X_L_G]),
			      shift);
	bmi_emul_set_data_reg(emul, data->gyr_y, &(data->reg[BMI160_GYR_Y_L_G]),
			      shift);
	bmi_emul_set_data_reg(emul, data->gyr_z, &(data->reg[BMI160_GYR_Z_L_G]),
			      shift);

	shift = bmi_emul_range_to_shift(data->reg[BMI160_ACC_RANGE]);
	bmi_emul_set_data_reg(emul, data->acc_x, &(data->reg[BMI160_ACC_X_L_G]),
			      shift);
	bmi_emul_set_data_reg(emul, data->acc_y, &(data->reg[BMI160_ACC_Y_L_G]),
			      shift);
	bmi_emul_set_data_reg(emul, data->acc_z, &(data->reg[BMI160_ACC_Z_L_G]),
			      shift);

	bmi_emul_set_sensortime_reg(emul, &(data->reg[BMI160_SENSORTIME_0]));
}

static int bmi_emul_append_frame(struct i2c_emul *emul,
				 struct bmi_emul_frame *frame)
{
	struct bmi_emul_data *data;
	struct bmi_emul_frame *tmp_frame;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	if (data->fifo_frame == NULL) {
		data->fifo_frame = frame;
		bmi_emul_set_current_frame(emul, data->fifo_frame);
	} else {
		tmp_frame = data->fifo_frame;
		while (tmp_frame->next != NULL) {
			tmp_frame = tmp_frame->next;
		}
		tmp_frame->next = frame;
	}

	return 0;
}

static uint16_t bmi_emul_fifo_len(struct i2c_emul *emul)
{
	struct bmi_emul_frame *frame;
	struct bmi_emul_data *data;
	uint16_t len = 0;
	uint8_t fifo_conf;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	fifo_conf = data->reg[BMI160_FIFO_CONFIG_1];

	if (data->fifo_skip != 0 && (fifo_conf & BMI160_FIFO_HEADER_EN)) {
		len += 2;
	}

	frame = data->fifo_frame;
	while (frame != NULL) {
		len += bmi_emul_get_frame_len(emul, frame);
		frame = frame->next;
	}

	len += bmi_emul_get_frame_len(emul, NULL);
	/* Do not count last empty frame byte */
	len--;

	return len;
}

/**
 * @brief Get byte that should be returned for FIFO data
 *
 * @param byte
 */
static uint8_t bmi_emul_get_fifo_data(struct i2c_emul *emul, int byte)
{
	struct bmi_emul_data *data;
	uint8_t ret;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	if (byte == 0) {
		/* Repeat uncompleated read of frame */
		data->fifo_frame_byte = 0;

		/* Return header for skip frame */
		if (data->fifo_skip != 0) {
			return 0x40;
		}
	}

	if (data->fifo_skip != 0 && byte == 1) {
		/* Return number of skipped frames */
		ret = data->fifo_skip;
		data->fifo_skip = 0;

		return ret;
	}

	/* Get next valid frame */
	while (data->fifo_frame_byte >= data->fifo_frame_len) {
		/* No data */
		if (data->fifo_frame == NULL) {
			return 0;
		}
		data->fifo_frame = data->fifo_frame->next;
		data->fifo_frame_byte = 0;
		data->fifo_frame_len = bmi_emul_get_frame_len(emul,
							      data->fifo_frame);
		bmi_emul_set_current_frame(emul, data->fifo_frame);
	}

	ret = data->fifo[data->fifo_frame_byte];
	data->fifo_frame_byte++;

	return ret;
}

/**
 * @brief Handle I2C read message. Response is obtained from reg field of bmi
 *        emul data. When accessing accelerometer value, register data is first
 *        computed using internal emulator state. Before default handler, custom
 *        user read function is called if provided.
 *
 * @param emul Pointer to BMI emulator
 * @param reg Register address to read
 * @param byte Byte which is accessed during block read
 * @param buf Pointer where result should be stored
 *
 * @return 0 on success
 * @return -EIO on error
 */
static int bmi_emul_handle_read(struct i2c_emul *emul, int reg, int byte, char *buf)
{
	struct bmi_emul_data *data;
	uint16_t fifo_len;
	int ret;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);

	/*
	 * If register is not FIFO data, then block read access subsequent
	 * registers
	 */
	if (reg != BMI160_FIFO_DATA) {
		reg += byte;
	}

	if (data->read_func) {
		ret = data->read_func(emul, reg, data->read_func_data);
		if (ret < 0) {
			return -EIO;
		} else if (ret == 0) {
			/* Immediately return value set by custom function */
			*buf = data->reg[reg];

			return 0;
		}
	}

	/* Stop on going command if required */
	bmi_emul_end_cmd(emul);

	if (data->read_fail_reg == reg ||
	    data->read_fail_reg == BMI_EMUL_FAIL_ALL_REG) {
		return -EIO;
	}

	/* Burst reads are not supported if all sensors are in suspend mode */
	if ((data->reg[BMI160_PMU_STATUS] & 0x3f) == 0 && byte > 0) {
		LOG_ERR("Block reads are not supported in suspend mode");
		return -EIO;
	}

	switch (reg) {
	case BMI160_GYR_X_L_G:
	case BMI160_GYR_X_H_G:
	case BMI160_GYR_Y_L_G:
	case BMI160_GYR_Y_H_G:
	case BMI160_GYR_Z_L_G:
	case BMI160_GYR_Z_H_G:
	case BMI160_ACC_X_L_G:
	case BMI160_ACC_X_H_G:
	case BMI160_ACC_Y_L_G:
	case BMI160_ACC_Y_H_G:
	case BMI160_ACC_Z_L_G:
	case BMI160_ACC_Z_H_G:
	case BMI160_SENSORTIME_0:
	case BMI160_SENSORTIME_1:
	case BMI160_SENSORTIME_2:
		/*
		 * Snapshot of current emulator state is created on data read
		 * and shouldn't be changed until next I2C operation
		 */
		if (byte == 0) {
			bmi_emul_get_data(emul);
		}
		break;
	case BMI160_FIFO_LENGTH_0:
	case BMI160_FIFO_LENGTH_1:
		if (byte == 0) {
			fifo_len = bmi_emul_fifo_len(emul);
			data->reg[BMI160_FIFO_LENGTH_0] = fifo_len & 0xff;
			data->reg[BMI160_FIFO_LENGTH_1] = (fifo_len >> 8) & 0x7;
		}
		break;
	case BMI160_FIFO_DATA:
		bmi_emul_get_fifo_data(emul, byte);
		break;
	}

	*buf = data->reg[reg];

	return 0;
}

/**
 * @biref Emulate an I2C transfer to a BMI accelerometer
 *
 * This handles simple reads and writes
 *
 * @param emul I2C emulation information
 * @param msgs List of messages to process
 * @param num_msgs Number of messages to process
 * @param addr Address of the I2C target device
 *
 * @retval 0 If successful
 * @retval -EIO General input / output error
 */
static int bmi_emul_transfer(struct i2c_emul *emul, struct i2c_msg *msgs,
			     int num_msgs, int addr)
{
	const struct bmi_emul_cfg *cfg;
	struct bmi_emul_data *data;
	unsigned int len;
	int ret, i;
	bool read;

	data = CONTAINER_OF(emul, struct bmi_emul_data, emul);
	cfg = data->cfg;

	if (cfg->addr != addr) {
		LOG_ERR("Address mismatch, expected %02x, got %02x", cfg->addr,
			addr);
		return -EIO;
	}

	i2c_dump_msgs("emul", msgs, num_msgs, addr);

	for (; num_msgs > 0; num_msgs--, msgs++) {
		read = msgs->flags & I2C_MSG_READ;

		switch (data->msg_state) {
		case BMI_EMUL_NONE_MSG:
			data->msg_byte = 0;
			break;
		case BMI_EMUL_IN_WRITE:
			if (read) {
				/* Finish write command */
				if (data->msg_byte == 2) {
					k_mutex_lock(&data->data_mtx,
						     K_FOREVER);
					ret = bmi_emul_handle_write(emul,
							data->cur_reg,
							data->write_byte);
					k_mutex_unlock(&data->data_mtx);
					if (ret) {
						return -EIO;
					}
				}
				data->msg_byte = 0;
			}
			break;
		case BMI_EMUL_IN_READ:
			if (!read) {
				data->msg_byte = 0;
			}
			break;
		}
		data->msg_state = read ? BMI_EMUL_IN_READ : BMI_EMUL_IN_WRITE;

		if (msgs->flags & I2C_MSG_STOP) {
			data->msg_state = BMI_EMUL_NONE_MSG;
		}

		if (!read) {
			/* Dispatch wrtie command */
			for (i = 0; i < msgs->len; i++) {
				switch (data->msg_byte) {
				case 0:
					data->cur_reg = msgs->buf[i];
					break;
				case 1:
					data->write_byte = msgs->buf[i];
					break;
				default:
					data->msg_state = BMI_EMUL_NONE_MSG;
					LOG_ERR("Too long write command");
					return -EIO;
				}
				data->msg_byte++;
			}

			/* Execute write command */
			if (msgs->flags & I2C_MSG_STOP && data->msg_byte == 2) {
				k_mutex_lock(&data->data_mtx, K_FOREVER);
				ret = bmi_emul_handle_write(emul, data->cur_reg,
							    data->write_byte);
				k_mutex_unlock(&data->data_mtx);
				if (ret) {
					return -EIO;
				}
			}
		} else {
			/* Dispatch read command */
			for (i = 0; i < msgs->len; i++) {
				k_mutex_lock(&data->data_mtx, K_FOREVER);
				ret = bmi_emul_handle_read(emul, data->cur_reg,
							   data->msg_byte,
							   &(msgs->buf[i]));
				k_mutex_unlock(&data->data_mtx);
				if (ret) {
					return -EIO;
				}
			}
		}
	}

	return 0;
}

/* Device instantiation */

static struct i2c_emul_api bmi_emul_api = {
	.transfer = bmi_emul_transfer,
};

/**
 * @brief Set up a new BMI emulator
 *
 * This should be called for each BMI device that needs to be
 * emulated. It registers it with the I2C emulation controller.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */
static int bmi_emul_init(const struct emul *emul,
			 const struct device *parent)
{
	const struct bmi_emul_cfg *cfg = emul->cfg;
	struct bmi_emul_data *data = cfg->data;
	int ret;

	data->emul.api = &bmi_emul_api;
	data->emul.addr = cfg->addr;
	data->i2c = parent;
	data->cfg = cfg;
	k_mutex_init(&data->data_mtx);

	ret = i2c_emul_register(parent, emul->dev_label, &data->emul);

	bmi_emul_reset(&data->emul);

	return ret;
}

#define BMI_EMUL(n)							\
	static struct bmi_emul_data bmi_emul_data_##n = {		\
		.acc_x = DT_INST_PROP(n, nvm_acc_x),			\
		.acc_y = DT_INST_PROP(n, nvm_acc_y),			\
		.acc_z = DT_INST_PROP(n, nvm_acc_z),			\
		.error_on_cal_trg_nrdy = DT_INST_PROP(n,		\
				error_on_compensation_not_ready),	\
		.error_on_ro_write = DT_INST_PROP(n, error_on_ro_write),\
		.error_on_rsvd_write = DT_INST_PROP(n,			\
					error_on_reserved_bit_write),	\
		.msg_state = BMI_EMUL_NONE_MSG,				\
		.cur_reg = 0,						\
		.write_func = NULL,					\
		.read_func = NULL,					\
		.write_fail_reg = BMI_EMUL_NO_FAIL_REG,			\
		.read_fail_reg = BMI_EMUL_NO_FAIL_REG,			\
	};								\
									\
	static const struct bmi_emul_cfg bmi_emul_cfg_##n = {		\
		.i2c_label = DT_INST_BUS_LABEL(n),			\
		.data = &bmi_emul_data_##n,				\
		.addr = DT_INST_REG_ADDR(n),				\
	};								\
	EMUL_DEFINE(bmi_emul_init, DT_DRV_INST(n), &bmi_emul_cfg_##n)

DT_INST_FOREACH_STATUS_OKAY(BMI_EMUL)

#define BMI_EMUL_CASE(n)					\
	case DT_INST_DEP_ORD(n): return &bmi_emul_data_##n.emul;

/** Check description in emul_bmi.h */
struct i2c_emul *bmi_emul_get(int ord)
{
	switch (ord) {
	DT_INST_FOREACH_STATUS_OKAY(BMI_EMUL_CASE)

	default:
		return NULL;
	}
}
