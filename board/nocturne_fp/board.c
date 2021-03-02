/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Meowth Fingerprint MCU configuration */

#include "common.h"
#include "fpsensor.h"
#include "fpsensor_fpc.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"

/**
 * Disable restricted commands when the system is locked.
 *
 * @see console.h system.c
 */
int console_is_restricted(void)
{
	return system_is_locked();
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	if (IS_ENABLED(SECTION_IS_RW)) {
		board_init_rw();
	} else {
		/* No suspend-based power management in RO. */
		disable_sleep(SLEEP_MASK_AP_RUN);
		hook_notify(HOOK_CHIPSET_RESUME);
	}

#ifdef SECTION_IS_RW
	elan = 0;

	fp_sensor_init = &fp_sensor_init_fpc;
	fp_sensor_deinit = &fp_sensor_deinit_fpc;
	fp_sensor_get_info = &fp_sensor_get_info_fpc;
	fp_sensor_low_power = &fp_sensor_low_power_fpc;
	fp_sensor_configure_detect_ptr = &fp_sensor_configure_detect;
	fp_sensor_finger_status_ptr = &fp_sensor_finger_status;
	fp_sensor_acquire_image_with_mode_ptr =
		&fp_sensor_acquire_image_with_mode;
	fp_finger_enroll = &fp_finger_enroll_fpc;
	fp_finger_match = &fp_finger_match_fpc;
	fp_enrollment_begin = &fp_enrollment_begin_fpc;
	fp_enrollment_finish = &fp_enrollment_finish_fpc;
	fp_maintenance = &fp_maintenance_fpc;
#endif
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
