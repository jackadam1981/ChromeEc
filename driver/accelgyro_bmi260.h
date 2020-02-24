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

#define BMI260_CHIP_ID      0x00
#define BMI260_CHIP_ID_MAJOR    0x27

/* TODO(chingkang): check the time is correct */
#define BMI260_SPEC_ACC_STARTUP_TIME_MS     10
#define BMI260_SPEC_GYR_STARTUP_TIME_MS     80
#define BMI260_SPEC_MAG_STARTUP_TIME_MS     60

#define BMI260_ERR_REG          0x02
                                
#define BMI260_STATUS           0x03
#define BMI260_AUX_BUSY             BIT(2)
#define BMI260_CMD_RDY              BIT(4)
#define BMI260_DRDY_AUX             BIT(5)
#define BMI260_DRDY_GYR             BIT(6)
#define BMI260_DRDY_ACC             BIT(7)
#define BMI260_DRDY_OFF(_sensor)    (7 - (_sensor))
#define BMI260_DRDY_MASK(_sensor)   (1 << BMI260_DRDY_OFF(_sensor))

#define BMI260_AUX_X_L_G        0x04
#define BMI260_AUX_X_H_G        0x05
#define BMI260_AUX_Y_L_G        0x06
#define BMI260_AUX_Y_H_G        0x07
#define BMI260_AUX_Z_L_G        0x08
#define BMI260_AUX_Z_H_G        0x09
#define BMI260_AUX_R_L_G        0x0a
#define BMI260_AUX_R_H_G        0x0b

#define BMI260_ACC_X_L_G        0x0c
#define BMI260_ACC_X_H_G        0x0d
#define BMI260_ACC_Y_L_G        0x0e
#define BMI260_ACC_Y_H_G        0x0f
#define BMI260_ACC_Z_L_G        0x10
#define BMI260_ACC_Z_H_G        0x11
#define BMI260_GYR_X_L_G        0x12
#define BMI260_GYR_X_H_G        0x13
#define BMI260_GYR_Y_L_G        0x14
#define BMI260_GYR_Y_H_G        0x15
#define BMI260_GYR_Z_L_G        0x16
#define BMI260_GYR_Z_H_G        0x17


#define BMI260_SENSORTIME_0     0x18
#define BMI260_SENSORTIME_1     0x19
#define BMI260_SENSORTIME_2     0x1a

#define BMI260_EVENT            0x1b

/* first 2 bytes are the interrupt reasons, next 2 some qualifier */

#define BMI260_INT_STATUS_0     0x1c
#define BMI260_SIG_MOTION_OUT       BIT(0)
#define BMI260_STEP_COUNTER_OUT     BIT(1)
#define BMI260_HIGH_LOW_G_OUT       BIT(2)
#define BMI260_TAP_OUT              BIT(3)
#define BMI260_FLAT_OUT             BIT(4)
#define BMI260_NO_MOTION_OUT        BIT(5)
#define BMI260_ANY_MOTION_OUT       BIT(6)
#define BMI260_ORIENTATION_OUT      BIT(7)

#define BMI260_INT_STATUS_1     0x1d
#define BMI260_FFULL_INT            BIT(0 + 8)
#define BMI260_FWM_INT              BIT(1 + 8)
#define BMI260_ERR_INT              BIT(2 + 8)
#define BMI260_AUX_DRDY_INT         BIT(5 + 8)
#define BMI260_GYR_DRDY_INT         BIT(6 + 8)
#define BMI260_ACC_DRDY_INT         BIT(7 + 8)

#define BMI260_INT_MASK             0xFFFF

#define BMI260_SC_OUT_0         0x1e
#define BMI260_SC_OUT_1         0x1f


#define BMI260_ORIENT_ACT       0x20

#define BMI260_INTERNAL_STATUS  0X21
#define BMI260_MESSAGE_MASK         0xf
#define BMI260_NOT_INIT             0x00
#define BMI260_INIT_OK              0x01
#define BMI260_INIT_ERR             0x02
#define BMI260_DRV_ERR              0x03
#define BMI260_SNS_STOP             0x04
#define BMI260_NVM_ERROR            0x05
#define BMI260_START_UP_ERROR       0x06
#define BMI260_COMPAT_ERROR         0x07

#define BMI260_TEMPERATURE_0    0x22
#define BMI260_TEMPERATURE_1    0x23
#define BMI260_INVALID_TEMP         0x8000

