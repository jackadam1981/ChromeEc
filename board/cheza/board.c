/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Cheza board-specific configuration */

#include "adc_chip.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "console.h"
#include "extpower.h"
#include "driver/ppc/sn5s330.h"
#include "driver/tcpm/anx74xx.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/tcpm/tcpci.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "lid_switch.h"
#include "pi3usb9281.h"
#include "power_button.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

#define USB_PD_PORT_ANX74XX	0

/* Forward Declaration */
static void tcpc_alert_event(enum gpio_signal signal);
static void vbus_discharge_handler(void);
static void vbus0_evt(enum gpio_signal signal);
static void vbus1_evt(enum gpio_signal signal);
static void usb0_evt(enum gpio_signal signal);
static void usb1_evt(enum gpio_signal signal);
static void anx74xx_cable_det_interrupt(enum gpio_signal signal);
static void ppc_interrupt(enum gpio_signal signal);

#include "gpio_list.h"


/* 8-bit I2C address */
#define PI3USB9281_I2C_ADDR	0x4a
#define DA9313_I2C_ADDR		0xd0
#define CHARGER_I2C_ADDR	0x12

/* GPIO Interrupt Handlers */
static void tcpc_alert_event(enum gpio_signal signal)
{
	if ((signal == GPIO_USB_C0_PD_INT_ODL) &&
	    !gpio_get_level(GPIO_USB_C0_PD_RST_R_L))
		return;
	else if ((signal == GPIO_USB_C1_PD_INT_ODL) &&
		 !gpio_get_level(GPIO_USB_C1_PD_RST_ODL))
		return;

#ifdef HAS_TASK_PDCMD
	/* Exchange status with TCPCs */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
#endif
}

static void vbus_discharge_handler(void)
{
	/*
	 * Set PD discharge whenever VBUS detection is high (i.e. below
	 * threshold).
	 */
	pd_set_vbus_discharge(0,
			gpio_get_level(GPIO_USB_C0_VBUS_DET_L));
	pd_set_vbus_discharge(1,
			gpio_get_level(GPIO_USB_C0_VBUS_DET_L));
}
DECLARE_DEFERRED(vbus_discharge_handler);

static void vbus0_evt(enum gpio_signal signal)
{
	/* VBUS present GPIO is inverted */
	usb_charger_vbus_change(0, !gpio_get_level(signal));
	task_wake(TASK_ID_PD_C0);

	hook_call_deferred(&vbus_discharge_handler_data, 0);
}

static void vbus1_evt(enum gpio_signal signal)
{
	/* VBUS present GPIO is inverted */
	usb_charger_vbus_change(1, !gpio_get_level(signal));
	task_wake(TASK_ID_PD_C1);

	hook_call_deferred(&vbus_discharge_handler_data, 0);
}

static void usb0_evt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12, 0);
}

static void usb1_evt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_USB_CHG_P1, USB_CHG_EVENT_BC12, 0);
}

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
static void anx74xx_cable_det_handler(void)
{
	int cable_det = gpio_get_level(GPIO_USB_C0_CABLE_DET);
	int reset_n = gpio_get_level(GPIO_USB_C0_PD_RST_R_L);

	/*
	 * A cable_det low->high transition was detected. If following the
	 * debounce time, cable_det is high, and reset_n is low, then ANX3429 is
	 * currently in standby mode and needs to be woken up. Set the
	 * TCPC_RESET event which will bring the ANX3429 out of standby
	 * mode. Setting this event is gated on reset_n being low because the
	 * ANX3429 will always set cable_det when transitioning to normal mode
	 * and if in normal mode, then there is no need to trigger a tcpc reset.
	 */
	if (cable_det && !reset_n)
		task_set_event(TASK_ID_PD_C0, PD_EVENT_TCPC_RESET, 0);
}
DECLARE_DEFERRED(anx74xx_cable_det_handler);

static void anx74xx_cable_det_interrupt(enum gpio_signal signal)
{
	/* debounce for 2 msec */
	hook_call_deferred(&anx74xx_cable_det_handler_data, (2 * MSEC));
}
#endif

static void ppc_interrupt(enum gpio_signal signal)
{
	int port = (signal == GPIO_USB_C0_SWCTL_INT_ODL) ? 0 : 1;

	sn5s330_interrupt(port);
}

