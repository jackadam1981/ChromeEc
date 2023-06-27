/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "driver/accelgyro_bmi3xx.h"
#include "driver/accelgyro_bmi_common.h"
#include "emul/emul_bmi3xx.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT zephyr_bmi3xx_emul

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
LOG_MODULE_REGISTER(emul_bmi3xx);

/** Mask reserved bits in each register of BMI3XX */
static const uint16_t bmi323_emul_rsvd_mask[] = {
	[BMI3_REG_CHIP_ID] = 0xff00,
	[BMI3_REG_ERR_REG] = 0xffe1,
	[BMI3_REG_STATUS] = 0x0,
	[BMI3_REG_ACC_DATA_X] = 0x0,
	[BMI3_REG_ACC_DATA_Y] = 0x0,
	[BMI3_REG_ACC_DATA_Z] = 0x0,
	[BMI3_REG_GYR_DATA_X] = 0x0,
	[BMI3_REG_GYR_DATA_Y] = 0x0,
	[BMI3_REG_GYR_DATA_Z] = 0x0,
	[0x09 ... 0x0b] = 0x0,
	[0x0c] = 0xffc0,
	[BMI3_REG_INT_STATUS_INT1] = 0x0,
	[0x0e ... 0x10] = 0x0,
	[0x11] = 0xc200,
	[0x12 ... 0x13] = 0x0,
	[BMI3_FEATURE_IO_STATUS] = 0xfffe,
	[BMI3_REG_FIFO_FILL_LVL] = 0xf800,
	[BMI3_REG_FIFO_DATA] = 0x0,
	[0x17 ... 0x1f] = 0xffff,
	[BMI3_REG_ACC_CONF] = 0x8000,
	[BMI3_REG_GYR_CONF] = 0x8000,
	[0x22 ... 0x27] = 0xffff,
	[0x28 ... 0x29] = 0xc200,
	[0x2a] = 0xfeee,
	[0x2b] = 0xffee,
	[0x2c ... 0x34] = 0xffff,
	[BMI3_REG_FIFO_WATERMARK] = 0xfc00,
	[BMI3_REG_FIFO_CONF] = 0xf0fe,
	[BMI3_REG_FIFO_CTRL] = 0xfffe,
	[BMI3_REG_IO_INT_CTRL] = 0xf8f8,
	[BMI3_REG_IO_INT_CONF] = 0xfffe,
	[0x3a ... 0x3b] = 0x0,
	[0x3c ... 0x3f] = 0xffff,
	[BMI3_REG_UGAIN_OFF_SEL] = 0xffff,
	[BMI3_REG_FEATURE_ENGINE_GLOB_CTRL] = 0xfffe,
	[0x41] = 0xf800,
	[0x42] = 0x0,
	[0x43] = 0xfffc,
	[0x44] = 0xffff,
	[0x45] = 0xffc4,
	[0x46] = 0xffff,
	[0x47] = 0xffc0,
	[0x48 ... 0x4f] = 0xffff,
	[0x50] = 0xfffe,
	[0x51] = 0xfff0,
	[0x52] = 0xfffc,
	[0x53] = 0xffe0,
	[0x54 ... 0x5f] = 0xffff,
	[0x60] = 0xe000,
	[0x61] = 0xff00,
	[0x62] = 0xe000,
	[0x63] = 0xff00,
	[0x64] = 0xe000,
	[0x65] = 0xff00,
	[0x66] = 0xfc00,
	[0x67] = 0xff80,
	[0x68] = 0xfc00,
	[0x69] = 0xff80,
	[0x6a] = 0xfc00,
	[0x6b] = 0xff80,
	[0x6c ... 0x6f] = 0xffff,
	[0x70] = 0x0,
	[0x71 ... 0x72] = 0xff00,
	[0x73 ... 0x7d] = 0xff,
	[BMI3_REG_CMD] = 0x0,
	[0x7f] = 0xff,
};

/** Run-time data used by the emulator */
struct bmi3xx_emul_data {
	/** Common I2C data */
	struct i2c_common_emul_data common;

	/** Current state of all emulated BMI registers */
	uint16_t reg[BMI3XX_EMUL_MAX_REG];
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

	/** Return error when trying to write to RO register */
	bool error_on_ro_write;
	/** Return error when trying to write 1 to reserved bit */
	bool error_on_rsvd_write;
	/**
	 * If effect of command is vissable after simulated time from issuing
	 * command
	 */
	bool simulate_command_exec_time;
	/** Return error when trying to read WO register */
	bool error_on_wo_read;

	/** Value of data byte in ongoing write message */
	uint8_t write_byte;

	/** List of FIFO frames */
	struct bmi3xx_emul_frame *fifo_frame;
	/** First FIFO frame in byte format */
	uint16_t fifo[8];
	/** Number of FIFO frames that were skipped */
	uint16_t fifo_skip;
	/** Currently accessed byte of first frame */
	int fifo_frame_byte;
	/** Length of first frame */
	int fifo_frame_len;

	/** Last time when emulator was resetted in sensor time units */
	int64_t zero_time;
	/** Time when current command should end */
	uint32_t cmd_end_time;

	/** Emulated model of BMI */
	int type;
	/** Pointer to data specific for emulated model of BMI */
	const struct bmi3xx_emul_type_data *type_data;
};

