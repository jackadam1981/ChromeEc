/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BMI220 accelerometer and gyro for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_BMI220_H
#define __CROS_EC_ACCELGYRO_BMI220_H

#include "accelgyro.h"
#include "common.h"
#include "mag_bmm150.h"
#include "driver/accelgyro_bmi220_public.h"

#define BMI220_CHIP_ID      0x00
#define BMI220_CHIP_ID_MAJOR    0x26

#define BMI220_ERR_REG          0x02

#define BMI220_STATUS           0x03
#define BMI220_AUX_BUSY             BIT(2)
#define BMI220_CMD_RDY              BIT(4)
#define BMI220_DRDY_AUX             BIT(5)
#define BMI220_DRDY_GYR             BIT(6)
#define BMI220_DRDY_ACC             BIT(7)
#define BMI220_DRDY_OFF(_sensor)    (7 - (_sensor))
#define BMI220_DRDY_MASK(_sensor)   (1 << BMI220_DRDY_OFF(_sensor))

#define BMI220_AUX_X_L_G           0x04
#define BMI220_AUX_X_H_G           0x05
#define BMI220_AUX_Y_L_G           0x06
#define BMI220_AUX_Y_H_G           0x07
#define BMI220_AUX_Z_L_G           0x08
#define BMI220_AUX_Z_H_G           0x09
#define BMI220_AUX_R_L_G           0x0a
#define BMI220_AUX_R_H_G           0x0b
#define BMI220_ACC_X_L_G           0x0c
#define BMI220_ACC_X_H_G           0x0d
#define BMI220_ACC_Y_L_G           0x0e
#define BMI220_ACC_Y_H_G           0x0f
#define BMI220_ACC_Z_L_G           0x10
#define BMI220_ACC_Z_H_G           0x11
#define BMI220_GYR_X_L_G           0x12
#define BMI220_GYR_X_H_G           0x13
#define BMI220_GYR_Y_L_G           0x14
#define BMI220_GYR_Y_H_G           0x15
#define BMI220_GYR_Z_L_G           0x16
#define BMI220_GYR_Z_H_G           0x17

#define BMI220_SENSORTIME_0     0x18
#define BMI220_SENSORTIME_1     0x19
#define BMI220_SENSORTIME_2     0x1a

#define BMI220_EVENT            0x1b

/* 2 bytes interrupt reasons*/
#define BMI220_INT_STATUS_0     0x1c

#define BMI220_STEP_COUNTER_OUT     BIT(1)

#define BMI220_NO_MOTION_OUT        BIT(5)
#define BMI220_ANY_MOTION_OUT       BIT(6)


#define BMI220_INT_STATUS_1     0x1d
#define BMI220_FFULL_INT            BIT(0 + 8)
#define BMI220_FWM_INT              BIT(1 + 8)
#define BMI220_ERR_INT              BIT(2 + 8)
#define BMI220_AUX_DRDY_INT         BIT(5 + 8)
#define BMI220_GYR_DRDY_INT         BIT(6 + 8)
#define BMI220_ACC_DRDY_INT         BIT(7 + 8)

#define BMI220_INT_MASK             0xFFFF

#define BMI220_SC_OUT_0         0x1e
#define BMI220_SC_OUT_1         0x1f



#define BMI220_INTERNAL_STATUS  0X21
#define BMI220_MESSAGE_MASK         0xf
#define BMI220_NOT_INIT             0x00
#define BMI220_INIT_OK              0x01
#define BMI220_INIT_ERR             0x02
#define BMI220_DRV_ERR              0x03
#define BMI220_SNS_STOP             0x04
#define BMI220_NVM_ERROR            0x05
#define BMI220_START_UP_ERROR       0x06
#define BMI220_COMPAT_ERROR         0x07

#define BMI220_TEMPERATURE_0    0x22
#define BMI220_TEMPERATURE_1    0x23

#define BMI220_FIFO_LENGTH_0    0x24
#define BMI220_FIFO_LENGTH_1    0x25
#define BMI220_FIFO_LENGTH_MASK     (BIT(14) - 1)
#define BMI220_FIFO_DATA        0x26

#define BMI220_FEAT_PAGE        0x2f



#define BMI220_ACC_CONF         0x40
#define BMI220_ACC_BW_OFFSET        4
#define BMI220_ACC_BW_MASK          (0x7 << BMI220_ACC_BW_OFFSET)
#define BMI220_FILTER_PERF          BIT(7)
#define BMI220_ULP                  0x0
#define BMI220_HP                   0x1

#define BMI220_ACC_RANGE        0x41
#define BMI220_GSEL_2G              0x00
#define BMI220_GSEL_4G              0x01
#define BMI220_GSEL_8G              0x02
#define BMI220_GSEL_16G             0x03

