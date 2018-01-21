/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Board specific implementation for base detection. */

#include "adc_chip.h"
#include "base_detect.h"
#include "common.h"
#include "console.h"
#include "hooks.h"

/* TODO(aaboagye): Verify these values. */
#define ATTACH_MIN_MV 300
#define ATTACH_MAX_MV 500

#define DETACH_MIN_MV 0
#define DETACH_MAX_MV 100

const struct base_det_cfg base_pin_cfg = {
	.attach_pin = ADC_BASE_ATTACH,
	.detach_pin = ADC_BASE_DETACH,
};

int base_seems_attached(int attach_pin_mv, int detach_pin_mv)
{
	if (gpio_get_level(GPIO_BASE_PWR_EN))
		return (attach_pin_mv >= 2800) && (detach_pin_mv >= 2);
	else
		return (attach_pin_mv <= ATTACH_MAX_MV) &&
			(attach_pin_mv >= ATTACH_MIN_MV) &&
			(detach_pin_mv <= 5);
}

int base_seems_detached(int attach_pin_mv, int detach_pin_mv)
{
	return (attach_pin_mv >= 2800) && (detach_pin_mv <= 5);
}

static void base_detect_change(void)
{
	switch (base_get_detect_state()) {
	case BASE_DETACHED:
		/*
		 * Disable power fault interrupt.  It will read low when base
		 * power is removed.
		 */
		gpio_disable_interrupt(GPIO_BASE_PWR_FLT_L);
		/* Now, remove power to the base. */
		gpio_set_level(GPIO_BASE_PWR_EN, 0);
		break;

	case BASE_ATTACHED:
		/* Apply power to the base. */
		gpio_set_level(GPIO_BASE_PWR_EN, 1);
		/* Monitor for base power faults. */
		gpio_enable_interrupt(GPIO_BASE_PWR_FLT_L);
		break;

	default:
		break;
	};
}
DECLARE_HOOK(HOOK_BASE_DETECT_CHANGE, base_detect_change, HOOK_PRIO_DEFAULT);
