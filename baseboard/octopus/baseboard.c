/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Octopus family-specific configuration */

#include "adc.h"
#include "adc_chip.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "common.h"
#include "driver/accel_kionix.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "driver/bc12/bq24392.h"
#include "driver/charger/bd9995x.h"
#include "driver/ppc/nx20p3483.h"
#include "driver/ppc/sn5s330.h"
#include "driver/tcpm/anx7447.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/tcpm.h"
#include "driver/usb_mux_it5205.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "motion_sense.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "system.h"
#include "tcpci.h"
#include "temp_sensor.h"
#include "thermistor.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/******************************************************************************/
/* Wake up pins */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_LID_OPEN,
	GPIO_AC_PRESENT,
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/******************************************************************************/
/* Power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
#ifdef CONFIG_POWER_S0IX
	{GPIO_PCH_SLP_S0_L,
		POWER_SIGNAL_ACTIVE_HIGH | POWER_SIGNAL_DISABLE_AT_BOOT,
		"SLP_S0_DEASSERTED"},
#endif
	{GPIO_PCH_SLP_S3_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,   POWER_SIGNAL_ACTIVE_HIGH, "SLP_S4_DEASSERTED"},
	{GPIO_SUSPWRDNACK,    POWER_SIGNAL_ACTIVE_HIGH,
	 "SUSPWRDNACK_DEASSERTED"},

	{GPIO_ALL_SYS_PGOOD,  POWER_SIGNAL_ACTIVE_HIGH, "ALL_SYS_PGOOD"},
	{GPIO_RSMRST_L_PGOOD, POWER_SIGNAL_ACTIVE_HIGH, "RSMRST_L"},
	{GPIO_PP3300_PG,      POWER_SIGNAL_ACTIVE_HIGH, "PP3300_PG"},
	{GPIO_PP5000_PG,      POWER_SIGNAL_ACTIVE_HIGH, "PP5000_PG"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/******************************************************************************/
/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	/*
	 * F3 key scan cycle completed but scan input is not
	 * charging to logic high when EC start scan next
	 * column for "T" key, so we set .output_settle_us
	 * to 80us from 50us.
	 */
	.output_settle_us = 80,
	.debounce_down_us = 9 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
#if defined(OCTOPUS_EC_NPCX796FB)
	{"battery", I2C_PORT_BATTERY, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"tcpc0",   I2C_PORT_TCPC0,   100, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"tcpc1",   I2C_PORT_TCPC1,   100, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"eeprom",  I2C_PORT_EEPROM,  100, GPIO_I2C3_SCL, GPIO_I2C3_SDA},
	{"charger", I2C_PORT_CHARGER, 100, GPIO_I2C4_SCL, GPIO_I2C4_SDA},
	{"sensor",  I2C_PORT_SENSOR,  100, GPIO_I2C7_SCL, GPIO_I2C7_SDA},
#elif defined(OCTOPUS_EC_ITE8320)
	{"power",  IT83XX_I2C_CH_A, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"sensor", IT83XX_I2C_CH_B, 100, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"usbc0",  IT83XX_I2C_CH_C, 100, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"usbc1",  IT83XX_I2C_CH_E, 100, GPIO_I2C4_SCL, GPIO_I2C4_SDA},
	{"eeprom", IT83XX_I2C_CH_F, 100, GPIO_I2C5_SCL, GPIO_I2C5_SDA},
#endif /* OCTOPUS_EC_ variant */
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/******************************************************************************/
/* USB-A Configuration */
const int usb_port_enable[USB_PORT_COUNT] = {
#if USB_PORT_COUNT == 1
	GPIO_EN_USB_A_5V,
	/* TODO(b/74388692): Add second port control after hardware fix. */
#else
	GPIO_EN_USB_A0_5V,
	GPIO_EN_USB_A1_5V,
#endif
};


/******************************************************************************/
/* USB-C Configuration */

#if defined(OCTOPUS_USBC_STANDALONE_TCPCS)
#define USB_PD_PORT_ANX7447	0
#define USB_PD_PORT_PS8751	1
#elif defined(OCTOPUS_USBC_ITE_EC_TCPCS)
#define USB_PD_PORT_ITE_0	0
#define USB_PD_PORT_ITE_1	1
#endif

