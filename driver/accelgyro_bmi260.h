/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BMI260 accelerometer and gyro and BMM150 compass module for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_BMI260_H
#define __CROS_EC_ACCELGYRO_BMI260_H

#include "accelgyro.h"
#include "mag_bmm150.h"

/*
 * The addr field of motion_sensor support both SPI and I2C:
 * This is defined in include/i2c.h and is no longer an 8bit
 * address. The 7/10 bit address starts at bit 0 and leaves
 * room for a 10 bit address, although we don't currently
 * have any 10 bit slaves.  I2C or SPI is indicated by a
 * more significant bit
 */

/* I2C addresses */
#define BMI260_ADDR0_FLAGS	0x68
#define BMI260_ADDR1_FLAGS	0x69

#define BMI260_CHIP_ID           0x00
#define BMI260_CHIP_ID_MAJOR     0x27

#define BMI260_SPEC_ACC_STARTUP_TIME_MS     10
#define BMI260_SPEC_GYR_STARTUP_TIME_MS     80
#define BMI260_SPEC_MAG_STARTUP_TIME_MS     60


#define BMI260_ERR_REG         0x02
#define BMI260_PMU_STATUS      0x03
#define BMI260_PMU_MAG_OFFSET               0
#define BMI260_PMU_GYR_OFFSET               2
#define BMI260_PMU_ACC_OFFSET               4
#define BMI260_PMU_SENSOR_STATUS(_sensor_type, _val) \
	(((_val) >> (4 - 2 * (_sensor_type))) & 0x3)
#define BMI260_PMU_SUSPEND                  0
#define BMI260_PMU_NORMAL                   1
#define BMI260_PMU_LOW_POWER                2
#define BMI260_PMU_FAST_STARTUP             3

#define BMI260_MAG_X_L_G       0x04
#define BMI260_MAG_X_H_G       0x05
#define BMI260_MAG_Y_L_G       0x06
#define BMI260_MAG_Y_H_G       0x07
#define BMI260_MAG_Z_L_G       0x08
#define BMI260_MAG_Z_H_G       0x09
#define BMI260_RHALL_L_G       0x0a
#define BMI260_RHALL_H_G       0x0b
#define BMI260_GYR_X_L_G       0x0c
#define BMI260_GYR_X_H_G       0x0d
#define BMI260_GYR_Y_L_G       0x0e
#define BMI260_GYR_Y_H_G       0x0f
#define BMI260_GYR_Z_L_G       0x10
#define BMI260_GYR_Z_H_G       0x11
#define BMI260_ACC_X_L_G       0x12
#define BMI260_ACC_X_H_G       0x13
#define BMI260_ACC_Y_L_G       0x14
#define BMI260_ACC_Y_H_G       0x15
#define BMI260_ACC_Z_L_G       0x16
#define BMI260_ACC_Z_H_G       0x17

#define BMI260_SENSORTIME_0    0x18
#define BMI260_SENSORTIME_1    0x19
#define BMI260_SENSORTIME_2    0x1a

#define BMI260_STATUS          0x1b
#define BMI260_POR_DETECTED        BIT(0)
#define BMI260_GYR_SLF_TST         BIT(1)
#define BMI260_MAG_MAN_OP          BIT(2)
#define BMI260_FOC_RDY             BIT(3)
#define BMI260_NVM_RDY             BIT(4)
#define BMI260_DRDY_MAG            BIT(5)
#define BMI260_DRDY_GYR            BIT(6)
#define BMI260_DRDY_ACC            BIT(7)
#define BMI260_DRDY_OFF(_sensor)   (7 - (_sensor))
#define BMI260_DRDY_MASK(_sensor)  (1 << BMI260_DRDY_OFF(_sensor))

