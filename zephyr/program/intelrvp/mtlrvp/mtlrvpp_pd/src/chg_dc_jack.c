/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_state.h"
#include "gpio.h"
#include "hooks.h"
#include "intelrvp.h"
#include "tcpm/tcpci.h"

void board_dc_jack_interrupt(enum gpio_signal signal)
{
}
