/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ZEPHYR_WRITE_PROTECT_H
#define __CROS_EC_ZEPHYR_WRITE_PROTECT_H

#include "gpio/gpio_int.h"
#include "gpio_signal.h"

#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_BOARD_PTLRVP_MCHP
#ifdef CONFIG_CROS_SYSTEM_XEC
/**
 * Sync up gpio_wp state with mchp_wp state. Update GPIO076 state according to
 * gpio_wp state set from SOC.
 *
 * @return none.
 */
static inline void sync_wp_assert_status(void)
{
	/* For MEC1727, Sync up mchp_wp state with gpio_wp state */
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(mchp_wp),
			gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(mchp_wp_ex)));
}
#endif
#endif

/**
 * Check the WP state. The function depends on the alias 'gpio_wp'. It is used
 * to replace the enum-name.
 *
 * @return true if the WP is active, false otherwise.
 */
static inline int write_protect_is_asserted(void)
{
#ifdef CONFIG_WP_ALWAYS
	return true;
#else
	/* Read write protect GPIO */
#ifdef CONFIG_BOARD_PTLRVP_MCHP
#ifdef CONFIG_CROS_SYSTEM_XEC
	sync_wp_assert_status();
	return (!gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(mchp_wp)));
#endif
#else
	return gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_wp));
#endif
#endif
}

/**
 * Enable interrupt for WP pin. The interrupt itself has to be defined in a node
 * with compatible = "cros-ec,gpio-interrupts" and pointed by the alias int_wp.
 *
 * @return 0 if success
 */
static inline int write_protect_enable_interrupt(void)
{
#if DT_NODE_EXISTS(DT_ALIAS(int_wp))
	return gpio_enable_dt_interrupt(GPIO_INT_FROM_NODE(DT_ALIAS(int_wp)));
#else
	return -1;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_ZEPHYR_WRITE_PROTECT_H */