/* first 2 bytes are the interrupt reasons, next 2 some qualifier */
#define BMI260_INT_STATUS_0    0x1c
#define BMI260_STEP_INT            BIT(0)
#define BMI260_SIGMOT_INT          BIT(1)
#define BMI260_ANYM_INT            BIT(2)
#define BMI260_PMU_TRIGGER_INT     BIT(3)
#define BMI260_D_TAP_INT           BIT(4)
#define BMI260_S_TAP_INT           BIT(5)
#define BMI260_ORIENT_INT          BIT(6)
#define BMI260_FLAT_INT            BIT(7)
#define BMI260_ORIENT_XY_MASK	   0x30
#define BMI260_ORIENT_PORTRAIT		(0 << 4)
#define BMI260_ORIENT_PORTRAIT_INVERT	BIT(4)
#define BMI260_ORIENT_LANDSCAPE		(2 << 4)
#define BMI260_ORIENT_LANDSCAPE_INVERT	(3 << 4)


#define BMI260_INT_STATUS_1    0x1d
#define BMI260_HIGHG_INT           (1 << (2 + 8))
#define BMI260_LOWG_INT            (1 << (3 + 8))
#define BMI260_DRDY_INT            (1 << (4 + 8))
#define BMI260_FFULL_INT           (1 << (5 + 8))
#define BMI260_FWM_INT             (1 << (6 + 8))
#define BMI260_NOMO_INT            (1 << (7 + 8))

#define BMI260_INT_MASK            0xFFFF

#define BMI260_INT_STATUS_2    0x1e
#define BMI260_INT_STATUS_3    0x1f
#define BMI260_FIRST_X             (1 << (0 + 16))
#define BMI260_FIRST_Y             (1 << (1 + 16))
#define BMI260_FIRST_Z             (1 << (2 + 16))
#define BMI260_SIGN                (1 << (3 + 16))
#define BMI260_ANYM_OFFSET         0
#define BMI260_TAP_OFFSET          4
#define BMI260_HIGH_OFFSET         8
#define BMI260_INT_INFO(_type, _data) \
(CONCAT2(BMI260_, _data) << CONCAT3(BMI260_, _type, _OFFSET))

#define BMI260_ORIENT_Z            (1 << (6 + 24))
#define BMI260_FLAT                (1 << (7 + 24))

#define BMI260_TEMPERATURE_0   0x20
#define BMI260_TEMPERATURE_1   0x21
#define BMI260_INVALID_TEMP        0x8000


#define BMI260_FIFO_LENGTH_0   0x22
#define BMI260_FIFO_LENGTH_1   0x23
#define BMI260_FIFO_LENGTH_MASK    (BIT(11) - 1)
#define BMI260_FIFO_DATA       0x24
enum fifo_header {
	BMI260_EMPTY = 0x80,
	BMI260_SKIP = 0x40,
	BMI260_TIME = 0x44,
	BMI260_CONFIG = 0x48
};

#define BMI260_FH_MODE_MASK    0xc0
#define BMI260_FH_PARM_OFFSET    2
#define BMI260_FH_PARM_MASK    (0x7 << BMI260_FH_PARM_OFFSET)
#define BMI260_FH_EXT_MASK     0x03


#define BMI260_ACC_CONF        0x40
#define BMI260_ODR_MASK                 0x0F
#define BMI260_ACC_BW_OFFSET            4
#define BMI260_ACC_BW_MASK     (0x7 << BMI260_ACC_BW_OFFSET)

/* REMOVE: ok for 260 */
#define BMI260_ACC_RANGE       0x41
#define BMI260_GSEL_2G         0x00
#define BMI260_GSEL_4G         0x01
#define BMI260_GSEL_8G         0x02
#define BMI260_GSEL_16G        0x03

#define BMI260_GYR_CONF        0x42
#define BMI260_GYR_BW_OFFSET   4
#define BMI260_GYR_BW_MASK     (0x3 << BMI260_GYR_BW_OFFSET)

#define BMI260_GYR_RANGE       0x43
#define BMI260_DPS_SEL_2000    0x00
#define BMI260_DPS_SEL_1000    0x01
#define BMI260_DPS_SEL_500     0x02
#define BMI260_DPS_SEL_250     0x03
#define BMI260_DPS_SEL_125     0x04


#define BMI260_MAG_CONF        0x44

/* odr = 100 / (1 << (8 - reg)) ,within limit */
#define BMI260_ODR_0_78HZ      0x01
#define BMI260_ODR_100HZ       0x08

/* REMOVE: ok for 260 */
#define BMI260_REG_TO_ODR(_regval) \
	((_regval) < BMI260_ODR_100HZ ? 100000 / (1 << (8 - (_regval))) : \
					100000 * (1 << ((_regval) - 8)))
