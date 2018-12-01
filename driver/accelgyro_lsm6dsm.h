/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DSM (also LSM6DSL) Accel and Gyro driver for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_LSM6DSM_H
#define __CROS_EC_ACCELGYRO_LSM6DSM_H

#include "stm_mems_common.h"

#define LSM6DSM_I2C_ADDR(__x)		(__x << 1)

/*
 * 7-bit address is 110101xb. Where 'x' is determined
 * by the voltage on the ADDR pin
 */
#define LSM6DSM_ADDR0			LSM6DSM_I2C_ADDR(0x6a)
#define LSM6DSM_ADDR1			LSM6DSM_I2C_ADDR(0x6b)

/* COMMON DEFINE FOR ACCEL-GYRO SENSORS */
#define LSM6DSM_EN_BIT			0x01
#define LSM6DSM_DIS_BIT			0x00

/* Access to embedded sensor hub register bank */
#define LSM6DSM_FUNC_CFG_ACC_ADDR	0x01
#define LSM6DSM_FUNC_CFG_EN			0x80
#define LSM6DSM_FUNC_CFG_EN_B			0x20

/* FIFO decimator registers and bitmask */
#define LSM6DSM_FIFO_CTRL1_ADDR		0x06

/* Output data rate registers and masks */
#define LSM6DSM_ODR_REG(_sensor) \
	(LSM6DSM_CTRL1_ADDR + _sensor)
#define LSM6DSM_ODR_MASK		0xf0

#define LSM6DSM_FIFO_CTRL2_ADDR		0x07

#define LSM6DSM_FIFO_CTRL3_ADDR		0x08
#define LSM6DSM_FIFO_DEC_XL_OFF			0
#define LSM6DSM_FIFO_DEC_G_OFF			3

#define LSM6DSM_FIFO_CTRL4_ADDR		0x09

#define LSM6DSM_FIFO_DECIMATOR(_dec) \
	(_dec < 8 ? _dec : (2 + __builtin_ctz(_dec)))

/* Hardware FIFO size in byte */
#define LSM6DSM_MAX_FIFO_SIZE		4096
#define LSM6DSM_MAX_FIFO_LENGTH	(LSM6DSM_MAX_FIFO_SIZE / OUT_XYZ_SIZE)

#define LSM6DSM_FIFO_CTRL5_ADDR		0x0a
#define LSM6DSM_FIFO_CTRL5_ODR_OFF		3
#define LSM6DSM_FIFO_CTRL5_ODR_MASK \
	(0xf << LSM6DSM_FIFO_CTRL5_ODR_OFF)
#define LSM6DSM_FIFO_CTRL5_MODE_MASK		0x07

#define LSM6DSM_INT1_CTRL		0x0d
#define LSM6DSM_INT_FIFO_TH			0x08
#define LSM6DSM_INT_FIFO_OVR			0x10
#define LSM6DSM_INT_FIFO_FULL			0x20
#define LSM6DSM_INT_SIGMO			0x40

/* Who Am I */
#define LSM6DSM_WHO_AM_I_REG		0x0f
#define LSM6DSM_WHO_AM_I			0x6a

#define LSM6DSM_CTRL1_ADDR		0x10
#define LSM6DSM_XL_ODR_MASK			0xf0

#define LSM6DSM_CTRL2_ADDR		0x11
#define LSM6DSM_CTRL3_ADDR		0x12
#define LSM6DSM_SW_RESET			0x01
#define LSM6DSM_IF_INC				0x04
#define LSM6DSM_PP_OD				0x10
#define LSM6DSM_H_L_ACTIVE			0x20
#define LSM6DSM_BDU				0x40

#define LSM6DSM_CTRL4_ADDR		0x13
#define LSM6DSM_INT2_ON_INT1_MASK		0x20

#define LSM6DSM_CTRL6_ADDR		0x15
#define LSM6DSM_CTRL7_ADDR		0x16

#define LSM6DSM_CTRL10_ADDR		0x19
#define LSM6DSM_FUNC_EN_MASK			0x04
#define LSM6DSM_SIG_MOT_MASK			0x01
#define LSM6DSM_EMBED_FUNC_EN			0x04
#define LSM6DSM_SIG_MOT_EN			0x01

/* Master mode configuration register */
#define LSM6DSM_MASTER_CFG_ADDR		0x1a
#define LSM6DSM_PASSTROUGH_MASK			0x1f
#define LSM6DSM_EXT_TRIGGER_EN			0x10
#define LSM6DSM_PULLUP_EN			0x08
#define LSM6DSM_I2C_PASS_THRU_MODE		0x04
#define LSM6DSM_I2C_MASTER_ON			0x01

#define LSM6DSM_TAP_SRC_ADDR		0x1c
#define LSM6DSM_STAP_DETECT			0x20
#define LSM6DSM_DTAP_DETECT			0x10

#define LSM6DSM_STATUS_REG		0x1e