/* USB-C TPCP Configuration */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
#if defined(OCTOPUS_USBC_STANDALONE_TCPCS)
	[USB_PD_PORT_ANX7447] = {
		.i2c_host_port = I2C_PORT_TCPC0,
		.i2c_slave_addr = AN7447_TCPC0_I2C_ADDR,
		.drv = &anx7447_tcpm_drv,
		.pol = TCPC_ALERT_ACTIVE_LOW,
	},
	[USB_PD_PORT_PS8751] = {
		.i2c_host_port = I2C_PORT_TCPC1,
		.i2c_slave_addr = PS8751_I2C_ADDR1,
		.drv = &ps8xxx_tcpm_drv,
		.pol = TCPC_ALERT_ACTIVE_LOW,
	},
#elif defined(OCTOPUS_USBC_ITE_EC_TCPCS)
	[USB_PD_PORT_ITE_0] = {
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it83xx_tcpm_drv,
		.pol = TCPC_ALERT_ACTIVE_LOW,
	},
	[USB_PD_PORT_ITE_1] = {
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it83xx_tcpm_drv,
		.pol = TCPC_ALERT_ACTIVE_LOW,
	},
#endif /* OCTOPUS_USBC_ variants */
};

/* USB-C MUX Configuration */

/* TODO(crbug.com/826441): Consolidate this logic with other impls */
#ifdef OCTOPUS_USBC_ITE_EC_TCPCS
static void board_it83xx_hpd_status(int port, int hpd_lvl, int hpd_irq)
{
	enum gpio_signal gpio = port ?
		GPIO_USB_C1_HPD_1V8_ODL : GPIO_USB_C0_HPD_1V8_ODL;

	/* Invert HPD level since GPIOs are active low. */
	hpd_lvl = !hpd_lvl;

	gpio_set_level(gpio, hpd_lvl);
	if (hpd_irq) {
		gpio_set_level(gpio, 1);
		msleep(1);
		gpio_set_level(gpio, hpd_lvl);
	}
}
#endif

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
#if defined(OCTOPUS_USBC_STANDALONE_TCPCS)
	[USB_PD_PORT_ANX7447] = {
		.port_addr = USB_PD_PORT_ANX7447,
		.driver = &anx7447_usb_mux_driver,
		.hpd_update = &anx7447_tcpc_update_hpd_status,
	},
	[USB_PD_PORT_PS8751] = {
		.port_addr = USB_PD_PORT_PS8751,
		.driver = &tcpci_tcpm_usb_mux_driver,
		.hpd_update = &ps8xxx_tcpc_update_hpd_status,
	}
#elif defined(OCTOPUS_USBC_ITE_EC_TCPCS)
	[USB_PD_PORT_ITE_0] = {
		/* Driver uses I2C_PORT_USB_MUX as I2C port */
		.port_addr = IT5205_I2C_ADDR1,
		.driver = &it5205_usb_mux_driver,
		.hpd_update = &board_it83xx_hpd_status,
	},
	[USB_PD_PORT_ITE_1] = {
		/* Use PS8751 as mux only */
		.port_addr = MUX_PORT_AND_ADDR(
			I2C_PORT_USBC1, PS8751_I2C_ADDR1),
		.driver = &tcpci_tcpm_usb_mux_driver,
		.hpd_update = &board_it83xx_hpd_status,
	}
#endif /* OCTOPUS_USBC_ variants */
};

