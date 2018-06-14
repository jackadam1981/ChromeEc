/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LIS2MDL mag module for Chrome EC. */

#ifndef __CROS_EC_MAG_LIS2MDL_H
#define __CROS_EC_MAG_LIS2MDL_H

#include "accelgyro.h"
#include "mag_cal.h"
#include "stm_mems_common.h"

#define LIS2MDL_EN_BIT             1
#define LIS2MDL_DIS_BIT            0
#define LIS2MDL_I2C_ADDR(__x)      (__x << 1)

/*
 * 7-bit address is 0011110Xb. Where 'X' is determined
 * by the voltage on the ADDR pin
 */
#define LIS2MDL_ADDR0              LIS2MDL_I2C_ADDR(0x1e)
#define LIS2MDL_ADDR1              LIS2MDL_I2C_ADDR(0x1f)

/* Registers */
#define LIS2MDL_WHO_AM_I_REG       0x4f
#define LIS2MDL_WHOAMI_VAL         0x40

#define LIS2MDL_CFG_REG_A          0x60
#define LIS2MDL_COMP_TEMP_EN       (1 << 7)
#define LIS2MDL_REBOOT             (1 << 6)
#define LIS2MDL_SOFT_RST           (1 << 5)
#define LIS2MDL_LP                 (1 << 4)
/* Supported device ODR */
#define LIS2MDL_ODR10_HZ           0x00
#define LIS2MDL_ODR20_HZ           0x04
#define LIS2MDL_ODR50_HZ           0x08
#define LIS2MDL_ODR100_HZ          0x0c
#define LIS2MDL_ODR_OFFSET         2
#define LIS2MDL_ODR_MASK           (0x3 << LIS2MDL_ODR_OFFSET)
/* Operation MoD */
#define LIS2MDL_MD_CONTINUOS_MODE  0x00
#define LIS2MDL_MD_SINGLE_MODE     0x01
#define LIS2MDL_MD_IDLE1_MODE      0x02
#define LIS2MDL_MD_IDLE2_MODE      0x03
#define LIS2MDL_MODE_MASK          0x03

#define LIS2MDL_CFG_REG_B          0x61
#define LIS2MDL_LPF                (1 << 0)
#define LIS2MDL_OFF_CANC           (1 << 1)
#define LIS2MDL_SET_FREQ           (1 << 2)
#define LIS2MDL_INT_ON_DATAOFF     (1 << 3)
#define LIS2MDL_OFF_CANC_ONE_SHOT  (1 << 4)

#define LIS2MDL_CFG_REG_C          0x62
#define LIS2MDL_BDU                (1 << 4)

#define LIS2MDL_INT_CRTL_REG       0x63
#define LIS2MDL_STATUS_REG         0x67
#define LIS2MDL_OUT_REG            0x68

/* Output data size */
#define LIS2MDL_OUT_REG_SIZE       OUT_XYZ_SIZE

/* Define ODR supported range. */
#define LIS2MDL_MAX_ODR            100000
/*
 * In single shot mode, we can go really low, but the data
 * would be impacted by drift.
 */
#define LIS2MDL_MIN_ODR            10000

/* Return ODR real value normalized to sensor capabilities. */
#define LIS2MDL_ODR_TO_NORMALIZE(_odr) \
	(_odr < 20000 ? LIS2MDL_MIN_ODR : \
	 _odr < 50000 ? 20000 : \
	 _odr < LIS2MDL_MAX_ODR ? 50000 : LIS2MDL_MAX_ODR)

/* Assume _odr is normalized. */
#define LIS2MDL_ODR_TO_REG(_odr) (__fls(_odr))

/* Sensor resolution in number of bits, including sign. */
#define LIS2MDL_RESOLUTION         16

/*
 * Maximum sensor data range (milligauss):
 * Spec is 1.5 mguass / LSB, so 0.15 uT / LSB.
 * 0.15 << 15 = 4915.2(uT) (49.152 Gauss)
 */
#define LIS2MDL_RANGE              4915

#ifndef CONFIG_MAG_BMI160_LIS2MDL
extern const struct accelgyro_drv lis2mdl_drv;
#endif

struct lis2mdl_private_data {
	struct mag_cal_t cal;  /* must be first */
#ifdef CONFIG_MAG_BMI160_LIS2MDL
	vector_3_t       hn;   /* last sample for offset compensation */
	int              hn_valid;
#endif
};

#ifdef CONFIG_MAG_BMI160_LIS2MDL
#define LIS2MDL_DATA(_s) (&BMI160_GET_DATA(_s)->compass)
#define LIS2MDL_CAL(_s) (&LIS2MDL_DATA(_s)->cal)
#endif

int lis2mdl_init(const struct motion_sensor_t *s);
void lis2mdl_normalize(const struct motion_sensor_t *s,
		       vector_3_t v,
		       uint8_t *data);

#endif /* __CROS_EC_MAG_LIS2MDL */
