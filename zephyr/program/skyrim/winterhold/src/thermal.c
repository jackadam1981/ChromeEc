/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "body_detection.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "temp_sensor/temp_sensor.h"
#include "thermal.h"

#include <zephyr/thermal/thermal.h>

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

#define TEMP_AMB TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_amb))

static void enable_fans(void)
{
	thermal_enable_profile(DEVICE_DT_GET(DT_NODELABEL(thermal_profile)), true);

}
DECLARE_HOOK(HOOK_CHIPSET_RESET, enable_fans, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, enable_fans, HOOK_PRIO_DEFAULT);

static void disable_fans(void)
{
	thermal_enable_profile(DEVICE_DT_GET(DT_NODELABEL(thermal_profile)), false);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, disable_fans, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, disable_fans, HOOK_PRIO_DEFAULT);

static int last_amb_temp = -1;

/* Set SCI event to host for temperature change */
static void detect_temp_change(void)
{
	int t, rv;

	rv = temp_sensor_read(TEMP_AMB, &t);
	if (rv == EC_SUCCESS) {
		if (last_amb_temp != t) {
			last_amb_temp = t;
			host_set_single_event(EC_HOST_EVENT_THERMAL_THRESHOLD);
		}
	} else if (rv == EC_ERROR_INVAL) {
		CPRINTS("Temp sensor: Invalid id");
	}
}
DECLARE_HOOK(HOOK_SECOND, detect_temp_change, HOOK_PRIO_TEMP_SENSOR_DONE);