#define BMI260_ODR_TO_REG(_odr) \
	((_odr) < 100000 ? (__builtin_clz(100000 / (_odr)) - 24) : \
			   (39 - __builtin_clz((_odr) / 100000)))

/* REMOVE: ok for 260 */
#define BMI260_CONF_REG(_sensor)   (0x40 + 2 * (_sensor))
#define BMI260_RANGE_REG(_sensor)  (0x41 + 2 * (_sensor))

#define BMI260_FIFO_DOWNS      0x45
#define BMI260_FIFO_CONFIG_0   0x46
#define BMI260_FIFO_CONFIG_1   0x47
#define BMI260_FIFO_TAG_TIME_EN    BIT(1)
#define BMI260_FIFO_TAG_INT2_EN    BIT(2)
#define BMI260_FIFO_TAG_INT1_EN    BIT(3)
#define BMI260_FIFO_HEADER_EN      BIT(4)
#define BMI260_FIFO_MAG_EN         BIT(5)
#define BMI260_FIFO_ACC_EN         BIT(6)
#define BMI260_FIFO_GYR_EN         BIT(7)
#define BMI260_FIFO_TARG_INT(_i)  CONCAT3(BMI260_FIFO_TAG_INT, _i, _EN)
#define BMI260_FIFO_SENSOR_EN(_sensor) \
	((_sensor) == MOTIONSENSE_TYPE_ACCEL ? BMI260_FIFO_ACC_EN : \
	  ((_sensor) == MOTIONSENSE_TYPE_GYRO ? BMI260_FIFO_GYR_EN : \
	   BMI260_FIFO_MAG_EN))

#define BMI260_MAG_IF_0        0x4b
#define BMI260_MAG_I2C_ADDRESS BMI260_MAG_IF_0
#define BMI260_MAG_IF_1        0x4c
#define BMI260_MAG_I2C_CONTROL BMI260_MAG_IF_1
#define BMI260_MAG_READ_BURST_MASK 3
#define BMI260_MAG_READ_BURST_1    0
#define BMI260_MAG_READ_BURST_2    1
#define BMI260_MAG_READ_BURST_6    2
#define BMI260_MAG_READ_BURST_8    3
#define BMI260_MAG_OFFSET_OFF      3
#define BMI260_MAG_OFFSET_MASK     (0xf << BMI260_MAG_OFFSET_OFF)
#define BMI260_MAG_MANUAL_EN       BIT(7)

#define BMI260_MAG_IF_2        0x4d
#define BMI260_MAG_I2C_READ_ADDR    BMI260_MAG_IF_2
#define BMI260_MAG_IF_3        0x4e
#define BMI260_MAG_I2C_WRITE_ADDR   BMI260_MAG_IF_3
#define BMI260_MAG_IF_4        0x4f
#define BMI260_MAG_I2C_WRITE_DATA   BMI260_MAG_IF_4
#define BMI260_MAG_I2C_READ_DATA    BMI260_MAG_X_L_G

#define BMI260_INT_EN_0        0x50
#define BMI260_INT_ANYMO_X_EN      BIT(0)
#define BMI260_INT_ANYMO_Y_EN      BIT(1)
#define BMI260_INT_ANYMO_Z_EN      BIT(2)
#define BMI260_INT_D_TAP_EN        BIT(4)
#define BMI260_INT_S_TAP_EN        BIT(5)
#define BMI260_INT_ORIENT_EN       BIT(6)
#define BMI260_INT_FLAT_EN         BIT(7)
#define BMI260_INT_EN_1        0x51
#define BMI260_INT_HIGHG_X_EN      BIT(0)
#define BMI260_INT_HIGHG_Y_EN      BIT(1)
#define BMI260_INT_HIGHG_Z_EN      BIT(2)
#define BMI260_INT_LOW_EN          BIT(3)
#define BMI260_INT_DRDY_EN         BIT(4)
#define BMI260_INT_FFUL_EN         BIT(5)
#define BMI260_INT_FWM_EN          BIT(6)
#define BMI260_INT_EN_2        0x52
#define BMI260_INT_NOMOX_EN        BIT(0)
#define BMI260_INT_NOMOY_EN        BIT(1)
#define BMI260_INT_NOMOZ_EN        BIT(2)
#define BMI260_INT_STEP_DET_EN     BIT(3)

