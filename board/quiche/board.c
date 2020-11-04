/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Quiche board-specific configuration */

#include "common.h"
#include "driver/ppc/sn5s330.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/tcpm/stm32gx.h"
#include "driver/tcpm/tcpci.h"
#include "driver/usb_mux/ps8822.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "usb_pd_dp_ufp.h"
#include "usb_descriptor.h"
#include "usb_pe_sm.h"
#include "usb_prl_sm.h"
#include "usb_tc_sm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#define QUICHE_PD_DEBUG_LVL 1

#ifdef SECTION_IS_RW
#define CROS_EC_SECTION "RW"
#else
#define CROS_EC_SECTION "RO"
#endif

#ifdef SECTION_IS_RW
static int pd_dual_role_init[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	PD_DRP_TOGGLE_ON,
	PD_DRP_TOGGLE_ON,
};

static void ppc_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_HOST_USBC_PPC_INT_ODL:
		sn5s330_interrupt(USB_PD_PORT_HOST);
		break;
	case GPIO_USBC_DP_PPC_INT_ODL:
		sn5s330_interrupt(USB_PD_PORT_DP);
		break;

	default:
		break;
	}
}

static void tcpc_alert_event(enum gpio_signal s)
{
	int port = -1;

	switch (s) {
	case GPIO_USBC_DP_MUX_ALERT_ODL:
		board_debug_gpio(TRIGGER_2, 1);
		port = USB_PD_PORT_DP;
		break;
	default:
		return;
	}
	schedule_deferred_pd_interrupt(port);
}

void hpd_interrupt(enum gpio_signal signal)
{
	usb_pd_hpd_edge_event(signal);
}

void board_uf_manage_vbus(void)
{
	int level = gpio_get_level(GPIO_USBC_UF_MUX_VBUS_EN);

	ppc_vbus_source_enable(USB_PD_PORT_UF, level);
	CPRINTS("C2: Vbus %s", level ? "on" : "off");
}
DECLARE_DEFERRED(board_uf_manage_vbus);

static void board_uf_manage_vbus_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&board_uf_manage_vbus_data, 500);
}
#endif

#include "gpio_list.h" /* Must come after other header files. */

/*
 * Table GPIO signals control both power rails and reset lines to various chips
 * on the board. The order the signals are changed and the delay between GPIO
 * signals is driven by USB/MST hub power sequencing requirements.
 */
const struct power_seq board_power_seq[] = {
	{GPIO_EN_AC_JACK,               1, 20},
	{GPIO_EN_PP5000_A,              1, 31},
	{GPIO_EN_PP3300_B,              1, 100},
	{GPIO_EN_BB,                    1, 30},
	{GPIO_EN_PP1100_A,              1, 30},
	{GPIO_EN_PP1050_A,              1, 30},
	{GPIO_EN_PP1200_A,              1, 20},
	{GPIO_EN_PP5000_C,              1, 20},
	{GPIO_EN_PP5000_HSPORT,         1, 31},
	{GPIO_EN_DP_SINK,               1, 80},
	{GPIO_MST_LP_CTL_L,             1, 41},
	{GPIO_MST_RST_L,                1, 20},
	{GPIO_EC_HUB2_RESET_L,          1, 41},
	{GPIO_EC_HUB3_RESET_L,          1, 33},
	{GPIO_DP_SINK_RESET,            1, 100},
	{GPIO_USBC_DP_PD_RST_L,         1, 100},
	{GPIO_USBC_UF_RESET_L,          1, 33},
	{GPIO_DEMUX_DUAL_DP_PD_N,       1, 100},
	{GPIO_DEMUX_DUAL_DP_RESET_N,    1, 100},
	{GPIO_DEMUX_DP_HDMI_PD_N,       1, 10},
	{GPIO_DEMUX_DUAL_DP_MODE,       1, 10},
	{GPIO_DEMUX_DP_HDMI_MODE,       1, 5},
};

const size_t board_power_seq_count = ARRAY_SIZE(board_power_seq);