#define LSM6DSM_GYRO_OUT_X_L_ADDR	0x22
#define LSM6DSM_ACCEL_OUT_X_L_ADDR	0x28

#define LSM6DSM_SENSORHUB1_REG		0x2e

#define LSM6DSM_FIFO_STS1_ADDR		0x3a
#define LSM6DSM_FIFO_STS2_ADDR		0x3b
#define LSM6DSM_FIFO_DIFF_MASK			0x07ff
#define LSM6DSM_FIFO_EMPTY			0x1000
#define LSM6DSM_FIFO_FULL			0x2000
#define LSM6DSM_FIFO_DATA_OVR			0x4000
#define LSM6DSM_FIFO_WATERMARK			0x8000
#define LSM6DSM_FIFO_NODECIM			0x01

/* Out data register */
#define LSM6DSM_FIFO_DATA_ADDR		0x3e

/* Registers value for supported FIFO mode */
#define LSM6DSM_FIFO_MODE_BYPASS_VAL		0x00
#define LSM6DSM_FIFO_MODE_CONTINUOUS_VAL	0x06

#define LSM6DSM_FUNC_SRC1_ADDR		0x53
#define LSM6DSM_SENSORHUB_END_OP		0x01
#define LSM6DSM_SIGN_MOTION_IA			0x40

#define LSM6DSM_LIR_ADDR		0x58
#define LSM6DSM_LIR_MASK			0x01
#define LSM6DSM_EN_INT				0x80
#define LSM6DSM_EN_TAP				0x0e
#define LSM6DSM_TAP_MASK			0x8e

#define LSM6DSM_TAP_THS_6D		0x59
#define LSM6DSM_D4D_EN_MASK			0x80
#define LSM6DSM_TAP_TH_MASK			0x1f

#define LSM6DSM_INT_DUR2_ADDR		0x5a
#define LSM6DSM_TAP_DUR_MASK			0xf0
#define LSM6DSM_TAP_QUIET_MASK			0x0c

#define LSM6DSM_WUP_THS_ADDR		0x5b
#define LSM6DSM_S_D_TAP_MASK			0x80
#define LSM6DSM_STAP_EN				0
#define LSM6DSM_DTAP_EN				1

#define LSM6DSM_MD1_CFG_ADDR		0x5e
#define LSM6DSM_INT1_STAP			0x40
#define LSM6DSM_INT1_DTAP			0x08

/* Register values for Sensor Hub Slave 0 / Bank A */
#define LSM6DSM_SLV0_ADD_ADDR		0x02
#define LSM6DSM_SLV0_ADDR_SHFT			1
#define LSM6DSM_SLV0_ADDR_MASK			0xfe
#define LSM6DSM_SLV0_RD_BIT			0x01

#define LSM6DSM_SLV0_SUBADD_ADDR	0x03

#define LSM6DSM_SLV0_CONFIG_ADDR	0x04
#define LSM6DSM_SLV0_SLV_RATE_SHFT		6
#define LSM6DSM_SLV0_SLV_RATE_MASK		0xc0
#define LSM6DSM_SLV0_AUX_SENS_SHFT		4
#define LSM6DSM_SLV0_AUX_SENS_MASK		0x30
#define LSM6DSM_SLV0_NUM_OPS_MASK		0x07

#define LSM6DSM_SLV1_CONFIG_ADDR	0x07
#define LSM6DSM_SLV0_WR_ONCE_MASK		0x20

#define LSM6DSM_DATA_WRITE_SUB_SLV0_ADDR 0x0e

/* Define device available in FIFO pattern */
enum dev_fifo {
	FIFO_DEV_INVALID = -1,
	FIFO_DEV_GYRO = 0,
	FIFO_DEV_ACCEL,
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
	FIFO_DEV_MAG,
#endif
	FIFO_DEV_NUM,
};

struct fstatus {
	uint16_t len;
	uint16_t pattern;
};

/* Absolute maximum rate for acc and gyro sensors */
#define LSM6DSM_ODR_MIN_VAL		13000
#define LSM6DSM_ODR_MAX_VAL \
	MOTION_MAX_SENSOR_FREQUENCY(416000, LSM6DSM_ODR_MIN_VAL)

/* ODR reg value from selected data rate in mHz */
#define LSM6DSM_ODR_TO_REG(_odr) (__fls(_odr / LSM6DSM_ODR_MIN_VAL) + 1)

/* normalized ODR value from selected data rate in mHz */
#define LSM6DSM_REG_TO_ODR(_reg) (LSM6DSM_ODR_MIN_VAL << (_reg - 1))

/* Full Scale range value and gain for Acc */
#define LSM6DSM_FS_LIST_NUM		4

#define LSM6DSM_ACCEL_FS_ADDR		0x10
#define LSM6DSM_ACCEL_FS_MASK		0x0c

#define LSM6DSM_ACCEL_FS_2G_VAL		0x00
#define LSM6DSM_ACCEL_FS_4G_VAL		0x02
#define LSM6DSM_ACCEL_FS_8G_VAL		0x03
#define LSM6DSM_ACCEL_FS_16G_VAL	0x01