#define BMI260_INT_OUT_CTRL    0x53
#define BMI260_INT_EDGE_CTRL       BIT(0)
#define BMI260_INT_LVL_CTRL        BIT(1)
#define BMI260_INT_OD              BIT(2)
#define BMI260_INT_OUTPUT_EN       BIT(3)
#define BMI260_INT1_CTRL_OFFSET     0
#define BMI260_INT2_CTRL_OFFSET     4
#define BMI260_INT_CTRL(_i, _bit) \
(CONCAT2(BMI260_INT_, _bit) << CONCAT3(BMI260_INT, _i, _CTRL_OFFSET))

#define BMI260_INT_LATCH       0x54
#define BMI260_INT1_INPUT_EN       BIT(4)
#define BMI260_INT2_INPUT_EN       BIT(5)
#define BMI260_LATCH_MASK          0xf
#define BMI260_LATCH_NONE          0
#define BMI260_LATCH_5MS           5
#define BMI260_LATCH_FOREVER       0xf

#define BMI260_INT_MAP_0       0x55
#define BMI260_INT_LOWG_STEP       BIT(0)
#define BMI260_INT_HIGHG           BIT(1)
#define BMI260_INT_ANYMOTION       BIT(2)
#define BMI260_INT_NOMOTION        BIT(3)
#define BMI260_INT_D_TAP           BIT(4)
#define BMI260_INT_S_TAP           BIT(5)
#define BMI260_INT_ORIENT          BIT(6)
#define BMI260_INT_FLAT            BIT(7)

#define BMI260_INT_MAP_1       0x56
#define BMI260_INT_PMU_TRIG        BIT(0)
#define BMI260_INT_FFULL           BIT(1)
#define BMI260_INT_FWM             BIT(2)
#define BMI260_INT_DRDY            BIT(3)
#define BMI260_INT1_MAP_OFFSET      4
#define BMI260_INT2_MAP_OFFSET      0
#define BMI260_INT_MAP(_i, _bit) \
(CONCAT2(BMI260_INT_, _bit) << CONCAT3(BMI260_INT, _i, _MAP_OFFSET))
#define BMI260_INT_FIFO_MAP    BMI260_INT_MAP_1

#define BMI260_INT_MAP_2       0x57

#define BMI260_INT_MAP_INT_1   BMI260_INT_MAP_0
#define BMI260_INT_MAP_INT_2   BMI260_INT_MAP_2
#define BMI260_INT_MAP_REG(_i)  CONCAT2(BMI260_INT_MAP_INT_, _i)

#define BMI260_INT_DATA_0      0x58
#define BMI260_INT_DATA_1      0x59

#define BMI260_INT_MOTION_0    0x5f
#define BMI260_INT_MOTION_1    0x60
/*
 * The formula is defined in 2.11.25 (any motion interrupt [1]).
 *
 * if we want threshold at a (in mg), the register should be x, where
 * x * 7.81mg = a, assuming a range of 4G, which is
 * x * 4 * 1.953 = a so
 * x = a * 1000 / range * 1953
 */
#define BMI260_MOTION_TH(_s, _mg) \
	 (MIN(((_mg) * 1000) / ((_s)->drv->get_range(_s) * 1953), 0xff))
#define BMI260_INT_MOTION_2    0x61
#define BMI260_INT_MOTION_3    0x62
#define BMI260_MOTION_NO_MOT_SEL   BIT(0)
#define BMI260_MOTION_SIG_MOT_SEL  BIT(1)
#define BMI260_MOTION_SKIP_OFF 2
#define BMI260_MOTION_SKIP_MASK 0x3
#define BMI260_MOTION_SKIP_TIME(_ms) \
	(MIN(__fls((_ms) / 1500), BMI260_MOTION_SKIP_MASK))
#define BMI260_MOTION_PROOF_OFF 4
#define BMI260_MOTION_PROOF_MASK 0x3
#define BMI260_MOTION_PROOF_TIME(_ms) \
	(MIN(__fls((_ms) / 250), BMI260_MOTION_PROOF_MASK))

