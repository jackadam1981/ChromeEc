/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_state_v2.h"
#include "console.h"
#include "hooks.h"
#include "it83xx_pd.h"
#include "max14637.h"
#include "tcpci.h"
#include "system.h"

#ifdef CONFIG_USB_PD_RETIMER_INTEL_BB
#include "bb_retimer.h"
#endif

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/* USB-C TPCP Configuration */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
#ifdef CHIP_FAMILY_IT83XX
	[TYPE_C_PORT_0] = {
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it83xx_tcpm_drv,
	},
#ifdef HAS_TASK_PD_C1
	[TYPE_C_PORT_1] = {
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it83xx_tcpm_drv,
	},
#endif /* HAS_TASK_PD_C1 */
#endif /* CHIP_FAMILY_IT83XX */
};
BUILD_ASSERT(ARRAY_SIZE(tcpc_config) == CONFIG_USB_PD_PORT_COUNT);

/* BC1.2 chip Configuration */
#ifdef CONFIG_BC12_DETECT_MAX14637
const struct max14637_config_t max14637_config[CONFIG_USB_PD_PORT_COUNT] = {
	[TYPE_C_PORT_0] = {
		.chip_enable_pin = GPIO_USB_C0_BC12_VBUS_ON_ODL,
		.chg_det_pin = GPIO_USB_C0_BC12_CHG_DET_L,
		.flags = MAX14637_FLAGS_CHG_DET_ACTIVE_LOW,
	},
#ifdef HAS_TASK_PD_C1
	[TYPE_C_PORT_1] = {
		.chip_enable_pin = GPIO_USB_C1_BC12_VBUS_ON_ODL,
		.chg_det_pin = GPIO_USB_C1_BC12_CHG_DET_L,
		.flags = MAX14637_FLAGS_CHG_DET_ACTIVE_LOW,
	},
#endif /* HAS_TASK_PD_C1 */
};
BUILD_ASSERT(ARRAY_SIZE(max14637_config) == CONFIG_USB_PD_PORT_COUNT);
#endif /* CONFIG_BC12_DETECT_MAX14637 */

#ifdef CONFIG_USB_MUX_VIRTUAL
/* USB muxes Configuration */
struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	[TYPE_C_PORT_0] = {
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
#ifdef HAS_TASK_PD_C1
	[TYPE_C_PORT_1] = {
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
#endif /* HAS_TASK_PD_C1 */
};
BUILD_ASSERT(ARRAY_SIZE(usb_muxes) == CONFIG_USB_PD_PORT_COUNT);
#endif /* CONFIG_USB_MUX_VIRTUAL */

#ifdef CONFIG_USB_PD_RETIMER_INTEL_BB
/* USB Retimers configuration */
struct bb_retimer bb_retimers[CONFIG_USB_PD_PORT_COUNT] = {
	[TYPE_C_PORT_0] = {
		.i2c_port = I2C_PORT0_BB_RETIMER,
		.i2c_addr = I2C_PORT0_BB_RETIMER_ADDR,
		.usb_ls_en_gpio = GPIO_USB_C0_LS_EN,
		.retimer_rst_gpio = GPIO_USB_C0_RETIMER_RST,
		.force_power_gpio = GPIO_USB_C0_RETIMER_FORCE_PWR,
	},
#ifdef HAS_TASK_PD_C1
	[TYPE_C_PORT_1] = {
		.i2c_port = I2C_PORT1_BB_RETIMER,
		.i2c_addr = I2C_PORT1_BB_RETIMER_ADDR,
		.usb_ls_en_gpio = GPIO_USB_C1_LS_EN,
		.retimer_rst_gpio = GPIO_USB_C1_RETIMER_RST,
		.force_power_gpio = GPIO_USB_C1_RETIMER_FORCE_PWR,
	},
#endif /* HAS_TASK_PD_C1 */
};
BUILD_ASSERT(ARRAY_SIZE(bb_retimers) == CONFIG_USB_PD_PORT_COUNT);
#endif /* CONFIG_USB_PD_RETIMER_INTEL_BB */

#ifdef CONFIG_USB_PD_VBUS_DETECT_GPIO
void vbus0_evt(enum gpio_signal signal)
{
#ifdef HAS_TASK_USB_CHG_P0
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_VBUS, 0);
#endif
	task_wake(TASK_ID_PD_C0);
}

#ifdef HAS_TASK_PD_C1
void vbus1_evt(enum gpio_signal signal)
{
#ifdef HAS_TASK_USB_CHG_P1
	task_set_event(TASK_ID_USB_CHG_P1, USB_CHG_EVENT_VBUS, 0);
#endif
	task_wake(TASK_ID_PD_C1);
}
#endif /* HAS_TASK_PD_C1 */
#endif /* CONFIG_USB_PD_VBUS_DETECT_GPIO */

static int board_charger_port_is_sourcing_vbus(int port)
{
	int src_en;

	/* DC Jack can't source VBUS */
	if (port == DC_JACK_PORT_0 || port == CHARGE_PORT_NONE)
		return 0;

	src_en = gpio_get_level(tcpc_gpios[port].src.pin);

	return tcpc_gpios[port].src.pin_pol ? src_en : !src_en;
}