/*
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("Quiche"),
	[USB_STR_SERIALNO]     = 0,
	[USB_STR_VERSION]      =
			USB_STRING_DESC(CROS_EC_SECTION ":" CROS_EC_VERSION32),
	[USB_STR_UPDATE_NAME]  = USB_STRING_DESC("Firmware update"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

#ifdef SECTION_IS_RW
static void board_hpd_update(const struct usb_mux *me, int hpd_lvl, int hpd_irq)
{

}

/* TCPCs */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USB_PD_PORT_HOST] = {
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		.drv = &stm32gx_tcpm_drv,
	},
	[USB_PD_PORT_DP] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_I2C1,
			.addr_flags = PS8751_I2C_ADDR2_FLAGS,
		},
		.drv = &ps8xxx_tcpm_drv,
	},
};

const struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USB_PD_PORT_HOST] = {
		.usb_port = USB_PD_PORT_HOST,
		.i2c_port = I2C_PORT_I2C1,
		.i2c_addr_flags = PS8822_I2C_ADDR3_FLAG,
		.driver = &ps8822_usb_mux_driver,
		.hpd_update = &board_hpd_update,
	},
	[USB_PD_PORT_DP] = {
		.usb_port = USB_PD_PORT_DP,
		.i2c_port = I2C_PORT_I2C1,
		.i2c_addr_flags = PS8751_I2C_ADDR2_FLAGS,
		.driver = &tcpci_tcpm_usb_mux_driver,
		.hpd_update = &ps8xxx_tcpc_update_hpd_status,
	},
};

/* USB-C PPC Configuration */
struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USB_PD_PORT_HOST] = {
		.i2c_port = I2C_PORT_I2C1,
		.i2c_addr_flags = SN5S330_ADDR0_FLAGS,
		.drv = &sn5s330_drv
	},
	[USB_PD_PORT_DP] = {
		.i2c_port = I2C_PORT_I2C1,
		.i2c_addr_flags = SN5S330_ADDR2_FLAGS,
		.drv = &sn5s330_drv
	},
	[USB_PD_PORT_UF] = {
		.i2c_port = I2C_PORT_I2C3,
		.i2c_addr_flags = SN5S330_ADDR1_FLAGS,
		.drv = &sn5s330_drv
	},
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

const struct hpd_to_pd_config_t hpd_config = {
	.port = USB_PD_PORT_HOST,
	.signal = GPIO_DDI_MST_IN_HPD,
};

/* Power Delivery and charging functions */
void board_tcpc_init(void)
{
	/* Enable PPC interrupts. */
	gpio_enable_interrupt(GPIO_HOST_USBC_PPC_INT_ODL);
	gpio_enable_interrupt(GPIO_USBC_DP_PPC_INT_ODL);
	/* Enable HPD interrupt */
	gpio_enable_interrupt(GPIO_DDI_MST_IN_HPD);
	/* Enable TCPC interrupts. */
	gpio_enable_interrupt(GPIO_USBC_DP_MUX_ALERT_ODL);
	/* Enable VBUS control interrupt for C2 */
	gpio_enable_interrupt(GPIO_USBC_UF_MUX_VBUS_EN);
}
DECLARE_HOOK(HOOK_INIT, board_tcpc_init, HOOK_PRIO_INIT_I2C + 1);

enum pd_dual_role_states board_tc_get_initial_drp_mode(int port)
{
	return pd_dual_role_init[port];
}

static int board_ppc_disable_dead_battery(void)
{
	int reg = SN5S330_FUNC_SET4;
	int regval;
	int rv;
	int port = USB_PD_PORT_HOST;

	/* Get Func 4 register to read CC_EN bit */
	rv = i2c_read8(ppc_chips[port].i2c_port,
			 ppc_chips[port].i2c_addr_flags,
			 reg,
			 &regval);

	/*
	 * If i2c interface is responsive, then remove dead battery resistors
	 * and connect the CC lines.
	 */
	if (!rv) {
		regval |= SN5S330_CC_EN;
		CPRINTS("ppc: func_set4 = %x", regval);
		rv = i2c_write8(ppc_chips[port].i2c_port,
				ppc_chips[port].i2c_addr_flags,
				reg,
				regval);
	}

	return rv;
}