#define BMI260_FIFO_LENGTH_0    0x24
#define BMI260_FIFO_LENGTH_1    0x25
#define BMI260_FIFO_LENGTH_MASK     (BIT(14) - 1)
#define BMI260_FIFO_DATA        0x26

enum fifo_header {
	BMI260_EMPTY = 0x80,
	BMI260_SKIP = 0x40,
	BMI260_TIME = 0x44,
	BMI260_CONFIG = 0x48
};

#define BMI260_FH_MODE_MASK     0xc0
#define BMI260_FH_PARM_OFFSET       2
#define BMI260_FH_PARM_MASK         (0x7 << BMI260_FH_PARM_OFFSET)
#define BMI260_FH_EXT_MASK      0x03

#define BMI260_FEAT_PAGE        0x2f

/* Features page 0 */
#define BMI260_ORIENT_OUT       0x36
#define BMI260_ORIENT_OUT_PORTRAIT_LANDSCAPE_MASK   3
#define BMI260_ORIENT_PORTRAIT                      0x0
#define BMI260_ORIENT_LANDSCAPE                     0x1
#define BMI260_ORIENT_PORTRAIT_INVERT               0x2
#define BMI260_ORIENT_LANDSCAPE_INVERT              0x3

/* Features page 1 */
#define BMI260_TAP_1            0x3e
#define BMI260_TAP_1_EN                     BIT(0)
#define BMI260_TAP_1_SENSITIVITY_OFFSET     1
#define BMI260_TAP_1_SENSITIVITY_MASK       \
    (0x7 << BMI260_TAP_1_SENSITIVITY_OFFSET)

/* Features page 2 */
#define BMI260_ORIENT_1         0x30
#define BMI260_ORIENT_1_EN          BIT(0)
#define BMI260_ORIENT_1_UD_EN       BIT(1)
#define BMI260_ORIENT_1_MODE_OFFSET 2
#define BMI260_ORIENT_1_MODE_MASK   (0x3 << BMI260_ORIENT_1_MODE_OFFSET)

#define BMI260_ORIENT_2         0x32



#define BMI260_ACC_CONF         0x40
#define BMI260_ODR_MASK             0x0F
#define BMI260_ACC_BW_OFFSET        4
#define BMI260_ACC_BW_MASK          (0x7 << BMI260_ACC_BW_OFFSET)
#define BMI260_FILTER_PERF          BIT(7)
#define BMI260_ULP                  0x0
#define BMI260_HP                   0x1

#define BMI260_ACC_RANGE        0x41
#define BMI260_GSEL_2G              0x00
#define BMI260_GSEL_4G              0x01
#define BMI260_GSEL_8G              0x02
#define BMI260_GSEL_16G             0x03

#define BMI260_GYR_CONF         0x42
#define BMI260_GYR_BW_OFFSET        4
#define BMI260_GYR_BW_MASK          (0x3 << BMI260_GYR_BW_OFFSET)
#define BMI260_GYR_NOISE_PERF       BIT(6)

#define BMI260_GYR_RANGE        0x43
#define BMI260_DPS_SEL_2000         0x00
#define BMI260_DPS_SEL_1000         0x01
#define BMI260_DPS_SEL_500          0x02
#define BMI260_DPS_SEL_250          0x03
#define BMI260_DPS_SEL_125          0x04

#define BMI260_AUX_CONF         0x44

/* odr = 100 / (1 << (8 - reg)) ,within limit */
#define BMI260_ODR_0_78HZ       0x01
#define BMI260_ODR_100HZ        0x08

#define BMI260_REG_TO_ODR(_regval) \
	((_regval) < BMI260_ODR_100HZ ? 100000 / (1 << (8 - (_regval))) : \
					100000 * (1 << ((_regval) - 8)))
#define BMI260_ODR_TO_REG(_odr) \
	((_odr) < 100000 ? (__builtin_clz(100000 / (_odr)) - 24) : \
			   (39 - __builtin_clz((_odr) / 100000)))

#define BMI260_CONF_REG(_sensor)   (0x40 + 2 * (_sensor))
#define BMI260_RANGE_REG(_sensor)  (0x41 + 2 * (_sensor))

#define BMI260_FIFO_DOWNS       0x45

#define BMI260_FIFO_WTM_0       0x46
#define BMI260_FIFO_WTM_1       0x47

#define BMI260_FIFO_CONFIG_0    0x48
#define BMI260_FIFO_STOP_ON_FULL    BIT(0)
#define BMI260_FIFO_TAG_TIME_EN     BIT(1)

