/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Header for sar_sense.c */

#ifndef __CROS_EC_SAR_SENSE_H
#define __CROS_EC_SAR_SENSE_H

#include "chipset.h"
#include "common.h"
#include "ec_commands.h"
#include "gpio.h"
#include "math_util.h"

enum sar_chip_t {
	SENSOR_CHIP_SX9310 = 0,
};

#define SENSOR_ACTIVE_S5 CHIPSET_STATE_SOFT_OFF
#define SENSOR_ACTIVE_S3 CHIPSET_STATE_SUSPEND
#define SENSOR_ACTIVE_S0 CHIPSET_STATE_ON
#define SENSOR_ACTIVE_S0_S3 (SENSOR_ACTIVE_S3 | SENSOR_ACTIVE_S0)
#define SENSOR_ACTIVE_S0_S3_S5 (SENSOR_ACTIVE_S0_S3 | SENSOR_ACTIVE_S5)

struct sar_sensor_t {
	uint32_t active_mask;
	char *name;
	enum sar_chip_t chip;
	const struct sar_drv *drv;
	struct mutex *mutex;
	void *drv_data;
	uint8_t i2c_addr;
	int resolution;
	int sens;
	uint32_t status;
	enum chipset_state_mask active;
};

/* Defined at board level. */
extern struct sar_sensor_t sar_sensors[];
extern const unsigned int sar_sensor_count;

/*
 * Priority of the sar sense resume/suspend hooks, to be sure associated
 * hooks are scheduled properly.
 */
#define SAR_SENSE_HOOK_PRIO (HOOK_PRIO_DEFAULT)

#endif /* __CROS_EC_SAR_SENSE_H */