#define LSM6DSM_ACCEL_FS_MAX_VAL	16

/* Accel Reg value from Full Scale */
#define LSM6DSM_ACCEL_FS_REG(_fs) \
	(_fs == 2 ? LSM6DSM_ACCEL_FS_2G_VAL : \
	_fs == 16 ? LSM6DSM_ACCEL_FS_16G_VAL : \
	__fls(_fs))

/* Accel normalized FS value from Full Scale */
#define LSM6DSM_ACCEL_NORMALIZE_FS(_fs) (1 << __fls(_fs))

/* Full Scale range value and gain for Gyro */
#define LSM6DSM_GYRO_FS_ADDR		0x11
#define LSM6DSM_GYRO_FS_MASK		0x0c

#define LSM6DSM_GYRO_FS_245_VAL		0x00
#define LSM6DSM_GYRO_FS_500_VAL		0x01
#define LSM6DSM_GYRO_FS_1000_VAL	0x02
#define LSM6DSM_GYRO_FS_2000_VAL	0x03

#define LSM6DSM_GYRO_FS_245_GAIN	8750
#define LSM6DSM_GYRO_FS_500_GAIN	17500
#define LSM6DSM_GYRO_FS_1000_GAIN	35000
#define LSM6DSM_GYRO_FS_2000_GAIN	70000

#define LSM6DSM_GYRO_FS_MAX_VAL		20000

/* Gyro FS Gain value from selected Full Scale */
#define LSM6DSM_GYRO_FS_GAIN(_fs) \
	(LSM6DSM_GYRO_FS_245_GAIN << __fls(_fs / 245))

/* Gyro FS Full Scale value from Gain */
#define LSM6DSM_GYRO_GAIN_FS(_gain) \
	(_gain == LSM6DSM_GYRO_FS_245_GAIN ? 245 : \
	500 << (30 - __builtin_clz(_gain / LSM6DSM_GYRO_FS_245_GAIN)))

/* Gyro Reg value from Full Scale */
#define LSM6DSM_GYRO_FS_REG(_fs) \
	__fls(_fs / 245)

/* Gyro normalized FS value from Full Scale: for Gyro Gains are not multiple */
#define LSM6DSM_GYRO_NORMALIZE_FS(_fs) \
	(_fs == 245 ? 245 : 500 << __fls(_fs / 500))

/* FS register address/mask for Acc/Gyro sensors */
#define LSM6DSM_RANGE_REG(_sensor)  (LSM6DSM_ACCEL_FS_ADDR + (_sensor))
#define LSM6DSM_RANGE_MASK		0x0c

/* Status register bitmask for Acc/Gyro data ready */
enum lsm6dsm_status {
	LSM6DSM_STS_DOWN = 0x00,
	LSM6DSM_STS_XLDA_UP = 0x01,
	LSM6DSM_STS_GDA_UP = 0x02
};

#define LSM6DSM_STS_XLDA_MASK		0x01
#define LSM6DSM_STS_GDA_MASK		0x02

/* Sensor resolution in number of bits: fixed 16 bit */
#define LSM6DSM_RESOLUTION		16

extern const struct accelgyro_drv lsm6dsm_drv;

void lsm6dsm_interrupt(enum gpio_signal signal);

struct lsm6dsm_fifo_data {
	/*
	 * FIFO data order is based on the ODR of each sensors.
	 * For example Acc @ 52 Hz, Gyro @ 26 Hz Mag @ 13 Hz in FIFO we have
	 * for each pattern this data samples:
	 *  ________ _______ _______ _______ ________ _______ _______
	 * | Gyro_0 | Acc_0 | Mag_0 | Acc_1 | Gyro_1 | Acc_2 | Acc_3 |
	 * |________|_______|_______|_______|________|_______|_______|
	 *
	 * Total samples for each pattern: 2 Gyro, 4 Acc, 1 Mag
	 */
	/* Calculated samples in a pattern, based on ODR. */
	int samples_in_pattern[FIFO_DEV_NUM];

	/* Sum of all samples_in_pattern. */
	int total_samples_in_pattern;
};

struct lsm6dsm_data {
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
	struct stprivate_data st_data[3];
#else
	struct stprivate_data st_data[2];
#endif
#ifdef CONFIG_ACCEL_FIFO
	struct lsm6dsm_fifo_data config;
	struct lsm6dsm_fifo_data current;
	int next_in_patten;
#endif
};

#define LSM6DSM_GET_DATA(_s) ((struct lsm6dsm_data *)((_s)->drv_data))

#define LSM6DSM_ST_DATA(g, type) (&(&(g))->st_data[(type)])

#define LSM6DSM_MAIN_SENSOR(_s) ((_s) - (_s)->type)

#endif /* __CROS_EC_ACCELGYRO_LSM6DSM_H */
