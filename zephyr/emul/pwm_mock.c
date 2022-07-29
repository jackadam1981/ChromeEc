/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_pwm_mock

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include "console.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)

static int pwm_mock_init(const struct device *dev)
{
	return 0;
}

static int pwm_mock_set_cycles(const struct device *dev,
			       uint32_t channel, uint32_t period_cycles,
			       uint32_t pulse_cycles, pwm_flags_t flags)
{
	return 0;
}

static int pwm_mock_get_cycles_per_sec(const struct device *dev,
			       uint32_t channel, uint64_t *cycles)
{
	return 0;
}

static const struct pwm_driver_api pwm_mock_api = {
	.set_cycles = pwm_mock_set_cycles,
	.get_cycles_per_sec = pwm_mock_get_cycles_per_sec,
};

#define INIT_PWM_MOCK(inst) \
	DEVICE_DT_INST_DEFINE(inst, \
			      &pwm_mock_init, \
			      NULL, \
			      NULL, \
			      NULL, \
			      PRE_KERNEL_1, \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			      &pwm_mock_api);

DT_INST_FOREACH_STATUS_OKAY(INIT_PWM_MOCK)