/* Wake-up pins for hibernate */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_LID_OPEN,
	GPIO_AC_PRESENT,
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

const struct adc_t adc_channels[] = {};

/* I2C port map */
const struct i2c_port_t i2c_ports[] = {
	{"power",   I2C_PORT_POWER,  400, GPIO_I2C0_SCL,  GPIO_I2C0_SDA},
	{"tcpc0",   I2C_PORT_TCPC0,  400, GPIO_I2C1_SCL,  GPIO_I2C1_SDA},
	{"tcpc1",   I2C_PORT_TCPC1,  400, GPIO_I2C2_SCL,  GPIO_I2C2_SDA},
	{"eeprom",  I2C_PORT_EEPROM, 400, GPIO_I2C5_SCL,  GPIO_I2C5_SDA},
	{"sensor",  I2C_PORT_SENSOR, 400, GPIO_I2C7_SCL,  GPIO_I2C7_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Power Path Controller */
const struct ppc_config_t ppc_chips[] = {
	{
		.i2c_port = I2C_PORT_TCPC0,
		.i2c_addr = SN5S330_ADDR0,
		.drv = &sn5s330_drv
	},
	/*
	 * Port 1 uses two power switches instead:
	 *   NX5P3290: to source VBUS
	 *   NX20P5090: to sink VBUS (charge battery)
	 * which are controlled directly by EC GPIOs.
	 */
};
const unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

/* TCPC mux configuration */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	{NPCX_I2C_PORT0_0, 0x50, &anx74xx_tcpm_drv, TCPC_ALERT_ACTIVE_LOW},
	{NPCX_I2C_PORT0_0, 0x16, &ps8xxx_tcpm_drv, TCPC_ALERT_ACTIVE_LOW},
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.port_addr = 0, /* don't care / unused */
		.driver = &anx74xx_tcpm_usb_mux_driver,
		.hpd_update = &anx74xx_tcpc_update_hpd_status,
	},
	{
		.port_addr = 1,
		.driver = &tcpci_tcpm_usb_mux_driver,
		.hpd_update = &ps8xxx_tcpc_update_hpd_status,
	}
};

/* BC1.2 */
struct pi3usb9281_config pi3usb9281_chips[] = {
	{
		.i2c_port = I2C_PORT_TCPC0,
		.mux_lock = NULL,
	},
	{
		.i2c_port = I2C_PORT_TCPC1,
		.mux_lock = NULL,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pi3usb9281_chips) ==
	     CONFIG_BC12_DETECT_PI3USB9281_CHIP_COUNT);

/* Initialize board. */
static void board_init(void)
{
	/*
	 * Hack to enable the internal switch in BC1.2
	 * TODO(waihong): Enable BC1.2 autodetection.
	 */
	i2c_write8(I2C_PORT_TCPC0, PI3USB9281_I2C_ADDR, PI3USB9281_REG_CONTROL,
		   0x1b);  /* Manual switch on port-0 */
	i2c_write8(I2C_PORT_TCPC0, PI3USB9281_I2C_ADDR, PI3USB9281_REG_MANUAL,
		   0x24);  /* Connection of D+ and D- */
	i2c_write8(I2C_PORT_TCPC1, PI3USB9281_I2C_ADDR, PI3USB9281_REG_CONTROL,
		   0x1b);  /* Manual switch on port-1 */
	i2c_write8(I2C_PORT_TCPC1, PI3USB9281_I2C_ADDR, PI3USB9281_REG_MANUAL,
		   0x24);  /* Connection of D+ and D- */

	/*
	 * Disable SwitchCap auto-boot and make EN pin level-trigger
	 * TODO(b/77957956): Remove it after hardware fix.
	 */
	i2c_write8(I2C_PORT_POWER, DA9313_I2C_ADDR, 0x02, 0x34);

	/*
	 * Increase AdapterCurrentLimit{1,2} to max (6080mA)
	 */
	i2c_write16(I2C_PORT_POWER, CHARGER_I2C_ADDR, 0x3B, 0x17c0);
	i2c_write16(I2C_PORT_POWER, CHARGER_I2C_ADDR, 0x3F, 0x17c0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/**
 * Power on (or off) a single TCPC.
 * minimum on/off delays are included.
 *
 * @param port  Port number of TCPC.
 * @param mode  0: power off, 1: power on.
 */
void board_set_tcpc_power_mode(int port, int mode)
{
	if (port != USB_PD_PORT_ANX74XX)
		return;

	if (mode) {
		gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 1);
		msleep(ANX74XX_PWR_H_RST_H_DELAY_MS);
		gpio_set_level(GPIO_USB_C0_PD_RST_R_L, 1);
	} else {
		gpio_set_level(GPIO_USB_C0_PD_RST_R_L, 0);
		msleep(ANX74XX_RST_L_PWR_L_DELAY_MS);
		gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 0);
		msleep(ANX74XX_PWR_L_PWR_H_DELAY_MS);
	}
}

