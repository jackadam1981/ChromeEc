/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Header file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#ifndef __CROS_EC_PD_TASK_INTEL_ALTMODE_H
#define __CROS_EC_PD_TASK_INTEL_ALTMODE_H

#include "i2c.h"

struct pd_config_t {
	struct i2c_info_t i2c_info; /* I2C details */
	enum gpio_signal alert_signal; /* Active low interrupt pin */
};

extern const struct pd_config_t pd_config[];

/**
 * PD interrupt to wake the task to configure alternate modes
 *
 * Thre can be individual Interrupt pin for each PD port or all the PD
 * interrupts can be muxed to single GPIO to keep common code for
 * single port / dual port PD solutions offered by different PD vendors.
 *
 * @param signal Signal that generates the interrupt
 */
void pd_altmode_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_PD_TASK_INTEL_ALTMODE_H */
