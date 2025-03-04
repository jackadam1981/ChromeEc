/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rauru, CONFIG_RAURU_LOG_LEVEL);

#define SOFT_SLEW_RATE_THRESHOULD 512 /* mA */

int rauru_charge_soft_slew(int current)
{
	static bool soft_slewed;

	/*
	 * b:395996310 Implement soft slew rate charge to prevent from
	 * syv682 PPC sensstive OCP.
	 */
	if (current <= 0) {
		soft_slewed = false;
	} else if (current >= SOFT_SLEW_RATE_THRESHOULD && !soft_slewed) {
		soft_slewed = true;
		current = SOFT_SLEW_RATE_THRESHOULD;
	} else {
		soft_slewed = true;
	};

	return current;
}
