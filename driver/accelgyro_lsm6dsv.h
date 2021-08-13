/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DSV Accel and Gyro driver for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_LSM6DSV_H
#define __CROS_EC_ACCELGYRO_LSM6DSV_H

#include "stm_mems_common.h"

/*
 * 7-bit address is 110101xb. Where 'x' is determined
 * by the voltage on the ADDR pin
 */
#define LSM6DSV_ADDR0_FLAGS		0x6a
#define LSM6DSV_ADDR1_FLAGS		0x6b

/* Access to embedded sensor hub register bank */
#define LSM6DSV_FUNC_CFG_ACC_ADDR	0x01
#define LSM6DSV_FUNC_CFG_EN_MASK	0x80

/* Interrupt configuration */
#define LSM6DSV_IF_CFG			0x03
#define LSM6DSV_H_LACTIVE		0x10

/* Who Am I */
#define LSM6DSV_WHO_AM_I_REG		0x0f
#define LSM6DSV_WHO_AM_I		0x70

/* Common defines for Acc and Gyro sensors */
#define LSM6DSV_EN_BIT			0x01
#define LSM6DSV_DIS_BIT			0x00

#define LSM6DSV_CTRL1_ADDR		0x10
#define LSM6DSV_CTRL2_ADDR		0x11
#define LSM6DSV_CTRL3_ADDR		0x12
#define LSM6DSV_SW_RESET_MASK		0x01
#define LSM6DSV_IF_INC_MASK		0x04
#define LSM6DSV_BDU_MASK		0x40
#define LSM6DSV_BOOT_MASK		0x80

#define LSM6DSV_CTRL4_ADDR		0x13
#define LSM6DSV_INT2_ON_INT1_MASK	0x10

#define LSM6DSV_CTRL6_ADDR		0x15
#define LSM6DSV_CTRL7_ADDR		0x16
#define LSM6DSV_QVAR_ENABLE_MASK	0x80
#define LSM6DSV_INT2_DRDY_QVAR_MASK	0x40
#define LSM6DSV_QVAR_C_ZIN_MASK		0x30

#define LSM6DSV_CTRL8_ADDR		0x17
#define LSM6DSV_CTRL9_ADDR		0x18
#define LSM6DSV_QVAR_C_ZIN_0_MASK	0x10

#define LSM6DSV_CTRL10_ADDR		0x19
#define LSM6DSV_TIMESTAMP_EN		0x20

#define LSM6DSV_STATUS_REG		0x1e

#define LSM6DSV_GYRO_OUT_X_L_ADDR	0x22
#define LSM6DSV_ACCEL_OUT_X_L_ADDR	0x28
#define LSM6DSV_QVAR_OUT_ADDR		0x3a

/* Output data rate registers and masks */
#define LSM6DSV_ODR_REG(_sensor) \
	(LSM6DSV_CTRL1_ADDR + (_sensor))
#define LSM6DSV_ODR_MASK		0x0f

/* Hardware FIFO size in byte */
#define LSM6DSV_MAX_FIFO_SIZE		4096
#define LSM6DSV_MAX_FIFO_LENGTH		(LSM6DSV_MAX_FIFO_SIZE / OUT_XYZ_SIZE)

/* FIFO decimator registers and bitmask */
#define LSM6DSV_FIFO_CTRL1_ADDR		0x07
#define LSM6DSV_FIFO_CTRL2_ADDR		0x08

#define LSM6DSV_FIFO_CTRL3_ADDR		0x09
#define LSM6DSV_FIFO_ODR_XL_MASK	0x0f
#define LSM6DSV_FIFO_ODR_G_MASK		0xf0

#define LSM6DSV_FIFO_CTRL4_ADDR		0x0a
#define LSM6DSV_FIFO_MODE_MASK		0x07