static void board_ppc_force_detach(void)
{
	int i;

	/*
	 * When not powered, if VBUS is applied, then the PPC will power up in
	 * dead battery mode. This can result in the host attaching in SRC
	 * mode. If there is not event to force a detach, then there is no
	 * USB-PD messaging. To avoid this case, always remove the dead battery
	 * resistors (which connects CC lines) at initialization time. Attempt
	 * this up to 10 times since there may be some delay before the digital
	 * core/i2c interface will respond.
	 */
	for (i = 0; i < 10; i++) {
		if (board_ppc_disable_dead_battery() == EC_SUCCESS)
			break;
	}
}
DECLARE_HOOK(HOOK_INIT, board_ppc_force_detach, HOOK_PRIO_INIT_I2C + 1);
#endif

static void board_init(void)
{
	usb_mux_hpd_update(1, 0, 0);

}

static void board_config_usbc_uf_ppc(void)
{
	int vbus_level;

	/*
	 * This port is not usb-pd capable, but there is a ppc which must be
	 * initialized, and keep the VBUS switch enabled.
	 */
	ppc_init(USB_PD_PORT_UF);
	vbus_level = gpio_get_level(GPIO_USBC_UF_MUX_VBUS_EN);

	CPRINTS("usbc: UF PPC configured. VBUS = %s",
		vbus_level ? "on" : "off");

	/*
	 * Check intial state as there there may not be an edge event after
	 * interrupts are enabled if the port is attached at EC reboot time.
	 */
	ppc_vbus_source_enable(USB_PD_PORT_UF, vbus_level);
}
DECLARE_DEFERRED(board_config_usbc_uf_ppc);

static int command_c2(int argc, char **argv)
{
	int en = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "on")) {
		en = 1;
	} else if (!strcasecmp(argv[1], "off")) {
		en = 0;
	} else {
		return EC_ERROR_PARAM1;
	}
	ppc_vbus_source_enable(USB_PD_PORT_UF, en);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(c2, command_c2,
			"<on|off>",
			"C2 PPC");

__override uint8_t board_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT - 1;
}

#endif /* #ifdef SECTION_IS_RW */

static void board_init(void)
{
#ifdef SECTION_IS_RW
	board_select_drp_mode();
	hook_call_deferred(&board_select_drp_mode_data, 25 * MSEC);
	hook_call_deferred(&board_config_usbc_uf_ppc_data, 10 * MSEC);
#endif
>>>>>>> 6bf775e605 (quiche: Add support for C2 usbc port)
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

#ifdef SECTION_IS_RW
int ppc_get_alert_status(int port)
{
	if (port == USB_PD_PORT_HOST)
		return gpio_get_level(GPIO_HOST_USBC_PPC_INT_ODL) == 0;
	else if (port == USB_PD_PORT_DP)
		return gpio_get_level(GPIO_USBC_DP_PPC_INT_ODL) == 0;

	return 0;
}

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;
	int level;

	if (!gpio_get_level(GPIO_USBC_DP_MUX_ALERT_ODL)) {
		level = !!(tcpc_config[USB_PD_PORT_DP].flags &
			   TCPC_FLAGS_RESET_ACTIVE_HIGH);
		if (gpio_get_level(GPIO_USBC_DP_PD_RST_L) != level)
			status |= PD_STATUS_TCPC_ALERT_1;
	}

	return status;
}

void board_overcurrent_event(int port, int is_overcurrented)
{
	/* TODO(b/174825406): check correct operation for honeybuns */
}
#endif

static void board_debug_gpio_1_pulse(void)
{
	gpio_set_level(GPIO_TRIGGER_1, 0);
}
DECLARE_DEFERRED(board_debug_gpio_1_pulse);

static void board_debug_gpio_2_pulse(void)
{
	gpio_set_level(GPIO_TRIGGER_2, 0);
}
DECLARE_DEFERRED(board_debug_gpio_2_pulse);

void board_debug_gpio(int trigger, int enable, int pulse_usec)
{
	switch (trigger) {
	case TRIGGER_1:
		gpio_set_level(GPIO_TRIGGER_1, enable);
		if (pulse_usec)
			hook_call_deferred(&board_debug_gpio_1_pulse_data,
					   pulse_usec);
		break;
	case TRIGGER_2:
		gpio_set_level(GPIO_TRIGGER_2, enable);
		if (pulse_usec)
			hook_call_deferred(&board_debug_gpio_2_pulse_data,
					   pulse_usec);
		break;
	default:
		CPRINTS("bad debug gpio selection");
		break;
	}
}
