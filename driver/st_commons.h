/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Commons acc/gyro function for ST sensors oin Chrome EC */

#ifndef __CROS_EC_ST_COMMONS_H
#define __CROS_EC_ST_COMMONS_H

#include "common.h"
#include "util.h"
#include "accelgyro.h"
#include "console.h"
#include "i2c.h"
#include "driver/accel_lis2dh.h"
#include "driver/accelgyro_lsm6dsm.h"

#ifdef CONFIG_ST_SENSORS_DEBUG
/* Debug level variable */
extern int debug;

/* Common debug funcions */
#define CPRINTF(format, args...) if (debug) \
					cprintf(CC_ACCEL, format "\n", ## args)
#else /* CONFIG_ST_SENSORS_DEBUG */
#define CPRINTF(format, args...)
#endif /* CONFIG_ST_SENSORS_DEBUG */

/* MIN_AZ - Calculate min between two integer (zero skip) */
static inline uint16_t MIN_AZ(int _a, int _b)
{
	if (_a < _b) {
		if (_a == 0)
			return _b;
		return _a;
	} else if (_b == 0)
		return _a;

	return _b;
}

/* Set auto increment subaddress on multiple access */
#define AUTO_INC			0x80

/* X, Y, Z axis data len */
#define OUT_XYZ_SIZE			6

/* Common define for poswer off ODR */
#define ODR_POWER_OFF_VAL		0x00

#ifdef CONFIG_ACCEL_FIFO
/* Define min data size in FIFO for each elements stored */
#define FIFO_EL_NBYTE			OUT_XYZ_SIZE
/* Sensor FIFO buffer in pattern
 * FIFO size will be FIFO_EL_NBYTE * FIFO_BUFFER_NUM_PATTERN
 */
#define FIFO_BUFFER_NUM_PATTERN		16
/* Define number of data to be read from FIFO each time
 * It must be a multiple of OUT_XYZ_SIZE.
 * In case of LSM6DSM FIFO contains pattern depending ODR
 * of Acc/gyro, be sure that FIFO can contains almost
 * FIFO_BUFFER_NUM_PATTERNpattern
 */
#define FIFO_READ_LEN			(FIFO_BUFFER_NUM_PATTERN * FIFO_EL_NBYTE)

#endif /* CONFIG_ACCEL_FIFO */

/**
 * Read single register
 */
inline int raw_read8(const int port, const int addr, const int reg,
		     int *data_ptr);

/**
 * Write single register
 */
inline int raw_write8(const int port, const int addr, const int reg,
		      int data);

/**
 * Read n bytes for read
 * NOTE: Some chip use MSB for auto-increments in SUB address
 * 		 MSB must be set for autoincrement in multi read when auto_inc
 * 		 is set
 */
int raw_read_n(const int port, const int addr, const uint8_t reg,
	       uint8_t *data_ptr, const int len, int auto_inc);

 /**
 * write_data_with_mask - Write register with mask
 * @s: Motion sensor pointer
 * @reg: Device register
 * @mask: The mask to search
 * @data: Data pointer
 */
int write_data_with_mask(const struct motion_sensor_t *s, int reg,
			 uint8_t mask, uint8_t data);

 /**
 * set_resolution - Set bit resolution
 * @s: Motion sensor pointer
 * @res: Bit resolution
 * @rnd: Round bit
 */
int set_resolution(const struct motion_sensor_t *s, int res, int rnd);

/**
 * set_offset - Set data offset
 * @s: Motion sensor pointer
 * @offset: offset vector
 * @temp: Temp
 */
int set_offset(const struct motion_sensor_t *s,
	       const int16_t *offset, int16_t temp);

/**
 * get_offset - Get data offset
 * @s: Motion sensor pointer
 * @offset: offset vector
 * @temp: Temp
 */
int get_offset(const struct motion_sensor_t *s, int16_t *offset, int16_t *temp);

/**
 * get_data_rate - Get data rate (ODR)
 * @s: Motion sensor pointer
 */
int get_data_rate(const struct motion_sensor_t *s);

/**
 * normalize - Apply to LSB data sensitivity and rotation
 * @s: Motion sensor pointer
 * @v: vector
 * @data: LSB raw data
 */
void normalize(const struct motion_sensor_t *s, int *axis, uint8_t *data);

/* Internal data structure for sensors */
struct stprivate_data {
	struct accelgyro_saved_data_t base;
	int16_t offset[3];
	uint8_t resol;
#ifdef CONFIG_ACCEL_FIFO
	/* Define FIFO buffer to store data */
	uint8_t *fifo;
	uint8_t samples_in_pattern;
	uint16_t num_pattern;
#endif /* CONFIG_ACCEL_FIFO */
};

#endif /* __CROS_EC_ST_COMMONS_H */
