/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "util.h"

#include <zephyr/drivers/fan.h>
#include <zephyr/logging/log.h>

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)

struct fan {
	const struct device *dev;
};

#define APPEND_FAN(i)	\
	{ .dev = DEVICE_DT_GET(i) }, \

static struct fan fans[]= {
	DT_FOREACH_STATUS_OKAY(zephyr_fan_pid, APPEND_FAN)
};

int fan_get_count()
{
	return ARRAY_SIZE(fans);
}

int dptf_get_fan_duty_target()
{
	enum z_fan_mode mode;
	int ret, duty;

	/* TODO(crosbug.com/p/23803) */

	if(fan_get_count() == 0)
		return -1;

	ret = z_impl_fan_get_mode(fans[0].dev, &mode);
	if (ret || mode != FAN_MODE_PWM)
		return -1;

	ret = z_impl_fan_get_speed(fans[0].dev, FAN_SPEED_TYPE_PWM, &duty);
	if (ret != 0)
		return ret;

	return duty;
}

void dptf_set_fan_duty_target(int duty)
{
	int ret, i;

	/* TODO: Enable thermal control if duty is outside [0-100] */
	if (duty < 0 || duty > 100)
		return;

	for (i = 0; i < fan_get_count(); i++) {
		ret = z_impl_fan_set_mode(fans[i].dev, FAN_MODE_PWM);
		if (ret != 0)
			return;

		(void)z_impl_fan_set_speed(fans[i].dev, duty);
	}
}

/* TODO */
int fan_set_percent_needed(int ch, int pct)
{
	return 0;
}