/* USB-C PPC Configuration */
const struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_COUNT] = {
#if defined(OCTOPUS_USBC_STANDALONE_TCPCS)
	[USB_PD_PORT_ANX7447] = {
		.i2c_port = I2C_PORT_TCPC0,
		.i2c_addr = NX20P3483_ADDR2,
		.drv = &nx20p3483_drv,
	},
	[USB_PD_PORT_PS8751] = {
		.i2c_port = I2C_PORT_TCPC1,
		.i2c_addr = NX20P3483_ADDR2,
		.drv = &nx20p3483_drv,
		.flags = PPC_CFG_FLAGS_GPIO_CONTROL,
		.snk_gpio = GPIO_USB_C1_CHARGE_ON,
		.src_gpio = GPIO_EN_USB_C1_5V_OUT,
	},
#elif defined(OCTOPUS_USBC_ITE_EC_TCPCS)
	[USB_PD_PORT_ITE_0] = {
		.i2c_port = I2C_PORT_USBC0,
		.i2c_addr = SN5S330_ADDR0,
		.drv = &sn5s330_drv
	},
	[USB_PD_PORT_ITE_1] = {
		.i2c_port = I2C_PORT_USBC1,
		.i2c_addr = SN5S330_ADDR0,
		.drv = &sn5s330_drv
	},
#endif /* OCTOPUS_USBC_ variants */
};
const unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

/* BC 1.2 chip Configuration */
const struct bq24392_config_t bq24392_config[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.chip_enable_pin = GPIO_USB_C0_BC12_VBUS_ON,
		.chg_det_pin = GPIO_USB_C0_BC12_CHG_DET_L,
		.flags = BQ24392_FLAGS_CHG_DET_ACTIVE_LOW,
	},
	{
		.chip_enable_pin = GPIO_USB_C1_BC12_VBUS_ON,
		.chg_det_pin = GPIO_USB_C1_BC12_CHG_DET_L,
		.flags = BQ24392_FLAGS_CHG_DET_ACTIVE_LOW,
	},
};

/******************************************************************************/
/* Chipset callbacks/hooks */

/* Called by APL power state machine when transitioning from G3 to S5 */
void chipset_pre_init_callback(void)
{
	/* Enable 5.0V and 3.3V rails, and wait for Power Good */
	power_5v_enable(task_get_current(), 1);

	gpio_set_level(GPIO_EN_PP3300, 1);
	while (!gpio_get_level(GPIO_PP5000_PG) ||
	       !gpio_get_level(GPIO_PP3300_PG))
		;

	/* Enable PMIC */
	gpio_set_level(GPIO_PMIC_EN, 1);
}

/* Called by APL power state machine when transitioning to G3. */
void chipset_do_shutdown(void)
{
	/* Disable PMIC */
	gpio_set_level(GPIO_PMIC_EN, 0);

	/* Disable 5.0V and 3.3V rails, and wait until they power down. */
	power_5v_enable(task_get_current(), 0);

	/*
	 * Shutdown the 3.3V rail and wait for it to go down. We cannot wait
	 * for the 5V rail since other tasks may be using it.
	 */
	gpio_set_level(GPIO_EN_PP3300, 0);
	while (gpio_get_level(GPIO_PP3300_PG))
		;
}

/******************************************************************************/
/* Power Delivery and charing functions */

#ifdef OCTOPUS_USBC_STANDALONE_TCPCS
void board_tcpc_init(void)
{
	int count = 0;
	int port;

	/* Wait for disconnected battery to wake up */
	while (battery_hw_present() == BP_YES &&
	       battery_is_present() == BP_NO) {
		usleep(100 * MSEC);
		/* Give up waiting after 1 second */
		if (++count > 10) {
			ccprintf("TCPC_init: 1 second w/no battery\n");
			break;
		}
	}

	/* Only reset TCPC if not sysjump */
	if (!system_jumped_to_this_image())
		board_reset_pd_mcu();

	/* Enable PPC interrupts. */
	gpio_enable_interrupt(GPIO_USB_PD_C0_INT_L);
	gpio_enable_interrupt(GPIO_USB_PD_C1_INT_L);

	/* Enable TCPC interrupts. */
	gpio_enable_interrupt(GPIO_USB_C0_PD_INT_ODL);
	gpio_enable_interrupt(GPIO_USB_C1_PD_INT_ODL);

	/*
	 * Initialize HPD to low; after sysjump SOC needs to see
	 * HPD pulse to enable video path
	 */
	for (port = 0; port < CONFIG_USB_PD_PORT_COUNT; port++) {
		const struct usb_mux *mux = &usb_muxes[port];

		mux->hpd_update(port, 0, 0);
	}
}
DECLARE_HOOK(HOOK_INIT, board_tcpc_init, HOOK_PRIO_INIT_I2C + 1);
#endif /* OCTOPUS_USBC_STANDALONE_TCPCS */

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

