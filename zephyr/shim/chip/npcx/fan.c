/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fan.h"
#include "pwm/pwm.h"
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>

/* Data structure to define MFT (multi-function timer) channels. */
struct mft_t {
	/* PWM id */
	enum pwm_channel pwm_id;
	const struct device *dev;
};

#define MFT_INST(node_id)                                        \
	[node_id] = {                                            \
		.pwm_id = PWM_CHANNEL(DT_PHANDLE(node_id, pwm)), \
	},

/* MFT channels. These are logically separate from pwm_channels. */
static struct mft_t mft_channels[] = {
#if DT_NODE_EXISTS(DT_INST(0, named_fans))
	DT_FOREACH_CHILD(DT_INST(0, named_fans), MFT_INST)
#endif /* named_fan */
};

#define MFT_DEV_INIT(node_id) {                                               \
	mft_channels[node_id].dev =                                           \
		device_get_binding(DT_PROP_BY_PHANDLE(node_id, tach, label)); \
	}

int fan_rpm(int ch)
{
	struct sensor_value val = { 0 };

	sensor_sample_fetch_chan(mft_channels[ch].dev, SENSOR_CHAN_RPM);
	sensor_channel_get(mft_channels[ch].dev, SENSOR_CHAN_RPM, &val);

	return (int)val.val1;
}

void fan_rpm_setup(int ch, unsigned int flags)
{
	if (flags & FAN_USE_RPM_MODE)
		DT_FOREACH_CHILD(DT_INST(0, named_fans), MFT_DEV_INIT)

}

enum pwm_channel fan_get_pwm_id(int ch)
{
	return mft_channels[ch].pwm_id;
}

int fan_is_stalled(int ch)
{
	return fan_get_enabled(ch) && fan_get_duty(ch) &&
	       !fan_get_rpm_actual(ch);
}