/* The max positvie value of accel data is 0x7FFF, equal to range(g) */
/* So, in order to get +1g, divide the 0x7FFF by range */
#define BMI220_ACC_DATA_PLUS_1G(range) (0x7FFF / (range))
#define BMI220_ACC_DATA_MINUS_1G(range) (-BMI220_ACC_DATA_PLUS_1G(range))

#define BMI220_GYR_CONF         0x42
#define BMI220_GYR_BW_OFFSET        4
#define BMI220_GYR_BW_MASK          (0x3 << BMI220_GYR_BW_OFFSET)
#define BMI220_GYR_NOISE_PERF       BIT(6)

#define BMI220_GYR_RANGE        0x43
#define BMI220_DPS_SEL_2000         0x00
#define BMI220_DPS_SEL_1000         0x01
#define BMI220_DPS_SEL_500          0x02
#define BMI220_DPS_SEL_250          0x03
#define BMI220_DPS_SEL_125          0x04

#define BMI220_AUX_CONF         0x44

#define BMI220_FIFO_DOWNS       0x45

#define BMI220_FIFO_WTM_0       0x46
#define BMI220_FIFO_WTM_1       0x47

#define BMI220_FIFO_CONFIG_0    0x48
#define BMI220_FIFO_STOP_ON_FULL    BIT(0)
#define BMI220_FIFO_TIME_EN     BIT(1)

#define BMI220_FIFO_CONFIG_1    0x49
#define BMI220_FIFO_TAG_INT1_EN_OFFSET  0
#define BMI220_FIFO_TAG_INT1_EN_MASK    (0x3 << BMI220_FIFO_TAG_INT1_EN_OFFSET)
#define BMI220_FIFO_TAG_INT2_EN_OFFSET  2
#define BMI220_FIFO_TAG_INT2_EN_MASK    (0x3 << BMI220_FIFO_TAG_INT2_EN_OFFSET)
#define BMI220_FIFO_TAG_INT_EDGE        0x0
#define BMI220_FIFO_TAG_INT_LEVEL       0x1
#define BMI220_FIFO_TAG_ACC_SAT         0x2
#define BMI220_FIFO_TAG_GYR_SAT         0x3
#define BMI220_FIFO_HEADER_EN           BIT(4)
#define BMI220_FIFO_AUX_EN              BIT(5)
#define BMI220_FIFO_ACC_EN              BIT(6)
#define BMI220_FIFO_GYR_EN              BIT(7)
#define BMI220_FIFO_SENSOR_EN(_sensor) \
	((_sensor) == MOTIONSENSE_TYPE_ACCEL ? BMI220_FIFO_ACC_EN : \
	  ((_sensor) == MOTIONSENSE_TYPE_GYRO ? BMI220_FIFO_GYR_EN : \
	   BMI220_FIFO_AUX_EN))

#define BMI220_AUX_DEV_ID       0x4b
#define BMI220_AUX_I2C_ADDRESS          BMI220_AUX_DEV_ID

#define BMI220_AUX_IF_CONF      0x4c
#define BMI220_AUX_I2C_CONTROL          BMI220_AUX_IF_CONF
#define BMI220_AUX_READ_BURST_MASK      3
#define BMI220_AUX_MAN_READ_BURST_OFF   2
#define BMI220_AUX_MAN_READ_BURST_MASK  (0x3 << BMI220_AUX_MAN_READ_BURST_OFF)
#define BMI220_AUX_READ_BURST_1         0
#define BMI220_AUX_READ_BURST_2         1
#define BMI220_AUX_READ_BURST_6         2
#define BMI220_AUX_READ_BURST_8         3
#define BMI220_AUX_FCU_WRITE_EN         BIT(6)
#define BMI220_AUX_MANUAL_EN            BIT(7)

#define BMI220_AUX_RD_ADDR      0x4d
#define BMI220_AUX_I2C_READ_ADDR    BMI220_AUX_RD_ADDR
#define BMI220_AUX_WR_ADDR      0x4e
#define BMI220_AUX_I2C_WRITE_ADDR   BMI220_AUX_WR_ADDR
#define BMI220_AUX_WR_DATA      0x4f
#define BMI220_AUX_I2C_WRITE_DATA   BMI220_AUX_WR_DATA
#define BMI220_AUX_I2C_READ_DATA    BMI220_AUX_X_L_G

#define BMI220_ERR_REG_MSK      0x52
#define BMI220_FATAL_ERR            BIT(0)
#define BMI220_INTERNAL_ERR_OFF     1
#define BMI220_INTERNAL_ERR_MASK    (0xf << BMI220_INTERNAL_ERR_OFF)
#define BMI220_FIFO_ERR             BIT(6)
#define BMI220_AUX_ERR              BIT(7)

