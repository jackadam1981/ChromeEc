/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SHIM_INCLUDE_GPIO_GPIO_INT_H_
#define ZEPHYR_SHIM_INCLUDE_GPIO_GPIO_INT_H_

#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

/*
 * Zephyr based interrupt handling.
 * Uses device tree to configure the interrupt handling e.g
 *
 *	int_power_button: power_button {
 *		gpio = <&gpio_gsc_ec_pwr_btn_odl>;
 *		flags = <GPIO_INT_EDGE_BOTH>;
 *		handler = "power_button_interrupt";
 *	};
 *
 *	int_wp_l: wp_l {
 *		gpio = <&gpio_ec_wp_odl>;
 *		flags = <GPIO_INT_EDGE_BOTH>;
 *		handler = "wp_interrupt";
 *	};
 *
 * The interrupt can be enabled either using the DTS label:
 *
 *	gpio_interrupt_enable(GPIO_INTERRUPT(DT_NODELABEL(int_wp_l)));
 */

/*
 * Creates an internal name for the interrupt config block.
 */
#define GPIO_NODE_TO_INTERRUPT(id) DT_CAT(gpio_interrupt_, id)

/*
 * Maps nodelabel of interrupt node to internal configuration block.
 */
#define GPIO_INTERRUPT(lbl)	(&GPIO_NODE_TO_INTERRUPT(DT_NODELABEL(lbl)))

/*
 * Forward reference to avoiding exposing internal structure
 * defined in gpio_int.c
 */
struct gpio_int_config;
/*
 * Enable the interrupt.
 *
 * Interrupts are not automatically enabled, so
 * each interrupt will need a call to activate the interrupt e.g
 *
 *   ... // set up device
 *   gpio_interrupt_enable(GPIO_INTERRUPT(my_interrupt_node));
 */
void gpio_interrupt_enable(struct gpio_int_config *zc);

/*
 * Disable the interrupt.
 */
void gpio_interrupt_disable(struct gpio_int_config *zc);

/*
 * Declare interrupt configuration data structures.
 */
#define GPIO_INT_DECLARE(id)	\
extern struct gpio_int_config GPIO_NODE_TO_INTERRUPT(id);

#if DT_NODE_EXISTS(DT_PATH(gpio_interrupts))
DT_FOREACH_CHILD(DT_PATH(gpio_interrupts), GPIO_INT_DECLARE)
#endif

#undef GPIO_INT_DECLARE
#undef GPIO_INT_DECLARE_NODE

#endif /* ZEPHYR_SHIM_INCLUDE_GPIO_GPIO_INT_H_ */