/** Check description in emul_bmi.h */
void bmi3xx_emul_set_reg(const struct emul *emul, int reg, uint16_t val)
{
	struct bmi3xx_emul_data *data;

	printf("\033[33m%s emul 0x%x 0x%x\033[m\n", __func__, (int)emul, reg);
	if (reg < 0 || reg > BMI3XX_EMUL_MAX_REG) {
		return;
	}

	data = emul->data;
	data->reg[reg] = val;
}

/** Check description in emul_bmi.h */
uint16_t bmi3xx_emul_get_reg(const struct emul *emul, int reg)
{
	struct bmi3xx_emul_data *data;

	printf("\033[33m%s emul 0x%x 0x%x\033[m\n", __func__, (int)emul, reg);
	if (reg < 0 || reg > BMI3XX_EMUL_MAX_REG) {
		return 0;
	}

	data = emul->data;

	return data->reg[reg];
}

/** Check description in emul_bmi.h */
int16_t bmi3xx_emul_get_off(const struct emul *emul, enum bmi3xx_emul_axis axis)
{
	struct bmi3xx_emul_data *data;

	data = emul->data;

	switch (axis) {
	case BMI3XX_EMUL_ACC_X:
		return data->off_acc_x;
	case BMI3XX_EMUL_ACC_Y:
		return data->off_acc_y;
	case BMI3XX_EMUL_ACC_Z:
		return data->off_acc_z;
	case BMI3XX_EMUL_GYR_X:
		return data->off_gyr_x;
	case BMI3XX_EMUL_GYR_Y:
		return data->off_gyr_y;
	case BMI3XX_EMUL_GYR_Z:
		return data->off_gyr_z;
	}

	return 0;
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
static uint32_t bmi3xx_emul_val_to_twos_comp(int32_t val)
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

/**
 * @brief Convert accelerometer value from internal offset format
 *        (16bit, 0x01 == 0.061mg) to NVM format (8bit, 0x01 == 7.8mg).
 *        Function makes sure that NVM value is representation of two's
 *        complement signed number.
 *
 * @param val Value in internal offset format (16bit, 0x01 == 0.061mg).
 *
 * @return nvm NVM format representation of @p val (8bit, 0x01 == 3.9mg)
 * TODO fix nvm keyword
 */
static uint16_t bmi3xx_emul_off_to_2comp(int32_t off, uint8_t msb)
{
	uint32_t twos_comp_val;
	uint16_t nvm = 0;

	twos_comp_val = bmi3xx_emul_val_to_twos_comp(off);

	/*
	 * LSB in internal representation has value 0.061mg, while in NVM
	 * LSB is 3.9mg. Skip 0.06mg, 0.12mg, 0.24mg, 0.48mg, 0.97mg and
	 * 1.9mg bits.
	 */
	nvm |= (twos_comp_val >> (msb - 1)) & 0x7f;
	/* Set sign bit */
	nvm |= (twos_comp_val & BIT(31)) ? BIT(msb) : 0x00;

	return nvm;
}

/** Check description in emul_bmi.h */
void bmi3xx_emul_set_off(const struct emul *emul, enum bmi3xx_emul_axis axis,
			 int16_t val)
{
	struct bmi3xx_emul_data *data;
	uint16_t gyr_off;

	data = emul->data;

	switch (axis) {
	case BMI3XX_EMUL_ACC_X:
		data->off_acc_x = val;
		data->reg[data->type_data->acc_off_reg] =
			bmi3xx_emul_off_to_2comp(data->off_acc_x, 7);
		break;
	case BMI3XX_EMUL_ACC_Y:
		data->off_acc_y = val;
		data->reg[data->type_data->acc_off_reg + 2] =
			bmi3xx_emul_off_to_2comp(data->off_acc_y, 7);
		break;
	case BMI3XX_EMUL_ACC_Z:
		data->off_acc_z = val;
		data->reg[data->type_data->acc_off_reg + 4] =
			bmi3xx_emul_off_to_2comp(data->off_acc_z, 7);
		break;
	case BMI3XX_EMUL_GYR_X:
		data->off_gyr_x = val;
		gyr_off = bmi3xx_emul_off_to_2comp(data->off_gyr_x, 9);
		data->reg[data->type_data->gyr_off_reg] = gyr_off & 0xff;
		break;
	case BMI3XX_EMUL_GYR_Y:
		data->off_gyr_y = val;
		gyr_off = bmi3xx_emul_off_to_2comp(data->off_gyr_y, 9);
		data->reg[data->type_data->gyr_off_reg + 2] = gyr_off & 0xff;
		break;
	case BMI3XX_EMUL_GYR_Z:
		data->off_gyr_z = val;
		gyr_off = bmi3xx_emul_off_to_2comp(data->off_gyr_z, 9);
		data->reg[data->type_data->gyr_off_reg + 4] = gyr_off & 0xff;
		break;
	}
}

/**
 * @brief Compute length of given FIFO @p frame. If frame is null then length
 *        of empty frame is returned.
 *
 * @param emul Pointer to BMI emulator
 * @param frame Pointer to FIFO frame
 *
 * @return length of frame
 */
static uint8_t bmi3xx_emul_get_frame_len(const struct emul *emul,
					 struct bmi3xx_emul_frame *frame)
{
	int len = 0;

	/* Empty FIFO frame */
	if (!frame) {
		return len;
	}

	if (frame->type & BMI3XX_EMUL_FRAME_ACC) {
		len += 6;
	}
	if (frame->type & BMI3XX_EMUL_FRAME_GYR) {
		len += 6;
	}
	if (frame->type & BMI3XX_EMUL_FRAME_TEMP) {
		len += 2;
	}
	if (frame->type & BMI3XX_EMUL_FRAME_TIME) {
		len += 2;
	}

	return len;
}

/** Check description in emul_bmi.h */
uint16_t bmi3xx_emul_fifo_len(const struct emul *emul, bool tag_time)
{
	struct bmi3xx_emul_frame *frame;
	struct bmi3xx_emul_data *data;
	uint16_t len = 0;

	data = emul->data;

	frame = data->fifo_frame;
	while (frame != NULL) {
		len += bmi3xx_emul_get_frame_len(emul, frame);
		frame = frame->next;
	}

	return len;
}

/**
 * @brief Reset registers to default values
 *
 * @param regs Pointer to array of emulator's registers
 * @param emul Pointer to BMI emulator
 */
void bmi323_emul_reset(const struct emul *emul)
{
	bool tag_time;
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;

	printf("\033[31m%s emul 0x%x\033[m\n", __func__, (int)emul);
	regs[BMI3_REG_CHIP_ID] = 0x0043;
	regs[BMI3_REG_ERR_REG] = 0x0;
	regs[BMI3_REG_STATUS] = 0x1;
	regs[BMI3_REG_ACC_DATA_X] = 0x8000;
	regs[BMI3_REG_ACC_DATA_Y] = 0x8000;
	regs[BMI3_REG_ACC_DATA_Z] = 0x8000;
	regs[BMI3_REG_GYR_DATA_X] = 0x8000;
	regs[BMI3_REG_GYR_DATA_Y] = 0x8000;
	regs[BMI3_REG_GYR_DATA_Z] = 0x8000;
	regs[0x09] = 0x8000;
	for (int i = 0x0a; i <= 0x13; i++) {
		regs[i] = 0x0;
	}
	regs[BMI3_FEATURE_IO_STATUS] = 0x18;
	for (int i = 0x15; i <= 0x1f; i++) {
		regs[i] = 0x0;
	}
	regs[BMI3_REG_ACC_CONF] = 0x28;
	regs[BMI3_REG_GYR_CONF] = 0x48;
	for (int i = 0x22; i <= 0x27; i++) {
		regs[i] = 0x0;
	}
	regs[0x28] = 0x3206;
	regs[0x29] = 0x1206;
	for (int i = 0x2a; i <= 0x50; i++) {
		regs[i] = 0x0;
	}
	regs[0x51] = 0xa;
	for (int i = 0x52; i <= 0x7f; i++) {
		regs[i] = 0x0;
	}

	/* Call generic reset */
	tag_time = regs[BMI3_REG_FIFO_CONF] & (BMI3_FIFO_TIME_EN << 8);
	bmi3xx_emul_reset_common(emul, tag_time);
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
static int bmi3xx_emul_acc_range_to_shift(uint16_t range)
{
	switch ((range >> 4) & 0x7) {
	case BMI3_ACC_RANGE_2G:
		return 0;
	case BMI3_ACC_RANGE_4G:
		return 1;
	case BMI3_ACC_RANGE_8G:
		return 2;
	case BMI3_ACC_RANGE_16G:
		return 3;
	default:
		return 0;
	}
}

/**
 * @brief Convert range in format of GYR_RANGE register to number of bits
 *        that should be shifted right to obtain 16 bit reported gyroscope
 *        value from internal 32 bit value
 *
 * @param range Value of GYR_RANGE register
 *
 * @return shift Number of LSB that should be ignored from internal
 *               gyroscope value
 */
static int bmi3xx_emul_gyr_range_to_shift(uint16_t range)
{
	switch ((range >> 4) & 0x7) {
	case BMI3_GYR_RANGE_2000DPS:
		return 4;
	case BMI3_GYR_RANGE_1000DPS:
		return 3;
	case BMI3_GYR_RANGE_500DPS:
		return 2;
	case BMI3_GYR_RANGE_250DPS:
		return 1;
	case BMI3_GYR_RANGE_125DPS:
		return 0;
	default:
		return 0;
	}
}

/**
 * @brief Convert current time to sensor time (39 us units)
 *
 * @return time in 39 us units
 */
static int64_t bmi3xx_emul_get_sensortime(void)
{
	return k_uptime_ticks() * 1000000 / 39 / CONFIG_SYS_CLOCK_TICKS_PER_SEC;
}

/**
 * @brief Set registers at address @p reg with sensor time that elapsed since
 *        last reset of emulator
 *
 * @param emul Pointer to BMI emulator
 * @param reg Pointer to 3 byte array, where current sensor time should be
 *            stored
 */
static void bmi3xx_emul_set_sensortime_reg(const struct emul *emul,
					   uint16_t *reg)
{
	struct bmi3xx_emul_data *data;
	uint32_t twos_comp_val;
	int64_t time;

	data = emul->data;

	time = bmi3xx_emul_get_sensortime();

	twos_comp_val = bmi3xx_emul_val_to_twos_comp(time - data->zero_time);

	*reg = twos_comp_val & 0xff;
	*(reg + 1) = (twos_comp_val >> 16) & 0xff;
}

/**
 * @brief Convert given sensor axis @p val from internal units to register
 *        units. It shifts value by @p shift bits to the right to account
 *        range set in emulator's registers. Result is saved at address @p reg
 *
 * @param emul Pointer to BMI emulator
 * @param val Accelerometer or gyroscope value in internal units
 * @param reg Pointer to 2 byte array, where sensor value should be stored
 * @param shift How many bits should be shift to the right
 */
static void bmi3xx_emul_set_data_reg(const struct emul *emul, int32_t val,
				     uint16_t *reg, int shift)
{
	struct bmi3xx_emul_data *data;
	uint32_t twos_comp_val;

	printf("\033[33m%s emul 0x%x reg 0x%x\033[m\n", __func__, (int)emul,
	       (int)reg);
	data = emul->data;

	twos_comp_val = bmi3xx_emul_val_to_twos_comp(val);

	/* Shift unused bits because of selected range */
	twos_comp_val >>= shift;

	*reg = twos_comp_val & 0xff;
}

/** Check description in emul_bmi.h */
// TODO add temp
void bmi3xx_emul_state_to_reg(const struct emul *emul, int acc_shift,
			      int gyr_shift, int acc_reg, int gyr_reg,
			      int sensortime_reg, bool acc_off_en,
			      bool gyr_off_en)
{
	struct bmi3xx_emul_data *data;
	int32_t val[3];
	int i;

	printf("\033[33m%s emul 0x%x\033[m\n", __func__, (int)emul);
	data = emul->data;

	if (gyr_off_en) {
		val[0] = data->gyr_x - data->off_gyr_x;
		val[1] = data->gyr_y - data->off_gyr_y;
		val[2] = data->gyr_z - data->off_gyr_z;
	} else {
		val[0] = data->gyr_x;
		val[1] = data->gyr_y;
		val[2] = data->gyr_z;
	}

	for (i = 0; i < 3; i++) {
		bmi3xx_emul_set_data_reg(emul, val[i],
					 &(data->reg[gyr_reg + i]), gyr_shift);
	}

	if (acc_off_en) {
		val[0] = data->acc_x - data->off_acc_x;
		val[1] = data->acc_y - data->off_acc_y;
		val[2] = data->acc_z - data->off_acc_z;
	} else {
		val[0] = data->acc_x;
		val[1] = data->acc_y;
		val[2] = data->acc_z;
	}

	for (i = 0; i < 3; i++) {
		bmi3xx_emul_set_data_reg(emul, val[i],
					 &(data->reg[acc_reg + i]), acc_shift);
	}

	bmi3xx_emul_set_sensortime_reg(emul, &(data->reg[sensortime_reg]));
}

/** Check description in emul_bmi.h */
void bmi3xx_emul_set_cmd_end_time(const struct emul *emul, int time)
{
	struct bmi3xx_emul_data *data;

	data = emul->data;

	data->cmd_end_time = k_uptime_get_32() + time;
}

/**
 * @brief Set given FIFO @p frame as current frame in fifo field of emulator
 *        data structure
 *
 * @param emul Pointer to BMI emulator
 * @param frame Pointer to FIFO frame
 * @param tag_time Indicate if sensor time should be included in empty frame
 * @param header Indicate if header should be included in frame
 * @param acc_shift How many bits should be right shifted from accelerometer
 *                  data
 * @param gyr_shift How many bits should be right shifted from gyroscope data
 * TODO drop header
 */
static void bmi3xx_emul_set_current_frame(const struct emul *emul,
					  struct bmi3xx_emul_frame *frame,
					  bool tag_time, int acc_shift,
					  int gyr_shift)
{
	struct bmi3xx_emul_data *data;
	int i = 0;

	printf("\033[33m%s emul 0x%x\033[m\n", __func__, (int)emul);
	data = emul->data;

	data->fifo_frame_byte = 0;
	data->fifo_frame_len = bmi3xx_emul_get_frame_len(emul, frame);
	/* Empty FIFO frame */
	if (!frame) {
		return;
	}

	/* Sensor data FIFO frame */
	if (frame->type & BMI3XX_EMUL_FRAME_ACC) {
		bmi3xx_emul_set_data_reg(emul, frame->acc_x, &(data->fifo[i]),
					 acc_shift);
		i += 1;
		bmi3xx_emul_set_data_reg(emul, frame->acc_y, &(data->fifo[i]),
					 acc_shift);
		i += 1;
		bmi3xx_emul_set_data_reg(emul, frame->acc_z, &(data->fifo[i]),
					 acc_shift);
		i += 1;
	}
	if (frame->type & BMI3XX_EMUL_FRAME_GYR) {
		bmi3xx_emul_set_data_reg(emul, frame->gyr_x, &(data->fifo[i]),
					 gyr_shift);
		i += 1;
		bmi3xx_emul_set_data_reg(emul, frame->gyr_y, &(data->fifo[i]),
					 gyr_shift);
		i += 1;
		bmi3xx_emul_set_data_reg(emul, frame->gyr_z, &(data->fifo[i]),
					 gyr_shift);
		i += 1;
	}
	if (frame->type & BMI3XX_EMUL_FRAME_TEMP) {
		bmi3xx_emul_set_data_reg(emul, frame->temp, &(data->fifo[i]),
					 0);
	}

	if (frame->type & BMI3XX_EMUL_FRAME_TIME) {
		uint16_t time_buf[2];

		bmi3xx_emul_set_sensortime_reg(emul, &(time_buf[0]));
		/* fetch the first 16-bit word only. */
		data->fifo[i] = time_buf[0];
		i += 1;
	}
}

/** Check description in emul_bmi.h */
void bmi3xx_emul_flush_fifo(const struct emul *emul, bool tag_time)
{
	struct bmi3xx_emul_data *data;

	printf("\033[33m%s emul 0x%x\033[m\n", __func__, (int)emul);
	data = emul->data;

	data->fifo_skip = 0;
	data->fifo_frame = NULL;
	/*
	 * Gyroscope and accelerometer shift (last two arguments)
	 * are not important for NULL (empty) FIFO frame.
	 */
	bmi3xx_emul_set_current_frame(emul, NULL, tag_time, 0, 0);
}

/** Check description in emul_bmi.h */
void bmi3xx_emul_reset_common(const struct emul *emul, bool tag_time)
{
	struct bmi3xx_emul_data *data;

	printf("\033[33m%s emul 0x%x\033[m\n", __func__, (int)emul);

	data = emul->data;

	/* Flush FIFO */
	bmi3xx_emul_flush_fifo(emul, tag_time);

	/* Reset sensor timer */
	data->zero_time = bmi3xx_emul_get_sensortime();
}

/**
 * @brief Execute first part of command. Emulate state of device which is
 *        during handling command (status bits etc). This function save time
 *        on which command should end.
 *
 * @param regs Pointer to array of emulator's registers
 * @param emul Pointer to BMI emulator
 * @param cmd Command that is starting
 *
 * @return 0 on success
 * @return -EIO on failure
 */
static int bmi3xx_emul_start_cmd(const struct emul *emul, int cmd)
{
	int time;
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;

	printf("\033[33m%s emul 0x%x regs 0x%x cmd 0x%x\033[m\n", __func__,
	       (int)emul, (int)regs, cmd);
	switch (cmd) {
	case BMI3_CMD_SOFT_RESET:
		time = 1;
		break;
	default:
		LOG_ERR("Unknown command 0x%x", cmd);
		return -EIO;
	}

	regs[BMI3_REG_CMD] = cmd;
	bmi3xx_emul_set_cmd_end_time(emul, time);

	return 0;
}

/**
 * @brief Emulate end of ongoing command.
 *
 * @param regs Pointer to array of emulator's registers
 * @param emul Pointer to BMI emulator
 */
static void bmi3xx_emul_end_cmd(const struct emul *emul)
{
	bool tag_time;
	int cmd;
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;

	printf("\033[33m%s emul 0x%x reg 0x%x\033[m\n", __func__, (int)emul,
	       (int)regs);

	cmd = regs[BMI3_REG_CMD];
	regs[BMI3_REG_CMD] = 0;
	tag_time = regs[BMI3_REG_FIFO_CONF] & (BMI3_FIFO_TIME_EN << 8);

	switch (cmd) {
	case BMI3_CMD_SOFT_RESET:
		bmi323_emul_reset(emul);
		break;
	default:
		break;
	}
}

/** Check description in emul_bmi.h */
bool bmi3xx_emul_is_cmd_end(const struct emul *emul)
{
	struct bmi3xx_emul_data *data;

	data = emul->data;

	/* We are simulating command execution time and it doesn't expired */
	if (data->simulate_command_exec_time &&
	    data->cmd_end_time > k_uptime_get_32()) {
		return false;
	}

	return true;
}

/**
 * @brief Get currently accessed register. It is first register plus number of
 *        handled bytes for all registers except BMI3_REG_FIFO_DATA for which
 *        address incrementation is disabled.
 *
 * @param emul Pointer to BMI emulator
 * @param reg First byte of last write message
 * @param bytes Number of bytes already handled from current message
 * @param read If currently handled is read message
 *
 * @return Currently accessed register
 */
static int bmi323_emul_access_reg(const struct emul *emul, int reg, int byte,
				  bool read)
{
	printf("\033[33m%s emul 0x%x reg 0x%x byte 0x%d read %d\033[m\n",
	       __func__, (int)emul, reg, byte, read);
	/* Ignore first byte which sets starting register */
	if (!read) {
		byte -= 1;
	}

	/*
	 * If register is FIFO data, then read data from FIFO.
	 * Init data is also block, but it is not implemented in emulator.
	 * Else block read access subsequent registers.
	 */
	if (reg <= BMI3_REG_FIFO_DATA && reg + byte >= BMI3_REG_FIFO_DATA) {
		return BMI3_REG_FIFO_DATA;
	}

	return reg;
	/* return reg + byte; */
}

/** Check description in emul_bmi.h */
uint16_t bmi3xx_emul_get_fifo_data(const struct emul *emul, int byte,
				   bool tag_time, int acc_shift, int gyr_shift)
{
	struct bmi3xx_emul_data *data;
	int fifo_data;

	printf("\033[33m%s 0x%x 0x%d\033[m\n", __func__, (int)emul, byte);
	data = emul->data;

	if (byte == 0) {
		/* Repeat uncompleated read of frame */
		bmi3xx_emul_set_current_frame(emul, data->fifo_frame, tag_time,
					      acc_shift, gyr_shift);
	}

	/* Get next valid frame */
	while (data->fifo_frame_byte >= data->fifo_frame_len) {
		/* No data */
		if (data->fifo_frame == NULL) {
			return 0;
		}
		data->fifo_frame = data->fifo_frame->next;
		bmi3xx_emul_set_current_frame(emul, data->fifo_frame, tag_time,
					      acc_shift, gyr_shift);
	}

	fifo_data = data->fifo[data->fifo_frame_byte];
	data->fifo_frame_byte += 2;

	return fifo_data;
}

/**
 * @brief BMI3XX specific write function. It handle block writes. Init data
 *        register is trap register, so after reaching it, register address
 *        is not increased on block writes. Check if read only register is not
 *        accessed. Before writing value, ongoing command is finished if
 *        possible. Write to CMD register is handled by BMI3xx specific
 *        function. On changing of FIFO header/headerless mode or
 *        enabling/disabling sensor in headerless mode FIFO is flushed.
 *
 * @param regs Pointer to array of emulator's registers
 * @param emul Pointer to BMI emulator
 * @param reg Register address that is accessed
 * @param byte Number of handled bytes in this write command
 * @param val Value that is being written
 *
 * @return 0 on success
 * @return BMI3XX_EMUL_ACCESS_E on RO register access
 * @return -EIO on error
 */
static int bmi323_emul_handle_write(const struct emul *emul, int reg, int byte,
				    uint16_t val)
{
	uint16_t mask;
	bool tag_time;
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;

	printf("\033[34m%s emul 0x%x 0x%x 0x%d\033[m\n", __func__, (int)emul,
	       reg, byte);
	reg = bmi323_emul_access_reg(emul, reg, byte, false /* = read */);

	if (reg < BMI3_REG_FIFO_DATA || reg > BMI3_REG_CMD) {
		return BMI3XX_EMUL_ACCESS_E;
	}

	tag_time = regs[BMI3_REG_FIFO_CONF] & (BMI3_FIFO_TIME_EN << 8);

	switch (reg) {
	case BMI3_REG_FIFO_CONF:
		/*
		 * Clear FIFO on enabling/disabling sensors in headerless
		 * mode
		 */
		mask = BMI3_FIFO_ALL_EN << 8;
		if ((val & mask) != (regs[BMI3_REG_FIFO_CONF] & mask)) {
			bmi3xx_emul_flush_fifo(emul, tag_time);
		}
		break;
	}

	return 0;
}

static int bmi323_emul_start_read(const struct emul *emul, int reg)
{
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;

	printf("\033[34m%s emul 0x%x reg 0x%x\033[m\n", __func__, (int)emul,
	       reg);

	/* Stop on going command if required */
	if (regs[BMI3_REG_CMD] && bmi3xx_emul_is_cmd_end(emul)) {
		bmi3xx_emul_end_cmd(emul);
	}

	return 0;
}

static int bmi3xx_emul_start_read(const struct emul *emul, int reg)
{
	struct bmi3xx_emul_data *data;
	int ret;

	printf("\033[32m%s emul 0x%x reg 0x%x\033[m\n", __func__, (int)emul,
	       reg);
	data = emul->data;

	if (data->type_data->start_read == NULL) {
		return 0;
	}
	ret = data->type_data->start_read(emul, reg);
	return ret;

	return 0;
}

/**
 * @brief Handle I2C read message. BMI model specific read function is called.
 *        It is checked if accessed register isn't WO.
 *
 * @param emul Pointer to BMI emulator
 * @param reg Register address to read
 * @param buf Pointer where result should be stored
 * @param byte Byte which is accessed during block read
 *
 * @return 0 on success
 * @return -EIO on error
 */
static int bmi3xx_emul_handle_read(const struct emul *emul, int reg,
				   uint8_t *buf, int byte)
{
	struct bmi3xx_emul_data *data = emul->data;
	int ret;

	printf("\033[32m%s emul 0x%x reg 0x%x byte 0x%x\033[m\n", __func__,
	       (int)emul, reg, byte);

	ret = data->type_data->handle_read(emul, reg, byte, buf);
	reg = data->type_data->access_reg(emul, reg, byte, true /* = read */);
	if (ret == BMI3XX_EMUL_ACCESS_E) {
		LOG_ERR("Reading reg 0x%x which is WO", reg);
	} else if (ret != 0) {
		return ret;
	}

	return 0;
}

/**
 * @brief BMI3XX specific read function. It handle block reads. FIFO data
 *        register and init data register are trap registers, so
 *        after reaching it, register address is not increased on block reads.
 *        Before reading value, ongoing command is finished if possible.
 *        Read of sensor data traps current emulator state in registers.
 *        Read of FIFO length and FIFO data triggers default BMI functions.
 *
 * @param regs Pointer to array of emulator's registers
 * @param emul Pointer to BMI emulator
 * @param reg Register address that is accessed
 * @param byte Byte which is accessed during block read
 * @param buf Pointer where read byte should be stored
 *
 * @return 0 on success
 * @return BMI3XX_EMUL_ACCESS_E on WO register access
 * @return -EIO on other error
 */
static int bmi323_emul_handle_read(const struct emul *emul, int reg, int byte,
				   uint8_t *buf)
{
	uint16_t fifo_len;
	bool acc_off_en;
	bool gyr_off_en;
	bool tag_time;
	int gyr_shift;
	int acc_shift;
	int fifo_byte;
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;
	// struct bmi3xx_emul_data *data;

	printf("\033[32m%s emul 0x%x reg 0x%x byte 0x%d buf 0x%x\033[m\n",
	       __func__, (int)emul, reg, byte, (int)buf);
	/* Get number of bytes readed from FIFO */
	fifo_byte = byte - (reg - BMI3_REG_FIFO_DATA);

	reg = bmi323_emul_access_reg(emul, reg, byte, true /* = read */);

	if (reg == BMI3_REG_CMD) {
		*buf = 0;

		return BMI3XX_EMUL_ACCESS_E;
	}

	tag_time = regs[BMI3_REG_FIFO_CONF] & (BMI3_FIFO_TIME_EN << 8);
	acc_off_en = true;
	gyr_off_en = true;
	gyr_shift = bmi3xx_emul_gyr_range_to_shift(regs[BMI3_REG_GYR_CONF]);
	acc_shift = bmi3xx_emul_acc_range_to_shift(regs[BMI3_REG_ACC_CONF]);

	switch (reg) {
	case BMI3_REG_ACC_DATA_X:
	case BMI3_REG_ACC_DATA_Y:
	case BMI3_REG_ACC_DATA_Z:
	case BMI3_REG_GYR_DATA_X:
	case BMI3_REG_GYR_DATA_Y:
	case BMI3_REG_GYR_DATA_Z:
	case BMI3_REG_TEMP_DATA:
	case BMI3_REG_SENSOR_TIME_0:
	case BMI3_REG_SENSOR_TIME_1:
		/*
		 * Snapshot of current emulator state is created on data read
		 * and shouldn't be changed until next I2C operation
		 */
		if (byte == 0) {
			bmi3xx_emul_state_to_reg(emul, acc_shift, gyr_shift,
						 BMI3_REG_ACC_DATA_X,
						 BMI3_REG_GYR_DATA_X,
						 BMI3_REG_SENSOR_TIME_0,
						 acc_off_en, gyr_off_en);
		}
		break;
	case BMI3_REG_FIFO_FILL_LVL:
		if (byte == 0) {
			fifo_len = bmi3xx_emul_fifo_len(emul, tag_time);
			regs[BMI3_REG_FIFO_FILL_LVL] = fifo_len & 0x7ff;
		}
		break;
	case BMI3_REG_FIFO_DATA:
		regs[reg] = bmi3xx_emul_get_fifo_data(emul, fifo_byte, tag_time,
						      acc_shift, gyr_shift);
		break;
	}

	/* the first two bytes are dummies */
	if (byte <= 1)
		*buf = 0;
	else
		*buf = regs[reg] >> ((byte - 2) * 8);

	printf("\033[32m%s emul 0x%x reg 0x%x byte 0x%d buf 0x%x = 0x%x\033[m\n",
	       __func__, (int)emul, reg, byte, (int)buf, *buf);
	return 0;
}

static int bmi3xx_emul_finish_read(const struct emul *emul, int reg, int bytes)
{
	struct bmi3xx_emul_data *data = emul->data;
	int ret;

	printf("\033[32m%s emul 0x%x reg 0x%x bytes 0x%d\033[m\n", __func__,
	       (int)emul, reg, bytes);

	if (data->type_data->finish_read == NULL) {
		return 0;
	}
	ret = data->type_data->finish_read(emul, reg, bytes);
	return ret;
}

static int bmi3xx_emul_start_write(const struct emul *emul, int reg)
{
	struct bmi3xx_emul_data *data = emul->data;
	int ret;

	printf("\033[34m%s emul 0x%x reg 0x%x\033[m\n", __func__, (int)emul,
	       reg);

	if (data->type_data->start_write == NULL) {
		return 0;
	}
	ret = data->type_data->start_write(emul, reg);

	return ret;
}

static int bmi323_emul_start_write(const struct emul *emul, int reg)
{
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;

	printf("\033[34m%s emul 0x%x reg 0x%x\033[m\n", __func__, (int)emul,
	       reg);

	/* Stop on going command if required */
	if (regs[BMI3_REG_CMD] && bmi3xx_emul_is_cmd_end(emul)) {
		bmi3xx_emul_end_cmd(emul);
	}

	return 0;
}

static int bmi3xx_emul_finish_write(const struct emul *emul, int reg, int bytes)
{
	struct bmi3xx_emul_data *data = emul->data;
	int ret;

	printf("\033[34m%s emul 0x%x reg 0x%x bytes 0x%d\033[m\n", __func__,
	       (int)emul, reg, bytes);

	if (data->type_data->finish_write == NULL) {
		return 0;
	}
	ret = data->type_data->finish_write(emul, reg, bytes);
	return ret;
}

static int bmi323_emul_finish_write(const struct emul *emul, int reg, int bytes)
{
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;

	printf("\033[34m%s emul 0x%x reg 0x%x bytes 0x%d\033[m\n", __func__,
	       (int)emul, reg, bytes);

	switch (reg) {
	case BMI3_REG_CMD:
		return bmi3xx_emul_start_cmd(emul, regs[reg]);
	default:
		break;
	}

	return 0;
}

/**
 * @brief Handle I2C write message. BMI model specific write function is called.
 *        It is checked if accessed register isn't RO and reserved bits are set
 *        to 0. Write set value of reg field of bmi emulator data ignoring
 *        reserved bits. If required internal sensor offset values are updated.
 *
 * @param emul Pointer to BMI emulator
 * @param reg Register which is written
 * @param val Value being written to @p reg
 * @param byte Number of handled bytes in this write command
 *
 * @return 0 on success
 * @return -EIO on error
 */
static int bmi3xx_emul_handle_write(const struct emul *emul, int reg,
				    uint16_t val, int byte)
{
	struct bmi3xx_emul_data *data = emul->data;
	uint16_t *regs = data->reg;
	uint16_t rsvd_mask;
	int ret;

	printf("\033[34m%s emul 0x%x reg 0x%x val 0x%x byte 0x%x\033[m\n",
	       __func__, (int)emul, reg, val, byte);
	data = emul->data;

	ret = data->type_data->handle_write(emul, reg, byte, val);
	reg = data->type_data->access_reg(emul, reg, byte, false /* = read */);
	if (ret != 0) {
		if (ret == BMI3XX_EMUL_ACCESS_E) {
			LOG_ERR("Writing to reg 0x%x which is RO", reg);
		}

		return -EIO;
	}

	rsvd_mask = (data->type_data->rsvd_mask[reg]) >> (8 * (byte - 1));

	if (rsvd_mask & val) {
		LOG_ERR("Writing 0x%x to reg 0x%x with rsvd bits mask 0x%x",
			val, reg, rsvd_mask);
		return -EIO;
	}

	/* Ignore all reserved bits */
	val &= ~rsvd_mask;
	val |= regs[reg] & rsvd_mask;

	regs[reg] = (regs[reg] & (0xff << (8 * (2 - byte)))) |
		    val << (8 * (byte - 1));
	printf("\033[35mdata->reg[0x%x]=0x%x\033[m\n", reg, regs[reg]);

	return 0;
}

/** Confguration of BMI3XX */
struct bmi3xx_emul_type_data bmi3xx_emul = {
	.handle_write = bmi323_emul_handle_write,
	.handle_read = bmi323_emul_handle_read,
	.access_reg = bmi323_emul_access_reg,
	.reset = bmi323_emul_reset,
	.rsvd_mask = bmi323_emul_rsvd_mask,
	.start_read = bmi323_emul_start_read,
	.finish_read = NULL,
	.start_write = bmi323_emul_start_write,
	.finish_write = bmi323_emul_finish_write,
	.gyr_off_reg = BMI3_GYR_DP_OFF_X,
	.acc_off_reg = BMI3_ACC_DP_OFF_X,
};

/** Check description in emul_bmi.h */
const struct bmi3xx_emul_type_data *get_bmi3xx_emul_type_data(void)
{
	return &bmi3xx_emul;
}

/* Device instantiation */

/**
 * @brief Set up a new BMI3XX emulator
 *
 * This should be called for each BMI device that needs to be
 * emulated. It registers it with the I2C emulation controller.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */

int bmi3xx_emul_init(const struct emul *emul, const struct device *parent)
{
	struct bmi3xx_emul_data *data = emul->data;

	printf("\033[33m%s emul 0x%x\033[m\n", __func__, (int)emul);
	data->common.i2c = parent;
	i2c_common_emul_init(&data->common);

	switch (data->type) {
	case BMI3XX_EMUL_3XX:
		data->type_data = get_bmi3xx_emul_type_data();
		break;
	}

	/* Set callback access_reg to type specific function */
	data->common.access_reg = data->type_data->access_reg;

	data->type_data->reset(emul);

	return 0;
}

#define BMI3XX_EMUL(n)                                                  \
	static struct bmi3xx_emul_data bmi3xx_emul_data_##n = {		\
		.type = DT_STRING_TOKEN(DT_DRV_INST(n), device_model),	\
		.common = {						\
			.start_write = bmi3xx_emul_start_write,		\
			.write_byte = bmi3xx_emul_handle_write,		\
			.finish_write = bmi3xx_emul_finish_write,	\
			.start_read = bmi3xx_emul_start_read, \
			.read_byte = bmi3xx_emul_handle_read,		\
			.finish_read = bmi3xx_emul_finish_read,		\
			.access_reg = NULL,				\
		},							\
	};      \
	static const struct i2c_common_emul_cfg bmi3xx_emul_cfg_##n = { \
		.dev_label = DT_NODE_FULL_NAME(DT_DRV_INST(n)),         \
		.data = &bmi3xx_emul_data_##n.common,                   \
		.addr = DT_INST_REG_ADDR(n),                            \
	};                                                              \
	EMUL_DT_INST_DEFINE(n, bmi3xx_emul_init, &bmi3xx_emul_data_##n, \
			    &bmi3xx_emul_cfg_##n, &i2c_common_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(BMI3XX_EMUL)

DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);

struct i2c_common_emul_data *
emul_bmi3xx_get_i2c_common_data(const struct emul *emul)
{
	return emul->data;
}
