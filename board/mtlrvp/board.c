/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel MTL-P-RVP board-specific configuration */

#include "console.h"
#include "registers.h"

static void fake_interrupt(enum gpio_signal signal)
{
}

#include "gpio_list.h"
/******************************************************************************/