#define BMI260_FIFO_CONFIG_1    0x49
#define BMI260_FIFO_TAG_INT1_EN_OFFSET  0
#define BMI260_FIFO_TAG_INT1_EN_MASK    (0x3 << BMI260_FIFO_TAG_INT1_EN_OFFSET)
#define BMI260_FIFO_TAG_INT2_EN_OFFSET  2
#define BMI260_FIFO_TAG_INT2_EN_MASK    (0x3 << BMI260_FIFO_TAG_INT2_EN_OFFSET)
#define BMI260_FIFO_TAG_INT_EDGE        0x0
#define BMI260_FIFO_TAG_INT_LEVEL       0x1
#define BMI260_FIFO_TAG_ACC_SAT         0x2
#define BMI260_FIFO_TAG_GYR_SAT         0x3
#define BMI260_FIFO_HEADER_EN           BIT(4)
#define BMI260_FIFO_AUX_EN              BIT(5)
#define BMI260_FIFO_ACC_EN              BIT(6)
#define BMI260_FIFO_GYR_EN              BIT(7)
#define BMI260_FIFO_SENSOR_EN(_sensor) \
	((_sensor) == MOTIONSENSE_TYPE_ACCEL ? BMI260_FIFO_ACC_EN : \
	  ((_sensor) == MOTIONSENSE_TYPE_GYRO ? BMI260_FIFO_GYR_EN : \
	   BMI260_FIFO_AUX_EN))

#define BMI260_AUX_DEV_ID       0x4b
#define BMI260_AUX_I2C_ADDRESS          BMI260_AUX_DEV_ID

#define BMI260_AUX_IF_CONF      0x4c
#define BMI260_AUX_I2C_CONTROL          BMI260_AUX_IF_CONF
#define BMI260_AUX_READ_BURST_MASK      3
#define BMI260_AUX_MAN_READ_BURST_OFF   2
#define BMI260_AUX_MAN_READ_BURST_MASK  (0x3 << BMI280_AUX_MAN_READ_BURST_OFF)
#define BMI260_AUX_READ_BURST_1         0
#define BMI260_AUX_READ_BURST_2         1
#define BMI260_AUX_READ_BURST_6         2
#define BMI260_AUX_READ_BURST_8         3
#define BMI260_AUX_FCU_WRITE_EN         BIT(6)
#define BMI260_AUX_MANUAL_EN            BIT(7)

#define BMI260_AUX_RD_ADDR      0x4d
#define BMI260_AUX_I2C_READ_ADDR    BMI260_AUX_RD_ADDR
#define BMI260_AUX_WR_ADDR      0x4e
#define BMI260_AUX_I2C_WRITE_ADDR   BMI260_AUX_WR_ADDR
#define BMI260_AUX_WR_DATA      0x4f
#define BMI260_AUX_I2C_WRITE_DATA   BMI260_AUX_WR_DATA
#define BMI260_AUX_I2C_READ_DATA    BMI260_AUX_X_L_G

/* INT: register that not exist in 260
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


// TODO: Create interrupt configuration routine specific to BM260.
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
*/
/*
 * The formula is defined in 2.11.25 (any motion interrupt [1]).
 *
 * if we want threshold at a (in mg), the register should be x, where
 * x * 7.81mg = a, assuming a range of 4G, which is
 * x * 4 * 1.953 = a so
 * x = a * 1000 / (range * 1953)
 */
/*
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
*/

/* No hysterisis, theta block, int on slope > 0.2 or axis > 1.5, symmetrical */
//#define BMI260_INT_ORIENT_0_INIT_VAL			0x48

//#define BMI260_INT_ORIENT_1				0x66

/* no axes remap, no int on up/down, no blocking angle */
//#define BMI260_INT_ORIENT_1_INIT_VAL			0x00

//#define BMI260_INT_FLAT_0      0x67
//#define BMI260_INT_FLAT_1      0x68
// end of INT

#define BMI260_ERR_REG_MSK      0x52
#define BMI260_FATAL_ERR            BIT(0)
#define BMI260_INTERNAL_ERR_OFF     1
#define BMI260_INTERNAL_ERR_MASK    (0xf << BMI260_INTERNAL_ERR_OFF)
#define BMI260_FIFO_ERR             BIT(6)
#define BMI260_AUX_ERR              BIT(7)

#define BMI260_INT1_IO_CTRL     0x53
#define BMI260_INT1_LVL             BIT(1)
#define BMI260_INT1_OD              BIT(2)
#define BMI260_INT1_OUTPUT_EN       BIT(3)
#define BMI260_INT1_INPUT_EN        BIT(4)

