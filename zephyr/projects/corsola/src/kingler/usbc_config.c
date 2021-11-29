/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kingler board-specific USB-C configuration */

#include "charger.h"
#include "console.h"
#include "driver/bc12/pi3usb9201_public.h"
#include "driver/charger/isl923x_public.h"
#include "driver/ppc/nx20p348x.h"
#include "driver/ppc/rt1718s.h"
#include "driver/tcpm/anx7447.h"
#include "driver/tcpm/rt1718s.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"

#include "baseboard_usbc_config.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {

	[USBC_PORT_C0] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_USB_C0,
			.addr_flags = AN7447_TCPC0_I2C_ADDR_FLAGS,
		},
		.drv = &anx7447_tcpm_drv,
		/* Alert is active-low, push-pull */
		.flags = 0,
	},
	[USBC_PORT_C1] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_USB_C1,
			.addr_flags = RT1718S_I2C_ADDR_FLAGS,
		},
		.drv = &rt1718s_tcpm_drv,
	}
};

#if 0
static int kingler_c1_ppc_init(int port)
{
}

static int kingler_c1_ppc_is_sourcing_vbus(int port)
{
	return 0;
}
/*
 * Kingler C1 PPC driver.
 * There are two PPC at C!: CC/SBU protected by RT1718S, and VBUS protected by
 * NX20P348X
 */
const struct ppc_drv kingler_c1_ppc_drv {
	.init = &kingler_c1_ppc_init,
	.is_sourcing_vbus = &kingler_c1_ppc_is_sourcing_vbus,
};
#endif

struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USBC_PORT_C0] = {
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = NX20P3483_ADDR2_FLAGS,
		.drv = &nx20p348x_drv
	},
	[USBC_PORT_C1] = {
		.i2c_port = I2C_PORT_USB_C1,
		.i2c_addr_flags = RT1718S_I2C_ADDR_FLAGS,
		.drv = &rt1718s_ppc_drv,
	}
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_CHARGER,
		.i2c_addr_flags = ISL923X_ADDR_FLAGS,
		.drv = &isl923x_drv,
	}
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {};

struct bc12_config bc12_ports[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USBC_PORT_C0] = {
		.drv = &pi3usb9201_drv,
	},
	[USBC_PORT_C1] = {
		.drv = &rt1718s_bc12_drv,
	}
};

const struct pi3usb9201_config_t
		pi3usb9201_bc12_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USBC_PORT_C0] = {
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
	},
	[USBC_PORT_C1] = { /* unused */ }
};

void board_reset_pd_mcu(void)
{
}

/* Used by Vbus discharge common code with CONFIG_USB_PD_DISCHARGE */
int board_vbus_source_enabled(int port)
{
	return tcpm_get_src_ctrl(port);
}

/* Used by USB charger task with CONFIG_USB_PD_5V_EN_CUSTOM */
int board_is_sourcing_vbus(int port)
{
	return board_vbus_source_enabled(port);
}

int board_set_active_charge_port(int port)
{
	return CHARGE_PORT_NONE;
}

void pd_power_supply_reset(int port)
{
}

uint16_t tcpc_get_alert_status(void)
{
	return 0;
}

int pd_set_power_supply_ready(int port)
{
	return EC_SUCCESS;
}

int pd_check_vconn_swap(int port)
{
	return EC_SUCCESS;
}

void ppc_interrupt(enum gpio_signal signal)
{
}
