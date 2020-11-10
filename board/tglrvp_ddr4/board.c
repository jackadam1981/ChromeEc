/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ADL-P-RVP-ITE board-specific configuration */

#include "bb_retimer.h"
#include "button.h"
#include "ccgxxf.h"
#include "common.h"
#include "charger.h"
#include "fan.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "power/icelake.h"
#include "isl9241.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "tablet_mode.h"
#include "uart.h"
#include "usb_pd_tbt.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)

/* Mutex for BB retimer shared NVM access */
static struct mutex bb_nvm_mutex;

/******************************************************************************/
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	[I2C_CHAN_FLASH] = {
		.name = "ec_flash",
		.port = IT83XX_I2C_CH_A,
		.kbps = 100,
		.scl = GPIO_EC_I2C_PROG_SCL,
		.sda = GPIO_EC_I2C_PROG_SDA,
	},
	[I2C_CHAN_BATT_CHG] = {
		.name = "batt_chg",
		.port = IT83XX_I2C_CH_B,
		.kbps = 100,
		.scl = GPIO_SMB_BS_CLK,
		.sda = GPIO_SMB_BS_DATA,
	},
	[I2C_CHAN_TYPEC] = {
		.name = "typec",
		.port = IT83XX_I2C_CH_E,
		.kbps = 400,
		.scl = GPIO_RETIMER_CLK_EC,
		.sda = GPIO_RETIMER_DATA_EC,
	},
};
BUILD_ASSERT(ARRAY_SIZE(i2c_ports) == I2C_CHAN_COUNT);
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* TCPC AIC GPIO Configuration */
/* TODO: Add code */
const struct tcpc_aic_gpio_config_t tcpc_aic_gpios[] = {
	[TYPE_C_PORT_0] = {
	},
	[TYPE_C_PORT_1] = {
	},
};
BUILD_ASSERT(ARRAY_SIZE(tcpc_aic_gpios) == CONFIG_USB_PD_PORT_MAX_COUNT);

/* USB-C TCPC Configuration */
/* TODO: Add code */
const struct tcpc_config_t tcpc_config[] = {
	[TYPE_C_PORT_0] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TYPEC,
			.addr_flags = 0,
		},
		.drv = &ccgxxf_tcpm_drv
	},
	[TYPE_C_PORT_1] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TYPEC,
			.addr_flags = 0,
		},
		.drv = &ccgxxf_tcpm_drv
	},
};
BUILD_ASSERT(ARRAY_SIZE(tcpc_config) == CONFIG_USB_PD_PORT_MAX_COUNT);

/* USB-C PPC configuration */
/* TODO: Add code */
struct ppc_config_t ppc_chips[] = {
	[TYPE_C_PORT_0] = {
		.i2c_port = I2C_PORT_TYPEC,
		.i2c_addr_flags = 0,
		.drv = &ccgxxf_ppc_drv,
	},
	[TYPE_C_PORT_1] = {
		.i2c_port = I2C_PORT_TYPEC,
		.i2c_addr_flags = 0,
		.drv = &ccgxxf_ppc_drv,
	},
};
BUILD_ASSERT(ARRAY_SIZE(ppc_chips) == CONFIG_USB_PD_PORT_MAX_COUNT);
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

/* USB-C retimer Configuration */
struct usb_mux usbc0_retimer = {
	.usb_port = TYPE_C_PORT_0,
	.driver = &bb_usb_retimer,
	.i2c_port = I2C_PORT_TYPEC,
	.i2c_addr_flags = I2C_PORT0_BB_RETIMER_ADDR,
};
struct usb_mux usbc1_retimer = {
	.usb_port = TYPE_C_PORT_1,
	.driver = &bb_usb_retimer,
	.i2c_port = I2C_PORT_TYPEC,
	.i2c_addr_flags = I2C_PORT1_BB_RETIMER_ADDR,
};

/* USB muxes Configuration */
const struct usb_mux usb_muxes[] = {
	[TYPE_C_PORT_0] = {
		.usb_port = TYPE_C_PORT_0,
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
		.next_mux = &usbc0_retimer,
	},
	[TYPE_C_PORT_1] = {
		.usb_port = TYPE_C_PORT_1,
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
		.next_mux = &usbc1_retimer,
	},
};
BUILD_ASSERT(ARRAY_SIZE(usb_muxes) == CONFIG_USB_PD_PORT_MAX_COUNT);

/* Each TCPC have corresponding IO expander */

/* Charger Chips */
const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_CHARGER,
		.i2c_addr_flags = ISL9241_ADDR_FLAGS,
		.drv = &isl9241_drv,
	},
};

/* TODO: Add code */
void board_overcurrent_event(int port, int is_overcurrented)
{
}

/* TODO: Add code */
__override void bb_retimer_power_handle(const struct usb_mux *me, int on_off)
{
	/* Handle retimer's power domain.*/
	if (on_off) {
		/*
		 * BB retimer NVM can be shared between multiple ports, hence
		 * lock enabling the retimer until the current retimer request
		 * is complete.
		 */
		mutex_lock(&bb_nvm_mutex);


		/*
		 * Tpw, minimum time from VCC to RESET_N de-assertion is 100us
		 * For boards that don't provide a load switch control, the
		 * retimer_init() function ensures power is up before calling
		 * this function.
		 */
		msleep(1);

		/* Allow 20ms time for the retimer to be initialized. */
		msleep(20);

		mutex_unlock(&bb_nvm_mutex);
	} else {
		msleep(1);
	}
}

/******************************************************************************/
/* PWROK signal configuration */
/*
 * On ADLRVP the ALL_SYS_PWRGD, VCCST_PWRGD, PCH_PWROK, and SYS_PWROK
 * signals are handled by the board. No EC control needed.
 */
const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_assert_list);

/*
 * Returns board information (board id[7:0] and Fab id[15:8]) on success
 * -1 on error.
 */
int board_get_version(void)
{
	int port0, port1;
	int fab_id, board_id, bom_id;

	if (ioexpander_read_intelrvp_version(&port0, &port1))
		return -1;
	/*
	 * Port0: bit 0   - BOM ID(2)
	 *        bit 2:1 - FAB ID(1:0) + 1
	 * Port1: bit 7:6 - BOM ID(1:0)
	 *        bit 5:0 - BOARD ID(5:0)
	 */
	bom_id = ((port1 & 0xC0) >> 6) | ((port0 & 0x01) << 2);
	fab_id = ((port0 & 0x06) >> 1) + 1;
	board_id = port1 & 0x3F;

	CPRINTS("BID:0x%x, FID:0x%x, BOM:0x%x", board_id, fab_id, bom_id);

	return board_id | (fab_id << 8);
}
