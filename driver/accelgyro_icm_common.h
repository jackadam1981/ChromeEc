/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ICM accelerometer and gyroscope common definitions for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_ICM_COMMON_H
#define __CROS_EC_ACCELGYRO_ICM_COMMON_H

#include "accelgyro.h"

struct icm_chip_desc {
	uint8_t whoami;
	const char *name;
};

#ifdef CONFIG_ACCEL_FIFO
/* reserve maximum 4 samples of 16 bytes */
#define ICM_FIFO_BUFFER	64
#else
#define ICM_FIFO_BUFFER	0
#endif

struct icm_drv_data_t {
	struct accelgyro_saved_data_t saved_data[2];
	const struct icm_chip_desc *chip;
	uint8_t bank;
	struct motion_sensor_t *accel;
	struct motion_sensor_t *gyro;
	uint8_t fifo_en;
	uint8_t fifo_buffer[ICM_FIFO_BUFFER] __aligned(sizeof(long));
};

#define ICM_GET_DATA(_s) \
	((struct icm_drv_data_t *)(_s)->drv_data)
#define ICM_GET_SAVED_DATA(_s) \
	(&ICM_GET_DATA(_s)->saved_data[(_s)->type])

/*
 * Virtual register address is 16 bits:
 * - 8 bits MSB coding bank number
 * - 8 bits LSB coding physical address
 */
#define ICM426XX_REG_GET_BANK(_r)	(((_r) & 0xFF00) >> 8)
#define ICM426XX_REG_GET_ADDR(_r)	((_r) & 0x00FF)

/* Sensor resolution in number of bits */
#define ICM_RESOLUTION		16

/**
 * sign_extend - sign extend a standard int value using the given sign-bit
 * @value: value to sign extend
 * @index: 0 based bit index to sign bit
 */
static inline int sign_extend(int value, int index)
{
	int shift = (sizeof(int) * 8) - index - 1;

	return (int)(value << shift) >> shift;
}

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct icm_param_pair {
	int val;	/* Value in engineering units. */
	int reg_val;	/* Corresponding register value. */
};

/**
 * @return the pair that matches the given engineering value passed in.
 * The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid pair. If the request is
 * outside the range of values, it returns the closest valid pair.
 */
const struct icm_param_pair *
icm_get_pair(const int eng_val, const int round_up,
	     const struct icm_param_pair *pairs, const unsigned int len);

/**
 * Read 8 bits register
 */
int icm_read8(const struct motion_sensor_t *s, const int reg, int *data_ptr);

/**
 * Write 8 bits register
 */
int icm_write8(const struct motion_sensor_t *s, const int reg, int data);

/**
 * Read 16 bits register
 */
int icm_read16(const struct motion_sensor_t *s, const int reg, int *data_ptr);

/**
 * Write 16 bits register
 */
int icm_write16(const struct motion_sensor_t *s, const int reg, int data);

/**
 * Read n bytes
 */
int icm_read_n(const struct motion_sensor_t *s, const int reg,
	       uint8_t *data_ptr, const int len);

int icm_field_update8(const struct motion_sensor_t *s, const int reg,
		      const uint8_t field_mask, const uint8_t set_value);

int icm_get_resolution(const struct motion_sensor_t *s);

int icm_get_range(const struct motion_sensor_t *s);

int icm_get_data_rate(const struct motion_sensor_t *s);

int icm_set_scale(const struct motion_sensor_t *s, const uint16_t *scale,
		  int16_t temp);

int icm_get_scale(const struct motion_sensor_t *s, uint16_t *scale,
		  int16_t *temp);

ssize_t icm_fifo_decode_packet(const void *packet, const void **accel,
			       const void **gyro, const void **temp,
			       const void **timestamp, unsigned int *odr);

#endif	/* __CROS_EC_ACCELGYRO_ICM_COMMON_H */
