/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_GPIO_MAP_H
#define __ZEPHYR_GPIO_MAP_H

#include <devicetree.h>
#include <gpio_signal.h>

#define POWER_SIGNAL_INT(gpio, edge)                                      \
	GPIO_INT(gpio, edge, power_signal_interrupt)
/*
 * Set EC_CROS_GPIO_INTERRUPTS to a space-separated list of GPIO_INT items.
 *
 * Each GPIO_INT requires three parameters:
 *   gpio_signal - The enum gpio_signal for the interrupt gpio
 *   interrupt_flags - The interrupt-related flags (e.g. GPIO_INT_EDGE_BOTH)
 *   handler - The platform/ec interrupt handler.
 *
 * Ensure that this files includes all necessary headers to declare all
 * referenced handler functions.
 *
 * For example, one could use the follow definition:
 * #define EC_CROS_GPIO_INTERRUPTS \
 *   GPIO_INT(NAMED_GPIO(h1_ec_pwr_btn_odl), GPIO_INT_EDGE_BOTH, button_print)
 */

#define EC_CROS_GPIO_INTERRUPTS                                           \
	POWER_SIGNAL_INT(GPIO_PCH_SLP_S0_L, GPIO_INT_EDGE_BOTH)           \
	GPIO_INT(GPIO_PCH_SLP_S3_L, GPIO_INT_EDGE_BOTH,                   \
		baseboard_all_sys_pgood_interrupt)                        \
	POWER_SIGNAL_INT(GPIO_PCH_SLP_S4_L, GPIO_INT_EDGE_BOTH)           \
	POWER_SIGNAL_INT(GPIO_SLP_SUS_L, GPIO_INT_EDGE_BOTH)              \
	POWER_SIGNAL_INT(GPIO_PG_EC_RSMRST_ODL, GPIO_INT_EDGE_BOTH)       \
	GPIO_INT(GPIO_PG_VCCIO_EXT_OD, GPIO_INT_EDGE_BOTH,                \
		baseboard_all_sys_pgood_interrupt)                        \
	GPIO_INT(GPIO_LID_OPEN, GPIO_INT_EDGE_BOTH, lid_interrupt)        \
	POWER_SIGNAL_INT(GPIO_PP5000_A_PG_OD, GPIO_INT_EDGE_BOTH)         \
	GPIO_INT(GPIO_PG_DRAM_OD, GPIO_INT_EDGE_BOTH,                     \
		baseboard_all_sys_pgood_interrupt)                        \
	POWER_SIGNAL_INT(GPIO_PG_PP1050_ST_S_OD, GPIO_INT_EDGE_BOTH)      \
	GPIO_INT(GPIO_POWER_BUTTON_L, GPIO_INT_EDGE_BOTH,                 \
		power_button_interrupt)                                   \
	GPIO_INT(GPIO_WP_L, GPIO_INT_EDGE_BOTH, switch_interrupt)
#endif /* __ZEPHYR_GPIO_MAP_H */