#define BMI260_INT2_IO_CTRL     0x54
#define BMI260_INT2_LVL             BIT(1)
#define BMI260_INT2_OD              BIT(2)
#define BMI260_INT2_OUTPUT_EN       BIT(3)
#define BMI260_INT2_INPUT_EN        BIT(4)

#define BMI260_INT_LATCH        0x55
#define BMI260_INT_LATCH_EN         BIT(0)

#define BMI260_INT1_MAP_FEAT    0x56
#define BMI260_INT2_MAP_FEAT    0x57
#define BMI260_MAP_SIG_MOTION_OUT   BIT(0)
#define BMI260_MAP_STEP_COUNTER_OUT BIT(1)
#define BMI260_MAP_HIGH_LOW_G_OUT   BIT(2)
#define BMI260_MAP_TAP_OUT          BIT(3)
#define BMI260_MAP_FLAT_OUT         BIT(4)
#define BMI260_MAP_NO_MOTION_OUT    BIT(5)
#define BMI260_MAP_ANY_MOTION_OUT   BIT(6)
#define BMI260_MAP_ORIENTAION_OUT   BIT(7)

#define BMI260_INT_MAP_DATA     0x58
#define BMI260_MAP_FFULL_INT        BIT(0)
#define BMI260_MAP_FWM_INT          BIT(1)
#define BMI260_MAP_DRDY_INT         BIT(2)
#define BMI260_MAP_ERR_INT          BIT(3)
#define BMI260_INT_MAP_DATA_INT1_OFFSET     0
#define BMI260_INT_MAP_DATA_INT2_OFFSET     4
#define BMI260_INT_MAP_DATA_REG(_i, _bit) \
	(CONCAT3(BMI260_MAP_, _bit, _INT) << CONCAT3(BMI260_INT_MAP_DATA_INT, _i, _OFFSET))

#define BMI260_INIT_CTRL        0x59
#define BMI260_INIT_ADDR_0      0x5b
#define BMI260_INIT_ADDR_1      0x5c
#define BMI260_INIT_DATA        0x5e
#define BMI260_INTERNAL_ERROR   0x5f
#define BMI260_INT_ERR_1            BIT(1)
#define BMI260_INT_ERR_2            BIT(2)
#define BMI260_FEAT_ENG_DISABLED    BIT(4)

#define BMI260_AUX_IF_TRIM      0x68
#define BMI260_GYR_CRT_CONF     0x69

#define BMI260_NVM_CONF         0x6a
#define BMI260_NVM_PROG_EN          BIT(1)

#define BMI260_IF_CONF          0x6b
#define BMI260_IF_SPI3              BIT(0)
#define BMI260_IF_SPI3_OIS          BIT(1)
#define BMI260_IF_OIS_EN            BIT(4)
#define BMI260_IF_AUX_EN            BIT(5)

#define BMI260_DRV              0x6c
#define BMI260_ACC_SELF_TEST    0x6d

#define BMI260_NV_CONF          0x70
#define BMI260_ACC_OFFSET_EN        BIT(3)

#define BMI260_OFFSET_ACC70     0x71
#define BMI260_OFFSET_ACC_MULTI_MG      (3900 * 1024)
#define BMI260_OFFSET_ACC_DIV_MG        1000000
#define BMI260_OFFSET_GYR70     0x74
#define BMI260_OFFSET_GYRO_MULTI_MDS    (61 * 1024)
#define BMI260_OFFSET_GYRO_DIV_MDS      1000
#define BMI260_OFFSET_EN_GYR98  0x77
#define BMI260_OFFSET_GYRO_EN       BIT(6)
#define BMI260_GYR_GAIN_EN          BIT(7)

#define BMI260_PWR_CONF         0x7c
#define BMI260_ADV_POWER_SAVE       BIT(0)
#define BMI260_FIFO_SELF_WAKE_UP    BIT(1)
#define BMI260_FUP_EN               BIT(2)

#define BMI260_PWR_CTRL         0x7d
#define BMI260_AUX_EN               BIT(0)
#define BMI260_GYR_EN               BIT(1)
#define BMI260_ACC_EN               BIT(2)
#define BMI260_PWR_EN(_sensor_type) BIT(2 - _sensor_type)
#define BMI260_TEMP_EN              BIT(3)

#define BMI260_CMD_REG          0x7e
#define BMI260_CMD_NOOP             0x00
#define BMI260_CMD_FIFO_FLUSH       0xb0
#define BMI260_CMD_SOFT_RESET       0xb6

/* no in 260 */
/*
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
*/

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
