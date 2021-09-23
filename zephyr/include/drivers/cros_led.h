/*
 * Copyright 2021 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CROS_LED_H_
#define ZEPHYR_INCLUDE_DRIVERS_CROS_LED_H_

#include <kernel.h>
#include <device.h>
#include <devicetree.h>

#include "ec_commands.h"
#include "led_common.h"

#define LED_ENUM(id) DT_ENUM_UPPER_TOKEN(id, enum_name)
#define LED_ENUM_WITH_COMMA(id) \
    COND_CODE_1(DT_NODE_HAS_PROP(id, enum_name), (LED_ENUM(id), ), ())
#define CROS_LED_LABEL "cros_led"

/* gpio  LED colors */
enum led_color {
    LED_OFF = 0,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_YELLOW,
    LED_AMBER,
    LED_WHITE,
    LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

/* All supported board names */
enum board_names {
#if DT_NODE_EXISTS(DT_NODELABEL(board_names))
    DT_FOREACH_CHILD(DT_NODELABEL(board_names), LED_ENUM_WITH_COMMA)
#endif
};

struct battery_led_color {
    /* Charge State */
    enum led_color charging_led;

    /* If discharge LED color is dependent on chipset state  */
    bool chipset_state;

    /* Discharge state */
#if DT_NODE_HAS_PROP(DT_NODELABEL(discharge_s3), period) 
    enum led_color discharge_s0_led;
    enum led_color discharge_s3_led_1;
    enum led_color discharge_s3_led_2;
    enum led_color discharge_s5_led;
    int discharge_s3_led_period;
#endif

    /* Error state */
    enum led_color error_led_1;
    enum led_color error_led_2;
    int error_led_period;

    /* Near full state */
    enum led_color near_full_led;

    /* Idle state */
    enum led_color idle_led_1;
    enum led_color idle_led_2;
    enum led_color idle_led_3;
    int idle_led_period;
};

#if DT_NODE_EXISTS(DT_NODELABEL(led_id_power))
struct power_led_color {
    /* Power state S0 */
    enum led_color s0_led;

    /* Power state S3 */
    enum led_color s3_led_1;
    enum led_color s3_led_2;
    int s3_period;

    /* Power state S5  */
    enum led_color s5_led;
};
#endif

/**
 * @brief CROS LED Driver APIs
 * @defgroup cros_led_interface CROS LED Driver APIs
 * @ingroup io_interfaces
 * @{
 */

/**
 * @cond INTERNAL_HIDDEN
 *
 * cros LED driver API definition and system call entry points
 *
 * (Internal use only.)
 */
typedef int (*cros_led_api_init)(const struct device *dev);

typedef int (*cros_led_api_set_brightness)(const struct device *dev,
					     enum ec_led_id, const uint8_t *brightness);

typedef int (*cros_led_api_get_brightness_range)(const struct device *dev,
					     enum ec_led_id led_id, uint8_t *brightness_range);

typedef int (*cros_board_led_api_set_battery)(const struct device *dev);

typedef int (*cros_led_api_control)(const struct device *dev,
                        enum ec_led_id led_id, enum ec_led_state state);

__subsystem struct cros_led_driver_api {
	cros_led_api_init init;
	cros_led_api_set_brightness led_set_brightness;
	cros_led_api_get_brightness_range led_get_brightness_range;
	cros_board_led_api_set_battery board_led_set_battery;
	cros_led_api_control led_control;
};

/**
 * @endcond
 */

/**
 * @brief Initialize LED.
 *
 * @param dev Pointer to the device structure for the LED instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_led_init(const struct device *dev);
static inline int z_impl_cros_led_init(const struct device *dev)
{
	const struct cros_led_driver_api *api =
		(const struct cros_led_driver_api *)dev->api;

	if (!api->init) {
		return -ENOTSUP;
	}

	return api->init(dev);
}

/**
 * @brief Set brightness.
 *
 * @param dev Pointer to the device structure for the LED instance.
 * @param led_id LED ID to set brightness for
 * @param brightness Mapping for LED colors 
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_led_set_brightness(const struct device *dev,
                enum ec_led_id led_id, const uint8_t *brightness);
static inline int z_impl_cros_led_set_brightness(const struct device *dev,
                enum ec_led_id led_id, const uint8_t *brightness)
{
	const struct cros_led_driver_api *api =
		(const struct cros_led_driver_api *)dev->api;

	if (!api->led_set_brightness) {
		return -ENOTSUP;
	}

	return api->led_set_brightness(dev, led_id, brightness);
}

/**
 * @brief Get brightness range.
 *
 * @param dev Pointer to the device structure for the LED instance.
 * @param led_id LED ID to get brightness for
 * @param brightness_range Mapping for LED colors 
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_led_get_brightness_range(const struct device *dev,
					     enum ec_led_id led_id, uint8_t *brightness_range);
static inline int z_impl_cros_led_get_brightness_range(const struct device *dev,
					     enum ec_led_id led_id, uint8_t *brightness_range)
{
	const struct cros_led_driver_api *api =
		(const struct cros_led_driver_api *)dev->api;

	if (!api->led_get_brightness_range) {
		return -ENOTSUP;
	}

	return api->led_get_brightness_range(dev, led_id, brightness_range);
}

/**
 * @brief Set battery led.
 *
 * @param dev Pointer to the device structure for the LED instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_board_led_set_battery(const struct device *dev);
static inline int z_impl_cros_board_led_set_battery(const struct device *dev)
{
	const struct cros_led_driver_api *api =
		(const struct cros_led_driver_api *)dev->api;

	if (!api->board_led_set_battery) {
		return -ENOTSUP;
	}

	return api->board_led_set_battery(dev);
}

/**
 * @brief Led control.
 *
 * @param dev Pointer to the device structure for the LED instance.
 * @param led_id LED ID to control 
 * @param state LED state 
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_led_control(const struct device *dev,
                        enum ec_led_id led_id, enum ec_led_state state);
static inline int z_impl_cros_led_control(const struct device *dev,
                        enum ec_led_id led_id, enum ec_led_state state)
{
	const struct cros_led_driver_api *api =
		(const struct cros_led_driver_api *)dev->api;

	if (!api->led_control) {
		return -ENOTSUP;
	}

	return api->led_control(dev, led_id, state);
}

/**
 * @}
 */
#include <syscalls/cros_led.h>
#endif
