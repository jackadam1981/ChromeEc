/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio_signal.h"

#include <zephyr/fff.h>

FAKE_VOID_FUNC(lid_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(c0_bc12_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(ccd_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(x_ec_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(xhci_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(lis2dw12_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(spi_event, enum gpio_signal);
FAKE_VOID_FUNC(switch_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(icm42607_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(gmr_tablet_switch_isr, enum gpio_signal);
FAKE_VOID_FUNC(button_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(power_button_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(pd_power_supply_reset, int);
FAKE_VOID_FUNC(tcpc_dump_std_registers, int);

FAKE_VALUE_FUNC(bool, in_interrupt_context);
FAKE_VALUE_FUNC(int, power_button_is_pressed);
FAKE_VALUE_FUNC(int, pd_set_power_supply_ready, int);
FAKE_VALUE_FUNC(int, pd_check_vconn_swap, int);