void board_charging_enable(int port, int enable)
{
	gpio_set_level(tcpc_gpios[port].snk.pin,
		tcpc_gpios[port].snk.pin_pol ? enable : !enable);

}

void board_vbus_enable(int port, int enable)
{
	gpio_set_level(tcpc_gpios[port].src.pin,
		tcpc_gpios[port].src.pin_pol ? enable : !enable);
}

int pd_snk_is_vbus_provided(int port)
{
	int vbus_intr;

	if (port == DC_JACK_PORT_0)
		return 1;

	vbus_intr = gpio_get_level(tcpc_gpios[port].vbus.pin);

	return tcpc_gpios[port].vbus.pin_pol ? vbus_intr : !vbus_intr;
}

void tcpc_alert_event(enum gpio_signal signal)
{
#ifdef HAS_TASK_PDCMD
	/* Exchange status with TCPCs */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
#endif
}

void board_tcpc_init(void)
{
	int i;

	/* Only reset TCPC if not sysjump */
	if (!system_jumped_to_this_image())
		board_reset_pd_mcu();

	/* Enable TCPCx interrupt */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		gpio_enable_interrupt(tcpc_gpios[i].vbus.pin);
}
DECLARE_HOOK(HOOK_INIT, board_tcpc_init, HOOK_PRIO_INIT_I2C + 1);

int board_tcpc_post_init(int port)
{
	return 0;
}

/* Reset PD MCU */
void board_reset_pd_mcu(void)
{
	/* Not applicable for ITE */
}

static inline int board_dc_jack_present(void)
{
	return gpio_get_level(GPIO_DC_JACK_PRESENT);
}

static void board_dc_jack_handle(void)
{
	struct charge_port_info charge_dc_jack;

	/* System is booted from DC Jack */
	if (board_dc_jack_present()) {
		charge_dc_jack.current = (PD_MAX_POWER_MW * 1000) /
					DC_JACK_MAX_VOLTAGE_MV;
		charge_dc_jack.voltage = DC_JACK_MAX_VOLTAGE_MV;
	} else {
		charge_dc_jack.current = 0;
		charge_dc_jack.voltage = USB_CHARGER_VOLTAGE_MV;
	}

	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				DC_JACK_PORT_0, &charge_dc_jack);
}
DECLARE_HOOK(HOOK_AC_CHANGE, board_dc_jack_handle, HOOK_PRIO_FIRST);

static void board_charge_init(void)
{
	int port, supplier;
	struct charge_port_info charge_init = {
		.current = 0,
		.voltage = USB_CHARGER_VOLTAGE_MV,
	};

	/* Initialize all charge suppliers to seed the charge manager */
	for (port = 0; port < CHARGE_PORT_COUNT; port++) {
		for (supplier = 0; supplier < CHARGE_SUPPLIER_COUNT; supplier++)
			charge_manager_update_charge(supplier, port,
				&charge_init);
	}

	board_dc_jack_handle();
}
DECLARE_HOOK(HOOK_INIT, board_charge_init, HOOK_PRIO_DEFAULT);

int board_set_active_charge_port(int port)
{
	int i;
	/* charge port is a realy physical port */
	int is_real_port = (port >= 0 &&
			port < CHARGE_PORT_COUNT);
	/* check if we are source vbus on that port */
	int source = board_charger_port_is_sourcing_vbus(port);

	if (is_real_port && source) {
		CPRINTS("Skip enable p%d", port);
		return EC_ERROR_INVAL;
	}

	/*
	 * Do not enable Type-C port if the DC Jack is present.
	 * When the Type-C is active port, hardware circuit will
	 * block DC jack from enabling +VADP_OUT.
	 */
	if (port != DC_JACK_PORT_0 && board_dc_jack_present()) {
		CPRINTS("DC Jack present, Skip enable p%d", port);
		return EC_ERROR_INVAL;
	}

	/* Make sure non-charging ports are disabled */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++) {
		if (i == port)
			continue;

		board_charging_enable(i, 0);
	}

	/* Enable charging port */
	if (port != DC_JACK_PORT_0 && port != CHARGE_PORT_NONE)
		board_charging_enable(port, 1);

	CPRINTS("New chg p%d", port);

	return EC_SUCCESS;
}

uint16_t tcpc_get_alert_status(void)
{
	int port;
	uint16_t status = 0;

	for (port = 0; port < CONFIG_USB_PD_PORT_COUNT; port++) {
#ifdef CHIP_FAMILY_IT83XX
		/*
		 * Since C0/C1 TCPC are embedded within EC, we don't need the
		 * PDCMD tasks. The (embedded) TCPC status since chip driver
		 * code handles its own interrupts and forward the correct
		 * events to the PD_C0 task. See it83xx/intc.c
		 */
		if (tcpc_config[port].drv == &it83xx_tcpm_drv)
			continue;
#endif
		if (pd_snk_is_vbus_provided(port))
			status |= (PD_STATUS_TCPC_ALERT_0 << port);
	}

	return status;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_set_input_current_limit(MAX(charge_ma,
				CONFIG_CHARGER_INPUT_CURRENT), charge_mv);
}
