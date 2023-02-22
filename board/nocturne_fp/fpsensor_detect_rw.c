/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor.h"
#include "fpsensor_detect.h"
#include "fpsensor_driver.h"
#include "gpio.h"
#include "timer.h"

enum fp_sensor_type fpsensor_detect_get_type(void)
{
	return FP_SENSOR_TYPE_FPC;
}

struct fp_sensor_interface *fpsensor_detect_get_driver(void)
{
	/* TODO: should this be HAVE_PRIVATE_FPC? */
#ifdef HAVE_PRIVATE
#if defined(CONFIG_FP_SENSOR_ELAN80) || defined(CONFIG_FP_SENSOR_ELAN515)
	return fp_driver_get_elan();
#elif defined(CONFIG_FP_SENSOR_FPC1025) || \
	defined(CONFIG_FP_SENSOR_FPC1035) || defined(CONFIG_FP_SENSOR_FPC1145)
	return fp_driver_get_fpc();
#endif
#endif /* HAVE_PRIVATE */

	/* Either we don't have private repositories or we have an unknown
	 * sensor so we have to do something sensible.
	 */
	return NULL; /* TODO: something better than NULL? */
}

enum fp_sensor_spi_select fpsensor_detect_get_spi_select(void)
{
	enum fp_sensor_spi_select ret;

	gpio_set_level(GPIO_DIVIDER_HIGHSIDE, 1);
	usleep(1);
	switch (gpio_get_level(GPIO_FP_SPI_SEL)) {
	case 0:
		ret = FP_SENSOR_SPI_SELECT_DEVELOPMENT;
		break;
	case 1:
		ret = FP_SENSOR_SPI_SELECT_PRODUCTION;
		break;
	default:
		ret = FP_SENSOR_SPI_SELECT_UNKNOWN;
		break;
	}
	gpio_set_level(GPIO_DIVIDER_HIGHSIDE, 0);
	return ret;
}
