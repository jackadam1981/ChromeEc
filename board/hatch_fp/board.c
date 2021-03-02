/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "fpsensor.h"
#include "fpsensor_detect.h"
#include "fpsensor_elan.h"
#include "fpsensor_fpc.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "usart_host_command.h"

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

static void board_init_transport(void)
{
	enum fp_transport_type ret_transport = get_fp_transport_type();
#ifdef SECTION_IS_RW
	enum fp_sensor_type sensor_type = get_fp_sensor_type();

	if (sensor_type == FP_SENSOR_TYPE_ELAN)
		elan = 1;
	else if (sensor_type == FP_SENSOR_TYPE_FPC)
		elan = 0;
	else
		ccprints("Failed to get sensor type!");

	if (elan) {
		fp_sensor_init = &fp_sensor_init_elan;
		fp_sensor_deinit = &fp_sensor_deinit_elan;
		fp_sensor_get_info = &fp_sensor_get_info_elan;
		fp_sensor_low_power = &fp_sensor_low_power_elan;
		fp_sensor_configure_detect_ptr =
			&fp_sensor_configure_detect_elan;
		fp_sensor_finger_status_ptr = &fp_sensor_finger_status_elan;
		fp_sensor_acquire_image_with_mode_ptr =
			&fp_sensor_acquire_image_with_mode_elan;
		fp_finger_enroll = &fp_finger_enroll_elan;
		fp_finger_match = &fp_finger_match_elan;
		fp_enrollment_begin = &fp_enrollment_begin_elan;
		fp_enrollment_finish = &fp_enrollment_finish_elan;
		fp_maintenance = &fp_maintenance_elan;
	} else {
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
	}
#endif

	ccprints("TRANSPORT_SEL: %s", fp_transport_type_to_str(ret_transport));

	/* Initialize transport based on bootstrap */
	switch (ret_transport) {
	case FP_TRANSPORT_TYPE_UART:
		/* Check if CONFIG_USART_HOST_COMMAND is enabled. */
		if (IS_ENABLED(CONFIG_USART_HOST_COMMAND))
			usart_host_command_init();
		else
			ccprints("ERROR: UART not supported in fw build.");

		/* Disable SPI interrupt to disable SPI transport layer */
		gpio_disable_interrupt(GPIO_SPI1_NSS);
		break;

	case FP_TRANSPORT_TYPE_SPI:
		/* SPI transport is enabled. SPI1_NSS interrupt will process
		 * incoming request/
		 */
		break;
	default:
		ccprints("ERROR: Selected transport is not valid.");
	}

	ccprints("TRANSPORT_SEL: %s",
		 fp_transport_type_to_str(get_fp_transport_type()));
}

/* Initialize board. */
static void board_init(void)
{
	/* Run until the first S3 entry.
	 * No suspend-based power management in RO.
	 */
	disable_sleep(SLEEP_MASK_AP_RUN);
	hook_notify(HOOK_CHIPSET_RESUME);
	board_init_transport();
	if (IS_ENABLED(SECTION_IS_RW))
		board_init_rw();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
