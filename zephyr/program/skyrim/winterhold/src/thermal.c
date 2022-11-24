/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "temp_sensor/temp_sensor.h"
#include "host_command.h"

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ##args)

#define TEMP_AMB TEMP_SENSOR_ID(DT_NODELABEL(temp_sensor_amb))

static int last_amb_temp = -1;

/* Set SCI event to host for temperature change */
static void detect_temp_change(void)
{
	int t, rv;

	rv = temp_sensor_read(TEMP_AMB, &t);
	switch (rv) {
	case EC_SUCCESS:
		if (last_amb_temp != t) {
			last_amb_temp = t;
			host_set_single_event(EC_HOST_EVENT_THERMAL_THRESHOLD);
		}
		break;
	case EC_ERROR_NOT_POWERED:
		CPRINTS("Temp sensor: Not powered");
		break;
	case EC_ERROR_INVAL:
		CPRINTS("Temp sensor: Invalid id");
		break;
	default:
		CPRINTS("Temp sensor: Error %d", rv);
		break;
	}
}
DECLARE_HOOK(HOOK_SECOND, detect_temp_change, HOOK_PRIO_TEMP_SENSOR_DONE);