#define LSM6DSV_INT1_CTRL		0x0d
#define LSM6DSV_INT2_CTRL		0x0e
#define LSM6DSV_INT_FIFO_TH_MASK	0x08
#define LSM6DSV_INT_FIFO_OVR_MASK	0x10
#define LSM6DSV_INT_FIFO_FULL_MASK	0x20

#define LSM6DSV_CTRL6_ADDR		0x15
#define LSM6DSV_CTRL8_ADDR		0x17

#define LSM6DSV_FIFO_STS1_ADDR		0x1b
#define LSM6DSV_FIFO_STS2_ADDR		0x1c
#define LSM6DSV_FIFO_DIFF_MASK		0x01ff
#define LSM6DSV_FIFO_FULL_MASK		0x2000
#define LSM6DSV_FIFO_DATA_OVR_MASK	0x4000
#define LSM6DSV_FIFO_WATERMARK_MASK	0x8000

/* Out FIFO data register */
#define LSM6DSV_FIFO_DATA_ADDR_TAG	0x78

/* Registers value for supported FIFO mode */
#define LSM6DSV_FIFO_MODE_BYPASS_VAL	0x00
#define LSM6DSV_FIFO_MODE_CONT_VAL	0x06

/* Define device available in FIFO pattern */
enum lsm6dsv_dev_fifo {
	LSM6DSV_FIFO_DEV_INVALID = -1,
	LSM6DSV_FIFO_DEV_GYRO = 0,
	LSM6DSV_FIFO_DEV_ACCEL,
	LSM6DSV_FIFO_DEV_QVAR,
	LSM6DSV_FIFO_DEV_NUM,
};

/* Define FIFO data pattern, tag and len */
#define LSM6DSV_SAMPLE_SIZE		6
#define LSM6DSV_TS_SAMPLE_SIZE		4
#define LSM6DSV_TAG_SIZE		1
#define LSM6DSV_FIFO_SAMPLE_SIZE	(LSM6DSV_SAMPLE_SIZE + LSM6DSV_TAG_SIZE)
#define LSM6DSV_MAX_FIFO_DEPTH		416

#define LSM6DSV_QVAR_FILTER_X		0x023E
#define LSM6DSV_QVAR_FEATURE		0x0244

enum lsm6dsv_tag_fifo {
	LSM6DSV_GYRO_TAG = 0x01,
	LSM6DSV_ACC_TAG = 0x02,
	LSM6DSV_QVAR_FILTER_X_TAG = 0x1b,
	LSM6DSV_QVAR_FEATURE_TAG = 0x1c,
};

struct lsm6dsv_fstatus {
	uint16_t len;
};

/* Absolute maximum rate for Acc and Gyro sensors */
#define LSM6DSV_ODR_MIN_VAL		7500
#define LSM6DSV_ODR_MAX_VAL \
	MOTION_MAX_SENSOR_FREQUENCY(240000, 7500)

/* ODR reg value from selected data rate in mHz */
#define LSM6DSV_ODR_TO_REG(_odr) (__fls(_odr / LSM6DSV_ODR_MIN_VAL) + 2)

#define LSM6DSV_FIFO_ODR_TO_REG(_s) \
	(_s->type == MOTIONSENSE_TYPE_ACCEL ? LSM6DSV_FIFO_ODR_XL_MASK : \
	 LSM6DSV_FIFO_ODR_G_MASK)

/* Normalized ODR values from selected data rate in mHz */
#define LSM6DSV_REG_TO_ODR(_reg) (LSM6DSV_ODR_MIN_VAL << (_reg - 2))

/* Full Scale ranges value and gain for Acc */
#define LSM6DSV_FS_LIST_NUM		4

#define LSM6DSV_ACCEL_FS_ADDR		LSM6DSV_CTRL8_ADDR
#define LSM6DSV_ACCEL_FS_MASK		0x0c

#define LSM6DSV_ACCEL_FS_2G_VAL		0x00
#define LSM6DSV_ACCEL_FS_4G_VAL		0x01
#define LSM6DSV_ACCEL_FS_8G_VAL		0x02
#define LSM6DSV_ACCEL_FS_16G_VAL	0x03