#define BMI260_INT_ORIENT_0				0x65

/* No hysterisis, theta block, int on slope > 0.2 or axis > 1.5, symmetrical */
#define BMI260_INT_ORIENT_0_INIT_VAL			0x48

#define BMI260_INT_ORIENT_1				0x66

/* no axes remap, no int on up/down, no blocking angle */
#define BMI260_INT_ORIENT_1_INIT_VAL			0x00

#define BMI260_INT_FLAT_0      0x67
#define BMI260_INT_FLAT_1      0x68

#define BMI260_FOC_CONF        0x69
#define BMI260_FOC_GYRO_EN              BIT(6)
#define BMI260_FOC_ACC_PLUS_1G          1
#define BMI260_FOC_ACC_MINUS_1G         2
#define BMI260_FOC_ACC_0G               3
#define BMI260_FOC_ACC_Z_OFFSET         0
#define BMI260_FOC_ACC_Y_OFFSET         2
#define BMI260_FOC_ACC_X_OFFSET         4

#define BMI260_CONF            0x6a
#define BMI260_IF_CONF         0x6b
#define BMI260_IF_MODE_OFF     4
#define BMI260_IF_MODE_MASK    3
#define BMI260_IF_MODE_AUTO_OFF 0
#define BMI260_IF_MODE_I2C_IOS  1
#define BMI260_IF_MODE_AUTO_I2C 2

#define BMI260_PMU_TRIGGER     0x6c
#define BMI260_SELF_TEST       0x6d

#define BMI260_OFFSET_ACC70        0x71
#define BMI260_OFFSET_ACC_MULTI_MG      (3900 * 1024)
#define BMI260_OFFSET_ACC_DIV_MG        1000000
#define BMI260_OFFSET_GYR70        0x74
#define BMI260_OFFSET_GYRO_MULTI_MDS    (61 * 1024)
#define BMI260_OFFSET_GYRO_DIV_MDS      1000
#define BMI260_OFFSET_EN_GYR98     0x77
#define BMI260_OFFSET_ACC_EN            BIT(6)
#define BMI260_OFFSET_GYRO_EN           BIT(7)


#define BMI260_CMD_REG             0x7e
#define BMI260_CMD_NOOP            0x00
#define BMI260_CMD_FIFO_FLUSH      0xb0
#define BMI260_CMD_SOFT_RESET      0xb6

/* no in 260 */
#define BMI260_CMD_START_FOC       0x03

#define BMI260_CMD_ACC_MODE_OFFSET 0x10
#define BMI260_CMD_ACC_MODE_SUSP   0x10
#define BMI260_CMD_ACC_MODE_NORMAL 0x11
#define BMI260_CMD_ACC_MODE_LOWPOWER 0x12
#define BMI260_CMD_GYR_MODE_SUSP   0x14
#define BMI260_CMD_GYR_MODE_NORMAL 0x15
#define BMI260_CMD_GYR_MODE_FAST_STARTUP 0x17
#define BMI260_CMD_MAG_MODE_SUSP   0x18
#define BMI260_CMD_MAG_MODE_NORMAL 0x19
#define BMI260_CMD_MAG_MODE_LOWPOWER 0x1a
#define BMI260_CMD_MODE_SUSPEND(_sensor_type) \
	(BMI260_CMD_ACC_MODE_OFFSET | (_sensor_type) << 2 | BMI260_PMU_SUSPEND)
#define BMI260_CMD_MODE_NORMAL(_sensor_type) \
	(BMI260_CMD_ACC_MODE_OFFSET | (_sensor_type) << 2 | BMI260_PMU_NORMAL)

#define BMI260_CMD_EXT_MODE_EN_B0  0x37
#define BMI260_CMD_EXT_MODE_EN_B1  0x9a
#define BMI260_CMD_EXT_MODE_EN_B2  0xc0

#define BMI260_CMD_EXT_MODE_ADDR   0x7f
#define BMI260_CMD_PAGING_EN           BIT(7)
#define BMI260_CMD_TARGET_PAGE         BIT(4)
#define BMI260_COM_C_TRIM_ADDR 0x85
#define BMI260_COM_C_TRIM              (3 << 4)



