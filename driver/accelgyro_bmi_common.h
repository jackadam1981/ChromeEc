/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* BMI accelerometer and gyro common definitions for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_BMI_COMMON_H
#define __CROS_EC_ACCELGYRO_BMI_COMMON_H

#ifdef CONFIG_ACCELGYRO_BMI160
#include "driver/accelgyro_bmi160.h"
#else
#include "driver/accelgyro_bmi260.h"
#endif

#include "i2c.h"
#include "spi.h"

/* odr = 100 / (1 << (8 - reg)) , within limit */
#define BMI_ODR_0_78HZ       0x01
#define BMI_ODR_100HZ        0x08

#define BMI_REG_TO_ODR(_regval) \
	((_regval) < BMI_ODR_100HZ ? 100000 / (1 << (8 - (_regval))) : \
					100000 * (1 << ((_regval) - 8)))
#define BMI_ODR_TO_REG(_odr) \
	((_odr) < 100000 ? (__builtin_clz(100000 / ((_odr) + 1)) - 24) : \
			   (39 - __builtin_clz((_odr) / 100000)))

enum fifo_header {
	BMI_FH_EMPTY = 0x80,
	BMI_FH_SKIP = 0x40,
	BMI_FH_TIME = 0x44,
	BMI_FH_CONFIG = 0x48
};

#define BMI_FH_MODE_MASK     0xc0
#define BMI_FH_PARM_OFFSET       2
#define BMI_FH_PARM_MASK         (0x7 << BMI_FH_PARM_OFFSET)
#define BMI_FH_EXT_MASK      0x03

/* Sensor resolution in number of bits. This sensor has fixed resolution. */
#define BMI_RESOLUTION      16

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct bmi_accel_param_pair {
	int val; /* Value in engineering units. */
	int reg_val; /* Corresponding register value. */
};

int get_xyz_reg(enum motionsensor_type type);

/**
 * @param type   Accel/Gyro
 * @param psize  Size of the table
 *
 * @return       Range table of the type.
 */
const struct bmi_accel_param_pair *bmi_get_range_table(
		enum motionsensor_type type, int *psize);

/**
 * @return reg value that matches the given engineering value passed in.
 * The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid reg value. If the request is
 * outside the range of values, it returns the closest valid reg value.
 */
int bmi_get_reg_val(const int eng_val, const int round_up,
		    const struct bmi_accel_param_pair *pairs,
		    const int size);

/**
 * @return engineering value that matches the given reg val
 */
int bmi_get_engineering_val(const int reg_val,
			    const struct bmi_accel_param_pair *pairs,
			    const int size);

#ifdef CONFIG_SPI_ACCEL_PORT
int bmi_spi_raw_read(const int addr, const uint8_t reg,
		     uint8_t *data, const int len);
#endif

/**
 * Read 8bit register from accelerometer.
 */
int bmi_read8(const int port, const uint16_t i2c_spi_addr_flags,
	      const int reg, int *data_ptr);

/**
 * Write 8bit register from accelerometer.
 */
int bmi_write8(const int port, const uint16_t i2c_spi_addr_flags,
	       const int reg, int data);

/**
 * Read 16bit register from accelerometer.
 */
int bmi_read16(const int port, const uint16_t i2c_spi_addr_flags,
	       const uint8_t reg, int *data_ptr);

/**
 * Write 16bit register from accelerometer.
 */
int bmi_write16(const int port, const uint16_t i2c_spi_addr_flags,
		const int reg, int data);

/**
 * Read 32bit register from accelerometer.
 */
int bmi_read32(const int port, const uint16_t i2c_spi_addr_flags,
	       const uint8_t reg, int *data_ptr);

/**
 * Read n bytes from accelerometer.
 */
int bmi_read_n(const int port, const uint16_t i2c_spi_addr_flags,
	       const uint8_t reg, uint8_t *data_ptr, const int len);

/**
 * Write n bytes from accelerometer.
 */
int bmi_write_n(const int port, const uint16_t i2c_spi_addr_flags,
		const uint8_t reg, uint8_t *data_ptr, const int len);

/*
 * Enable specific bit set of a 8-bit reg.
 */
int enable_reg8_bits(const struct motion_sensor_t *s, int reg, uint8_t bits);

/*
 * Disable specific bit set of a 8-bit reg.
 */
int disable_reg8_bits(const struct motion_sensor_t *s, int reg, uint8_t bits);

/*
 * @s: base sensor.
 * @v: output vector.
 * @input: 6-bits array input.
 */
void bmi_normalize(const struct motion_sensor_t *s, intv3_t v, uint8_t *input);

/*
 * Decode the header from the fifo.
 * Return 0 if we need further processing.
 * Sensor mutex must be held during processing, to protect the fifos.
 *
 * @accel: base sensor
 * @hdr: the header to decode
 * @last_ts: the last timestamp of fifo interrupt.
 * @bp: current pointer in the buffer, updated when processing the header.
 * @ep: pointer to the end of the valid data in the buffer.
 */
int bmi_decode_header(struct motion_sensor_t *accel,
		      enum fifo_header hdr, uint32_t last_ts,
		      uint8_t **bp, uint8_t *ep);

int bmi_set_range(const struct motion_sensor_t *s, int range, int rnd);

int bmi_get_range(const struct motion_sensor_t *s);

int bmi_get_resolution(const struct motion_sensor_t *s);

int bmi_set_scale(const struct motion_sensor_t *s,
	      const uint16_t *scale, int16_t temp);

int bmi_get_scale(const struct motion_sensor_t *s,
	      uint16_t *scale, int16_t *temp);

#endif /* __CROS_EC_ACCELGYRO_BMI_COMMON_H */
