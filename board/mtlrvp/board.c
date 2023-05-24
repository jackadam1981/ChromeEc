/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel MTL-P-RVP board-specific configuration */

#include "console.h"
#include "gpio.h"
#include "lid_switch.h"
#include "power.h"
#include "power/meteorlake.h"
#include "power_button.h"
#include "registers.h"
#include "usb_mux.h"
#include "ec_commands.h"
#include "ioexpander.h"
#include "mtl_pd.h"
#include "driver/ioexpander/it8801.h"

enum mtlrvp_typec_ports {
	TYPE_C_PORT_0,
	TYPE_C_PORT_1,
	TYPE_C_PORT_2,
	TYPE_C_PORT_3,
	TYPE_C_PORT_COUNT
};

enum mtlrvp_i2c {
#ifdef CONFIG_IO_EXPANDER
	I2C_CHAN_IOEX,
#endif
	I2C_CHAN_TYPEC_AIC1,
	I2C_CHAN_SMLINK,
	I2C_CHAN_COUNT
};

const struct i2c_port_t i2c_ports[] = {
#ifdef CONFIG_IO_EXPANDER
	[I2C_CHAN_IOEX] = {
		.name = "ioex",
		.port = I2C_PORT_KB_DISCRETE,
		.kbps = 100,
		.scl = GPIO_SMB_BS_CLK,
		.sda = GPIO_SMB_BS_DATA,
	},
#endif
	[I2C_CHAN_TYPEC_AIC1] = {
		.name = "typec_aic1",
		.port = I2C_PORT_TYPEC_AIC1,
		.kbps = 400,
		.scl = GPIO_USBC_TCPC_I2C_CLK_AIC1,
		.sda = GPIO_USBC_TCPC_I2C_DATA_AIC1,
	},
	[I2C_CHAN_SMLINK] = {
		.name = "smlink",
		.port = I2C_PORT_SMLINK,
		.kbps = 400,
		.scl = GPIO_SML1_CLK_MECC,
		.sda = GPIO_SML1_DATA_MECC,
	},
};
BUILD_ASSERT(ARRAY_SIZE(i2c_ports) == I2C_CHAN_COUNT);
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef CONFIG_IO_EXPANDER
/* IO expander config */
struct ioexpander_config_t ioex_config[] = {
	[IOEX_KBD_GPIO] = {
		.i2c_host_port = I2C_PORT_KB_DISCRETE,
		.i2c_addr_flags = IT8801_I2C_ADDR2,
		.drv = &it8801_ioexpander_drv,
	},
};
BUILD_ASSERT(ARRAY_SIZE(ioex_config) == CONFIG_IO_EXPANDER_PORT_COUNT);
#endif

/* PWROK signal configuration */
/*
 * On MTLRVP, SYS_PWROK_EC is an output controlled by EC and uses ALL_SYS_PWRGD
 * as input.
 */
const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_SYS_PWROK_EC,
		.delay_ms = 3,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	{
		.gpio = GPIO_SYS_PWROK_EC,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);

struct usb_mux_chain usbc0_tcss_usb_mux = {
	.mux =
		&(const struct usb_mux){
			.usb_port = TYPE_C_PORT_0,
			.driver = &virtual_usb_mux_driver,
			.hpd_update = &virtual_hpd_update,
		},
};

struct usb_mux_chain usbc1_tcss_usb_mux = {
	.mux =
		&(const struct usb_mux){
			.usb_port = TYPE_C_PORT_1,
			.driver = &virtual_usb_mux_driver,
			.hpd_update = &virtual_hpd_update,
		},
};

struct usb_mux_chain usbc2_tcss_usb_mux = {
	.mux =
		&(const struct usb_mux){
			.usb_port = TYPE_C_PORT_2,
			.driver = &virtual_usb_mux_driver,
			.hpd_update = &virtual_hpd_update,
		},
};

struct usb_mux_chain usbc3_tcss_usb_mux = {
	.mux =
		&(const struct usb_mux){
			.usb_port = TYPE_C_PORT_3,
			.driver = &virtual_usb_mux_driver,
			.hpd_update = &virtual_hpd_update,
		},
};

struct usb_mux dummy0_usb_mux = {
	.usb_port = TYPE_C_PORT_0,
	.driver = &dummy_mtl_pd,
	.hpd_update = dummy_hpd_update,
	.i2c_port = 0,
	.i2c_addr_flags = 0,
};

struct usb_mux dummy1_usb_mux = {
	.usb_port = TYPE_C_PORT_1,
	.driver = &dummy_mtl_pd,
	.hpd_update = dummy_hpd_update,
	.i2c_port = 0,
	.i2c_addr_flags = 0,
};

struct usb_mux dummy2_usb_mux = {
	.usb_port = TYPE_C_PORT_2,
	.driver = &dummy_mtl_pd,
	.hpd_update = dummy_hpd_update,
	.i2c_port = 0,
	.i2c_addr_flags = 0,
};

struct usb_mux dummy3_usb_mux = {
	.usb_port = TYPE_C_PORT_3,
	.driver = &dummy_mtl_pd,
	.hpd_update = dummy_hpd_update,
	.i2c_port = 0,
	.i2c_addr_flags = 0,
};

const struct usb_mux_chain usb_muxes[] = {
	[TYPE_C_PORT_0] = {
		.mux = &dummy0_usb_mux,
		.next = &usbc0_tcss_usb_mux,
	},
	[TYPE_C_PORT_1] = {
		.mux = &dummy1_usb_mux,
		.next = &usbc1_tcss_usb_mux,
	},
	[TYPE_C_PORT_2] = {
		.mux = &dummy2_usb_mux,
		.next = &usbc2_tcss_usb_mux,
	},
	[TYPE_C_PORT_3] = {
		.mux = &dummy3_usb_mux,
		.next = &usbc3_tcss_usb_mux,
	},
};
BUILD_ASSERT(ARRAY_SIZE(usb_muxes) == CONFIG_USB_PD_PORT_MAX_COUNT);

int extpower_is_present(void)
{
	return gpio_get_level(GPIO_BC_ACOK_EC);
}

__override int board_get_version(void)
{
	return 1;
}

static void ioex_kbd_interrupt(enum gpio_signal signal)
{
	gpio_set_level(GPIO_KBD_INTR, gpio_get_level(signal));
}

static void fake_interrupt(enum gpio_signal signal)
{
}

__override uint8_t board_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}

#include "gpio_list.h"
/******************************************************************************/
