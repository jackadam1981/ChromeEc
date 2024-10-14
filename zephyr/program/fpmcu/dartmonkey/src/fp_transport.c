/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <ec_commands.h>
#include <fpsensor/fpsensor_detect.h>
#include <gpio_signal.h>

enum fp_transport_type get_fp_transport_type(void)
{
	return FP_TRANSPORT_TYPE_SPI;
}
