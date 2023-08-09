/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "driver/retimer/bb_retimer_public.h"
#include "driver/tcpm/ccgxxf.h"
#include "driver/tcpm/nct38xx.h"
#include "driver/tcpm/tcpci.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "i2c.h"
#include "intel_rvp_board_id.h"
#include "intelrvp.h"
#include "ioexpander.h"
#include "isl9241.h"
#include "keyboard_raw.h"
#include "power/meteorlake.h"
#include "sn5s330.h"
#include "system.h"
#include "task.h"
#include "tusb1064.h"
#include "usb_mux.h"
#include "usbc/usb_muxes.h"
#include "usbc_ppc.h"
#include "util.h"
#include "usb_config.h"
#include <zephyr/drivers/espi.h>

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ##args)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ##args)

/* USB-C Configuration Start */

#define MTLP_DDR5_RVP_SKU_BOARD_ID 0x01
#define MTLP_LP5_RVP_SKU_BOARD_ID 0x02
#define MTL_RVP_BOARD_ID(id) ((id)&0x3F)

/* USB-C ports */
enum usbc_port {
	USBC_PORT_C0 = 0,
	USBC_PORT_C1,
#if defined(HAS_TASK_PD_C2)
	USBC_PORT_C2,
	USBC_PORT_C3,
#endif
	USBC_PORT_COUNT
};
BUILD_ASSERT(USBC_PORT_COUNT == CONFIG_USB_PD_PORT_MAX_COUNT);

__override bool board_is_tbt_usb4_port(int port)
{
	bool tbt_usb4 = true;

	switch (MTL_RVP_BOARD_ID(board_get_version())) {
	case MTLP_LP5_RVP_SKU_BOARD_ID:
		/* No retimer on port 0; and port 1 is not available */
		if ((port == USBC_PORT_C0) || (port == USBC_PORT_C1))
			tbt_usb4 = false;
		break;
	default:
		break;
	}
	return tbt_usb4;
}

__override enum tbt_compat_cable_speed board_get_max_tbt_speed(int port)
{
	enum tbt_compat_cable_speed max_speed = TBT_SS_TBT_GEN3;

	switch (MTL_RVP_BOARD_ID(board_get_version())) {
	case MTLP_LP5_RVP_SKU_BOARD_ID:
		if (port == USBC_PORT_C2)
			max_speed = TBT_SS_U32_GEN1_GEN2;
		break;
	default:
		break;
	}

	return max_speed;
}
