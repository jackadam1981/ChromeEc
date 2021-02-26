/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_GPIO_MAP_H
#define __ZEPHYR_GPIO_MAP_H

#include <devicetree.h>
#include <gpio_signal.h>

#define GPIO_PCH_SMI_L		NAMED_GPIO(pch_smi_l)
#define GPIO_PCH_SCI_L		NAMED_GPIO(pch_sci_l)
#define GPIO_GATE_A20_H		NAMED_GPIO(gate_a20_h)
#define GPIO_SYS_RESET_L	NAMED_GPIO(sys_reset_l)
#define GPIO_LPC_CLKRUN_L	NAMED_GPIO(lpc_clkrun_l)
#define GPIO_PCH_WAKE_L		NAMED_GPIO(pch_wake_l)

#define GPIO_I2C_A_SCL		NAMED_GPIO(i2c_a_scl)
#define GPIO_I2C_A_SDA		NAMED_GPIO(i2c_a_sda)
#define GPIO_I2C_B_SCL		NAMED_GPIO(i2c_b_scl)
#define GPIO_I2C_B_SDA		NAMED_GPIO(i2c_b_sda)
#define GPIO_I2C_C_SCL		NAMED_GPIO(i2c_c_scl)
#define GPIO_I2C_C_SDA		NAMED_GPIO(i2c_c_sda)
#define GPIO_I2C_E_SCL		NAMED_GPIO(i2c_e_scl)
#define GPIO_I2C_E_SDA		NAMED_GPIO(i2c_e_sda)

#define GPIO_UART2_SIN1		NAMED_GPIO(uart2_sin1)
#define GPIO_UART2_SOUT1	NAMED_GPIO(uart2_sout1)

#define GPIO_POWER_BUTTON_L	NAMED_GPIO(power_button_l)
#define GPIO_PCH_PLTRST_L	NAMED_GPIO(pch_pltrst_l)
#define GPIO_LID_OPEN		NAMED_GPIO(lid_open)
#define GPIO_WP_L		NAMED_GPIO(wp_l)
#define GPIO_UART1_RX		NAMED_GPIO(uart1_rx)

/* KSO/KSI pins can be used as GPIO input. */
#define GPIO_BOARD_VERSION1	NAMED_GPIO(board_version1)
#define GPIO_BOARD_VERSION2	NAMED_GPIO(board_version2)
#define GPIO_BOARD_VERSION3	NAMED_GPIO(board_version3)

//UNIMPLEMENTED(ENTERING_RW)
#define GPIO_ENTERING_RW	NAMED_GPIO(entering_rw)

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
	GPIO_INT(GPIO_POWER_BUTTON_L, GPIO_INT_EDGE_BOTH, power_button_interrupt) \
	GPIO_INT(GPIO_LID_OPEN, GPIO_INT_EDGE_BOTH, lid_interrupt)             \
	GPIO_INT(GPIO_WP_L, GPIO_INT_EDGE_BOTH, switch_interrupt)


#endif /* __ZEPHYR_GPIO_MAP_H */