#define BMI260_CMD_TGT_PAGE    0
#define BMI260_CMD_TGT_PAGE_COM    1
#define BMI260_CMD_TGT_PAGE_ACC    2
#define BMI260_CMD_TGT_PAGE_GYR    3

#define BMI260_FF_FRAME_LEN_TS          4
#define BMI260_FF_DATA_LEN_ACC          6
#define BMI260_FF_DATA_LEN_GYR          6
#define BMI260_FF_DATA_LEN_MAG          8

/* Sensor resolution in number of bits. This sensor has fixed resolution. */
#define BMI260_RESOLUTION      16

/* Min and Max sampling frequency in mHz */
#define BMI260_ACCEL_MIN_FREQ 12500
#define BMI260_ACCEL_MAX_FREQ MOTION_MAX_SENSOR_FREQUENCY(1600000, 100000)
#define BMI260_GYRO_MIN_FREQ  25000
#define BMI260_GYRO_MAX_FREQ MOTION_MAX_SENSOR_FREQUENCY(3200000, 100000)

extern const struct accelgyro_drv bmi260_drv;

enum bmi260_running_mode {
	STANDARD_UI_9DOF_FIFO          = 0,
	STANDARD_UI_IMU_FIFO           = 1,
	STANDARD_UI_IMU                = 2,
	STANDARD_UI_ADVANCEPOWERSAVE   = 3,
	ACCEL_PEDOMETER                = 4,
	APPLICATION_HEAD_TRACKING      = 5,
	APPLICATION_NAVIGATION         = 6,
	APPLICATION_REMOTE_CONTROL     = 7,
	APPLICATION_INDOOR_NAVIGATION  = 8,
};

#define BMI260_FLAG_SEC_I2C_ENABLED    BIT(0)
#define BMI260_FIFO_FLAG_OFFSET        4
#define BMI260_FIFO_ALL_MASK           7

struct bmi260_drv_data_t {
	struct accelgyro_saved_data_t saved_data[3];
	uint8_t              flags;
	uint8_t              enabled_activities;
	uint8_t              disabled_activities;
#ifdef CONFIG_MAG_BMI260_BMM150
	struct bmm150_private_data compass;
#endif
#ifdef CONFIG_BMI260_ORIENTATION_SENSOR
	uint8_t raw_orientation;
	enum motionsensor_orientation orientation;
	enum motionsensor_orientation last_orientation;
#endif

};

#define BMI260_GET_DATA(_s) \
	((struct bmi260_drv_data_t *)(_s)->drv_data)
#define BMI260_GET_SAVED_DATA(_s) \
	(&BMI260_GET_DATA(_s)->saved_data[(_s)->type])

#ifdef CONFIG_BMI260_ORIENTATION_SENSOR
#define ORIENTATION_CHANGED(_sensor) \
	(BMI260_GET_DATA(_sensor)->orientation != \
	BMI260_GET_DATA(_sensor)->last_orientation)

#define GET_ORIENTATION(_sensor) \
	(BMI260_GET_DATA(_sensor)->orientation)

#define SET_ORIENTATION(_sensor, _val) \
	(BMI260_GET_DATA(_sensor)->orientation = _val)

#define SET_ORIENTATION_UPDATED(_sensor) \
	(BMI260_GET_DATA(_sensor)->last_orientation = \
	BMI260_GET_DATA(_sensor)->orientation)
#endif

void bmi260_interrupt(enum gpio_signal signal);

#ifdef CONFIG_BMI260_SEC_I2C
/* Functions to access the secondary device through the accel/gyro. */
int bmi260_sec_raw_read8(const int port, const uint16_t addr_flags,
			 const uint8_t reg, int *data_ptr);
int bmi260_sec_raw_write8(const int port, const uint16_t addr_flags,
			  const uint8_t reg, int data);
#endif

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ACCEL
extern struct i2c_stress_test_dev bmi260_i2c_stress_test_dev;
#endif

int bmi260_get_sensor_temp(int idx, int *temp_ptr);
#endif /* __CROS_EC_ACCELGYRO_BMI260_H */