#define BMI220_INT1_IO_CTRL     0x53
#define BMI220_INT1_LVL             BIT(1)
#define BMI220_INT1_OD              BIT(2)
#define BMI220_INT1_OUTPUT_EN       BIT(3)
#define BMI220_INT1_INPUT_EN        BIT(4)

#define BMI220_INT2_IO_CTRL     0x54
#define BMI220_INT2_LVL             BIT(1)
#define BMI220_INT2_OD              BIT(2)
#define BMI220_INT2_OUTPUT_EN       BIT(3)
#define BMI220_INT2_INPUT_EN        BIT(4)

#define BMI220_INT_LATCH        0x55
#define BMI220_INT_LATCH_EN         BIT(0)

#define BMI220_INT1_MAP_FEAT    0x56
#define BMI220_INT2_MAP_FEAT    0x57
#define BMI220_MAP_SIG_MOTION_OUT   BIT(0)
#define BMI220_MAP_STEP_COUNTER_OUT BIT(1)
#define BMI220_MAP_HIGH_LOW_G_OUT   BIT(2)
#define BMI220_MAP_TAP_OUT          BIT(3)
#define BMI220_MAP_FLAT_OUT         BIT(4)
#define BMI220_MAP_NO_MOTION_OUT    BIT(5)
#define BMI220_MAP_ANY_MOTION_OUT   BIT(6)
#define BMI220_MAP_ORIENTAION_OUT   BIT(7)

#define BMI220_INT_MAP_DATA     0x58
#define BMI220_MAP_FFULL_INT        BIT(0)
#define BMI220_MAP_FWM_INT          BIT(1)
#define BMI220_MAP_DRDY_INT         BIT(2)
#define BMI220_MAP_ERR_INT          BIT(3)
#define BMI220_INT_MAP_DATA_INT1_OFFSET     0
#define BMI220_INT_MAP_DATA_INT2_OFFSET     4
#define BMI220_INT_MAP_DATA_REG(_i, _bit) \
	(CONCAT3(BMI220_MAP_, _bit, _INT) << \
	CONCAT3(BMI220_INT_MAP_DATA_INT, _i, _OFFSET))

#define BMI220_INIT_CTRL        0x59
#define BMI220_INIT_ADDR_0      0x5b
#define BMI220_INIT_ADDR_1      0x5c
#define BMI220_INIT_DATA        0x5e
#define BMI220_INTERNAL_ERROR   0x5f
#define BMI220_INT_ERR_1            BIT(1)
#define BMI220_INT_ERR_2            BIT(2)
#define BMI220_FEAT_ENG_DISABLED    BIT(4)

#define BMI220_AUX_IF_TRIM      0x68
#define BMI220_GYR_CRT_CONF     0x69

#define BMI220_NVM_CONF         0x6a
#define BMI220_NVM_PROG_EN          BIT(1)

#define BMI220_IF_CONF          0x6b
#define BMI220_IF_SPI3              BIT(0)
#define BMI220_IF_SPI3_OIS          BIT(1)
#define BMI220_IF_OIS_EN            BIT(4)
#define BMI220_IF_AUX_EN            BIT(5)

#define BMI220_DRV              0x6c
#define BMI220_ACC_SELF_TEST    0x6d

#define BMI220_NV_CONF          0x70
#define BMI220_ACC_OFFSET_EN        BIT(3)

#define BMI220_OFFSET_ACC70     0x71
#define BMI220_OFFSET_GYR70     0x74
#define BMI220_OFFSET_EN_GYR98  0x77
#define BMI220_OFFSET_GYRO_EN       BIT(6)
#define BMI220_GYR_GAIN_EN          BIT(7)

#define BMI220_PWR_CONF         0x7c
#define BMI220_ADV_POWER_SAVE       BIT(0)
#define BMI220_FIFO_SELF_WAKE_UP    BIT(1)
#define BMI220_FUP_EN               BIT(2)

#define BMI220_PWR_CTRL         0x7d
#define BMI220_AUX_EN               BIT(0)
#define BMI220_GYR_EN               BIT(1)
#define BMI220_ACC_EN               BIT(2)
#define BMI220_PWR_EN(_sensor_type) BIT(2 - _sensor_type)
#define BMI220_TEMP_EN              BIT(3)

#define BMI220_CMD_REG          0x7e
#define BMI220_CMD_FIFO_FLUSH       0xb0
#define BMI220_CMD_SOFT_RESET       0xb6

#define BMI220_FF_FRAME_LEN_TS          4
#define BMI220_FF_DATA_LEN_ACC          6
#define BMI220_FF_DATA_LEN_GYR          6
#define BMI220_FF_DATA_LEN_MAG          8

/* Root mean square noise of 100Hz accelerometer, units: ug */
#define BMI220_ACCEL_RMS_NOISE_100HZ    1060

#endif /* __CROS_EC_ACCELGYRO_BMI220_H */
