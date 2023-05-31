/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rex board-specific USB-C mux configuration */

#include "console.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "hooks.h"
#include "ioexpander.h"
#include "usb_mux.h"
#include "usb_mux_config.h"
#include "usbc/ppc.h"
#include "usbc/tcpci.h"
#include "usbc/usb_muxes.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_ZTEST

#undef USB_MUX_ENABLE_ALTERNATIVE
#define USB_MUX_ENABLE_ALTERNATIVE(x)

#undef TCPC_ENABLE_ALTERNATE_BY_NODELABEL
#define TCPC_ENABLE_ALTERNATE_BY_NODELABEL(x, y)

#undef PPC_ENABLE_ALTERNATE_BY_NODELABEL
#define PPC_ENABLE_ALTERNATE_BY_NODELABEL(x, y)

#endif /* CONFIG_ZTEST */

LOG_MODULE_DECLARE(ovis, CONFIG_OVIS_LOG_LEVEL);

uint32_t usb_db_type;

static void setup_usb_db(void)
{

}
DECLARE_HOOK(HOOK_INIT, setup_usb_db, HOOK_PRIO_POST_I2C);