#define LSM6DSV_ACCEL_FS_MAX_VAL	16

/* Accel reg value from Full Scale range */
static inline uint8_t lsm6dsv_accel_fs_reg(int fs)
{
	uint8_t ret;

	switch (fs) {
	case 2:
		ret = LSM6DSV_ACCEL_FS_2G_VAL;
		break;
	case 16:
		ret = LSM6DSV_ACCEL_FS_16G_VAL;
		break;
	default:
		ret = __fls(fs);
		break;
	}

	return ret;
}

#define LSM6DSV_XL_FS_REG(_fs) (__fls(_fs) - 1)

/* Accel normalized FS value from Full Scale */
#define LSM6DSV_ACCEL_NORMALIZE_FS(_fs) (1 << __fls(_fs))

/* Full Scale range value and gain for Gyro */
#define LSM6DSV_GYRO_FS_ADDR		LSM6DSV_CTRL6_ADDR
#define LSM6DSV_GYRO_FS_MASK		0x0f

/* Minimal Gyro range in mDPS */
#define LSM6DSV_GYRO_FS_MIN_VAL_MDPS		((8750 << 15) / 1000)
#define LSM6DSV_GYRO_FS_MAX_REG_VAL		3

/* Gyro reg value for Full Scale selection in DPS */
#define LSM6DSV_GYRO_FS_REG(_fs) \
	__fls(MAX(1, (_fs * 1000) / LSM6DSV_GYRO_FS_MIN_VAL_MDPS))

/* Gyro normalized FS value (in DPS) from Full Scale register */
#define LSM6DSV_GYRO_NORMALIZE_FS(_reg) \
	((LSM6DSV_GYRO_FS_MIN_VAL_MDPS << (_reg)) / 1000)

/* FS register address/mask for Acc/Gyro sensors */
#define LSM6DSV_RANGE_REG(_sensor)		(LSM6DSV_ACCEL_FS_ADDR + (_sensor))
#define LSM6DSV_RANGE_MASK			0x0c

/* Status register bit for Acc/Gyro/Qvar data ready */
enum lsm6dsv_status {
	LSM6DSV_STS_DOWN = 0x00,
	LSM6DSV_STS_XLDA_UP = 0x01,
	LSM6DSV_STS_GDA_UP = 0x02,
	LSM6DSV_STS_QVARDA_UP = 0x08,
};

/* Status register bitmask for Acc/Gyro/Qvar data ready */
#define LSM6DSV_STS_XLDA_MASK		0x01
#define LSM6DSV_STS_GDA_MASK		0x02
#define LSM6DSV_STS_QVARDA_MASK		0x08

/* Sensor resolution in number of bits: fixed 16 bit */
#define LSM6DSV_RESOLUTION		16

/* Aggregate private data for all supported sensor (Acc, Gyro) */
struct lsm6dsv_data {
	struct stprivate_data st_data[LSM6DSV_FIFO_DEV_NUM];
};

/*
 * Note: The specific number of samples to discard depends on the filters
 * configured for the chip, as well as the ODR being set. For most of our
 * allowed ODRs, 3 should suffice.
 * See: ST's LSM6DSV application notes (AN5192) Tables 12 and 18 for details
 */
#define LSM6DSV_DISCARD_SAMPLES 3

#define LSM6DSV_GET_DATA(_s) ((struct stprivate_data *)((_s)->drv_data))

/* Macro to initialize motion_sensors structure */
#define LSM6DSV_ST_DATA(g, type) (&(&(g))->st_data[(type)])
#define LSM6DSV_MAIN_SENSOR(_s) ((_s) - (_s)->type)

extern const struct accelgyro_drv lsm6dsv_drv;

void lsm6dsv_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_ACCELGYRO_LSM6DSV_H */