void board_reset_pd_mcu(void)
{
	/* Assert reset */
	gpio_set_level(GPIO_USB_C0_PD_RST_R_L, 0);
	gpio_set_level(GPIO_USB_C1_PD_RST_ODL, 0);

	msleep(MAX(1, ANX74XX_RST_L_PWR_L_DELAY_MS));
	gpio_set_level(GPIO_USB_C1_PD_RST_ODL, 1);
	/* Disable TCPC0 (anx3429) power */
	gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 0);

	msleep(ANX74XX_PWR_L_PWR_H_DELAY_MS);
	board_set_tcpc_power_mode(USB_PD_PORT_ANX74XX, 1);
}

int board_vbus_sink_enable(int port, int enable)
{
	if (port == 0) {
		/* Port 0 is controlled by a PPC SN5S330 */
		return ppc_vbus_sink_enable(port, 0);
	} else if (port == 1) {
		/* Port 1 is controlled by a power switch NX20P5090 */
		gpio_set_level(GPIO_EN_USB_C1_CHARGE_EC_L, !enable);
		return EC_SUCCESS;
	}
	return EC_ERROR_INVAL;
}

static int board_is_sourcing_vbus(int port)
{
	if (port == 0) {
		/* Port 0 is controlled by a PPC SN5S330 */
		return ppc_is_sourcing_vbus(port);
	} else if (port == 1) {
		/* Port 1 is controlled by a power switch NX5P3290 */
		return gpio_get_level(GPIO_EN_USB_C1_5V_OUT);
	}
	return EC_ERROR_INVAL;
}

void board_overcurrent_event(int port)
{
	/* TODO(waihong): Notice AP? */
	CPRINTS("p%d: overcurrent!", port);
}

int board_set_active_charge_port(int port)
{
	int is_real_port = (port >= 0 &&
			    port < CONFIG_USB_PD_PORT_COUNT);
	int i;
	int rv;

	if (!is_real_port && port != CHARGE_PORT_NONE)
		return EC_ERROR_INVAL;

	CPRINTS("New chg p%d", port);

	if (port == CHARGE_PORT_NONE) {
		/* Disable all ports. */
		for (i = 0; i < 2; i++) {
			rv = board_vbus_sink_enable(i, 0);
			if (rv) {
				CPRINTS("Disabling p%d sink path failed.", i);
				return rv;
			}
		}

		return EC_SUCCESS;
	}

	/* Check if the port is sourcing VBUS. */
	if (board_is_sourcing_vbus(port)) {
		CPRINTF("Skip enable p%d", port);
		return EC_ERROR_INVAL;
	}

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; i < 2; i++) {
		if (i == port)
			continue;

		if (board_vbus_sink_enable(i, 0))
			CPRINTS("p%d: sink path disable failed.", i);
	}

	/* Enable requested charge port. */
	if (board_vbus_sink_enable(port, 1)) {
		CPRINTS("p%d: sink path enable failed.");
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	/*
	 * Ignore lower charge ceiling on PD transition if our battery is
	 * critical, as we may brownout.
	 */
	if (supplier == CHARGE_SUPPLIER_PD &&
	    charge_ma < 1500 &&
	    charge_get_percent() < CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON) {
		CPRINTS("Using max ilim %d", max_ma);
		charge_ma = max_ma;
	}

	charge_set_input_current_limit(MAX(charge_ma,
					   CONFIG_CHARGER_INPUT_CURRENT),
				       charge_mv);
}

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_USB_C0_PD_INT_ODL))
		status |= PD_STATUS_TCPC_ALERT_0;
	if (!gpio_get_level(GPIO_USB_C1_PD_INT_ODL))
		status |= PD_STATUS_TCPC_ALERT_1;

	return status;
}
