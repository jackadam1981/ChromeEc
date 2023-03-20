/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TGL RVP ISH board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H
#include "baseboard.h"

#ifdef BOARD_TGLRVP_ISH
#define CONFIG_ACCELGYRO_LSM6DSM /* For LSM6DS3 */
#define CONFIG_ACCEL_FORCE_MODE_MASK BIT(BASE_ACCEL)

/* I2C ports */
#define I2C_PORT_SENSOR ISH_I2C1
#endif /* BOARD_TGLRVP_ISH */

#ifdef BOARD_ADL_ISH_LITE
/* EC Console Commands */
#undef CONFIG_CMD_ACCELS
#undef CONFIG_CMD_ACCEL_INFO
#undef CONFIG_CMD_TIMERINFO
#undef CONFIG_CMD_I2C_XFER
/*
 * Increases ISH D0i3 entering threshold to 3 seconds.
 * So ADL_ISH_LITE won't enter D0i3; this is to avoid extra power
 * consumption caused by D0i3 DMA copying.
 * In this way the deepest sleep ADL_ISH_LITE can enter is IPAPG + D0i2
 * while System can still go to S0i3.
 */
#undef CONFIG_ISH_D0I3_MIN_USEC
#define CONFIG_ISH_D0I3_MIN_USEC (3000 * MSEC)
#endif

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

/* Motion sensors */
enum sensor_id { BASE_ACCEL, SENSOR_COUNT };

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
