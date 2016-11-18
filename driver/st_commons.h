/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Commons acc/gyro function for ST sensors oin Chrome EC */

#ifndef __CROS_EC_ST_COMMONS_H
#define __CROS_EC_ST_COMMONS_H

#include "accelgyro.h"
#include "driver/accel_lis2dh.h"
#include "driver/accelgyro_lsm6dsm.h"

#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/* Set auto increment subaddress on multiple i2c access */
#define I2C_AUTO_INC			0x80

/* X, Y, Z axis data len */
#define OUT_XYZ_SIZE			6

#ifdef CONFIG_ACCEL_FIFO
/* Define number of data to be read from FIFO each time
 * It must be a multiple of LIS2DH_OUT_XYZ_SIZE */
#define FIFO_READ_LEN			(10 * OUT_XYZ_SIZE)
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
 * @temp: Temp. 
 */
int set_offset(const struct motion_sensor_t *s,
	       const int16_t *offset, int16_t temp);

/**
 * get_offset - Get data offset
 * @s: Motion sensor pointer
 * @offset: offset vector
 * @temp: Temp. 
 */
int get_offset(const struct motion_sensor_t *s,
	       int16_t *offset, int16_t *temp);

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
	/* Define FIFO buffer to store data and process it */
	uint8_t fifo[FIFO_READ_LEN];
#endif /* CONFIG_ACCEL_FIFO */
};

#endif /* __CROS_EC_ST_COMMONS_H */