#if defined(OCTOPUS_USBC_STANDALONE_TCPCS)
	if (!gpio_get_level(GPIO_USB_C0_PD_INT_ODL))
		status |= PD_STATUS_TCPC_ALERT_0;

	if (!gpio_get_level(GPIO_USB_C1_PD_INT_ODL)) {
		if (gpio_get_level(GPIO_USB_C1_PD_RST_ODL))
			status |= PD_STATUS_TCPC_ALERT_1;
	}
#elif defined(OCTOPUS_USBC_STANDALONE_TCPCS)
	/*
	 * Since C0/C1 TCPC are embedded within EC, we don't need the PDCMD
	 * tasks.The (embedded) TCPC status since chip driver code will
	 * handles its own interrupts and forward the correct events to
	 * the PD_C0 task. See it83xx/intc.c
	 */
#endif /* OCTOPUS_USBC_ variant */

	return status;
}

int board_set_active_charge_port(int port)
{
	int is_valid_port = (port >= 0 &&
			    port < CONFIG_USB_PD_PORT_COUNT);
	int i;

	if (!is_valid_port && port != CHARGE_PORT_NONE)
		return EC_ERROR_INVAL;


	if (port == CHARGE_PORT_NONE) {
		CPRINTSUSB("Disabling all charger ports");

		/* Disable all ports. */
		for (i = 0; i < ppc_cnt; i++) {
			/*
			 * Do not return early if one fails otherwise we can
			 * get into a boot loop assertion failure.
			 */
			if (ppc_vbus_sink_enable(i, 0))
				CPRINTSUSB("Disabling C%d as sink failed.", i);
		}

		return EC_SUCCESS;
	}

	/* Check if the port is sourcing VBUS. */
	if (ppc_is_sourcing_vbus(port)) {
		CPRINTFUSB("Skip enable C%d", port);
		return EC_ERROR_INVAL;
	}

	CPRINTSUSB("New charge port: C%d", port);

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; i < ppc_cnt; i++) {
		if (i == port)
			continue;

		if (ppc_vbus_sink_enable(i, 0))
			CPRINTSUSB("C%d: sink path disable failed.", i);
	}

	/* Enable requested charge port. */
	if (ppc_vbus_sink_enable(port, 1)) {
		CPRINTSUSB("C%d: sink path enable failed.", port);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_set_input_current_limit(MAX(charge_ma,
					   CONFIG_CHARGER_INPUT_CURRENT),
				       charge_mv);
}

/**
 * Reset all system PD/TCPC MCUs -- currently only called from
 * handle_pending_reboot() in common/power.c just before hard
 * resetting the system. This logic is likely not needed as the
 * PP3300_A rail should be dropped on EC reset.
 */
void board_reset_pd_mcu(void)
{
#if defined(OCTOPUS_USBC_STANDALONE_TCPCS)
	/* C0: ANX7447 does not have a reset pin. */

	/* C1: Assert reset to TCPC1 (PS8751) for required delay (1ms) */
	gpio_set_level(GPIO_USB_C1_PD_RST_ODL, 0);
	msleep(PS8XXX_RESET_DELAY_MS);
	gpio_set_level(GPIO_USB_C1_PD_RST_ODL, 1);
#elif defined(OCTOPUS_USBC_ITE_EC_TCPCS)
	/*
	 * C0 & C1: The internal TCPC on ITE EC does not have a reset signal,
	 * but it will get reset when the EC gets reset.
	 */
#endif
}

#ifdef OCTOPUS_USBC_ITE_EC_TCPCS
void board_pd_vconn_ctrl(int port, int cc_pin, int enabled)
{
	/*
	 * We ignore the cc_pin because the polarity should already be set
	 * correctly in the PPC driver via the pd state machine.
	 */
	if (ppc_set_vconn(port, enabled) != EC_SUCCESS)
		cprints(CC_USBPD, "C%d: Failed %sabling vconn",
			port, enabled ? "en" : "dis");
}
#endif
