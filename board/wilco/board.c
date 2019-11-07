/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MEC1701 EVB board-specific configuration */

#include "chip/mchp/tfdp_chip.h"
#include "als.h"
#include "bd99992gw.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "driver/accel_kionix.h"
#include "driver/accel_kx022.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/tcpm/tcpci.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "espi.h"
#include "lpc_chip.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "math_util.h"
#include "motion_sense.h"
#include "motion_lid.h"
#include "pi3usb9281.h"
#include "power.h"
#include "power_button.h"
#include "spi.h"
#include "spi_chip.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "temp_sensor.h"
#include "timer.h"
#include "uart.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "util.h"
#include "espi.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

#ifdef CONFIG_BOARD_PRE_INIT
/*
 * Used to enable JTAG debug during development.
 * NOTE: If ARM Serial Wire Viewer not used then SWV pin can be
 * be disabled and used for another purpose. Change mode to
 * MEC17XX_JTAG_MODE_SWD.
 */
void board_config_pre_init(void)
{
#ifdef CONFIG_CHIPSET_DEBUG
//	MEC17XX_EC_JTAG_EN = MEC17XX_JTAG_ENABLE + MEC17XX_JTAG_MODE_SWD_SWV;
#endif
}
#endif

#ifdef CONFIG_BOARD_HAS_BEFORE_RSMRST
/*
 * called from power/intel_x86.c when
 * RSMRST# and RSMRST_PGOOD_L are different.
 * parameter rsmrst is current state of
 * RSMRST_PGOOD_L pin.
 */
void board_before_rsmrst(int rsmrst)
{
	trace1(0, BRD, 0, "RSMRST_L_PGOOD transitioned to %d", rsmrst);
}
#endif


/*
 * Handle Skylake/Kabylake ALL_SYS_PWRGD signal on RVP3 board.
 * called from power_handle_state in power/skylake.c
 * Host chipset manufacturer indicated KBL requires 100 ms! We hope this
 * is KBL reference board and not host chipset requirement.
 */
// #ifdef CONFIG_BOARD_SKL_RVP3
#if 0
void board_handle_all_sus(enum power_state state)
{
	int allsys_in = gpio_get_level(GPIO_ALL_SYS_PWRGD);
	int allsys_out = gpio_get_level(GPIO_SYS_RESET_L);

	if (allsys_in == allsys_out)
		return;

	CPRINTS("PwrState=%d ALL_SYS_PWRGD=%d  SYS_RESET_L=%d",
		state, allsys_in, allsys_out);

	trace3(0, BRD, 0, "PowerState=%d ALL_SYS_PWRGD=%d SYS_RESET_L=%d",
	       state, allsys_in, allsys_out);

	/*
	 * Wait at least 10 ms between power signals going high
	 */
	if (allsys_in)
		msleep(100);

	if (!allsys_out) {
		/* CPRINTS("Set SYS_RESET_L = %d", allsys_in); */
		trace1(0, BRD, 0, "Set SYS_RESET_L=%d", allsys_in);
		gpio_set_level(GPIO_SYS_RESET_L, allsys_in);
	}

}
#endif

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	{ CONFIG_SPI_FLASH_PORT, 4, GPIO_QMSPI_CS0 },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

const enum gpio_signal hibernate_wake_pins[] = {
        GPIO_LID_OPEN,
};

int extpower_is_present(void)
{
	return 1;
}

const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/*
 * enable_input_devices() is called by the tablet_mode ISR, but changes the
 * state of GPIOs, so its definition must reside after including gpio_list.
 */
static void enable_input_devices(void);
DECLARE_DEFERRED(enable_input_devices);

void tablet_mode_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&enable_input_devices_data, 0);
}

void bc_link_interrupt(enum gpio_signal signal);

#include "gpio_list.h"

#if 0
/*
 * Logical SPI port to SPI controller table
 * Logical Port 0 = QMSPI0 SHD Port
 * Logical Port 1 = GPSPI0
 */
const uint8_t spi_port_to_ctrl[] = {
	(QMSPI0_PORT),
	(GPSPI0_PORT),
};
const unsigned int spi_ports_used = ARRAY_SIZE(spi_port_to_ctrl);

int board_spi_p2c(int port)
{
	int i2c_ctrl = -1;

	if (port < spi_ports_used)
		i2c_ctrl = (int)spi_port_to_ctrl[port];

	return i2c_ctrl;
}
#endif

void chipset_set_pmic_slp_sus_l(int level)
{
}

void board_handle_all_sus(enum power_state state)
{
}

int fake_get_temp(int idx, int *temp_ptr)
{
	return 0;
}

const struct temp_sensor_t temp_sensors[] = {
        {"Battery", TEMP_SENSOR_TYPE_BATTERY, fake_get_temp, 0, 4},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);


/* Initialize board. */
static void board_init(void)
{
	CPRINTS("HOOK_INIT - called board_init");
	trace0(0, HOOK, 0, "HOOK_INIT - call board_init");

	/* Provide AC status to the PCH */
	gpio_set_level(GPIO_PCH_ACOK, 1);

	gpio_enable_interrupt(GPIO_BC_INT_L);
	gpio_config_module(MODULE_I2C, 1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);


#ifdef CONFIG_CHARGER
/**
 * Set active charge port -- only one port can be active at a time.
 *
 * @param charge_port   Charge port to enable.
 *
 * Returns EC_SUCCESS if charge port is accepted and made active,
 * EC_ERROR_* otherwise.
 */
int board_set_active_charge_port(int charge_port)
{
	/* charge port is a realy physical port */
	int is_real_port = (charge_port >= 0 &&
			    charge_port < CONFIG_USB_PD_PORT_COUNT);
	/* check if we are source vbus on that port */
	int source = gpio_get_level(charge_port == 0 ? GPIO_USB_C0_5V_EN :
						       GPIO_USB_C1_5V_EN);

	if (is_real_port && source) {
		CPRINTS("Skip enable p%d", charge_port);
		trace1(0, BOARD, 0, "Skip enable charge port %d",
			charge_port);
		return EC_ERROR_INVAL;
	}

	CPRINTS("New chg p%d", charge_port);
	trace1(0, BOARD, 0, "New charge port %d", charge_port);

	if (charge_port == CHARGE_PORT_NONE) {
		/* Disable both ports */
		gpio_set_level(GPIO_USB_C0_CHARGE_EN_L, 1);
		gpio_set_level(GPIO_USB_C1_CHARGE_EN_L, 1);
	} else {
		/* Make sure non-charging port is disabled */
		gpio_set_level(charge_port ? GPIO_USB_C0_CHARGE_EN_L :
					     GPIO_USB_C1_CHARGE_EN_L, 1);
		/* Enable charging port */
		gpio_set_level(charge_port ? GPIO_USB_C1_CHARGE_EN_L :
					     GPIO_USB_C0_CHARGE_EN_L, 0);
	}

	return EC_SUCCESS;
}

/**
 * Set the charge limit based upon desired maximum.
 *
 * @param port          Port number.
 * @param supplier      Charge supplier type.
 * @param charge_ma     Desired charge limit (mA).
 * @param charge_mv     Negotiated charge voltage (mV).
 */
void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_set_input_current_limit(MAX(charge_ma,
				   CONFIG_CHARGER_INPUT_CURRENT), charge_mv);
}

/**
 * Return whether ramping is allowed for given supplier
 */
int board_is_ramp_allowed(int supplier)
{
	/* Don't allow ramping in RO when write protected */
	if (system_get_image_copy() != SYSTEM_IMAGE_RW
	    && system_is_locked())
		return 0;
	else
		return supplier == CHARGE_SUPPLIER_BC12_DCP ||
		       supplier == CHARGE_SUPPLIER_BC12_SDP ||
		       supplier == CHARGE_SUPPLIER_BC12_CDP ||
		       supplier == CHARGE_SUPPLIER_PROPRIETARY;
}

/**
 * Return the maximum allowed input current
 */
int board_get_ramp_current_limit(int supplier, int sup_curr)
{
	switch (supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
		return 2000;
	case CHARGE_SUPPLIER_BC12_SDP:
		return 1000;
	case CHARGE_SUPPLIER_BC12_CDP:
	case CHARGE_SUPPLIER_PROPRIETARY:
		return sup_curr;
	default:
		return 500;
	}
}
#else
/*
 * TODO HACK providing functions from common/charge_state_v2.c
 * which is not compiled in when no charger
 */
int charge_want_shutdown(void)
{
	return 0;
}

int charge_prevent_power_on(int power_button_pressed)
{
	return 0;
}


#endif

/*
 * Enable or disable input devices,
 * based upon chipset state and tablet mode
 */
static void enable_input_devices(void)
{
	int kb_enable = 1;

	keyboard_scan_enable(kb_enable, KB_SCAN_DISABLE_LID_ANGLE);
	// gpio_set_level(GPIO_ENABLE_TOUCHPAD, tp_enable);
}

/* Called on AP S5 -> S3 transition */
static void board_chipset_startup(void)
{
	CPRINTS("HOOK_CHIPSET_STARTUP - called board_chipset_startup");
	trace0(0, HOOK, 0, "HOOK_CHIPSET_STARTUP - board_chipset_startup");
	hook_call_deferred(&enable_input_devices_data, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S5 transition */
static void board_chipset_shutdown(void)
{
	CPRINTS("HOOK_CHIPSET_SHUTDOWN board_chipset_shutdown");
	trace0(0, HOOK, 0,
	       "HOOK_CHIPSET_SHUTDOWN board_chipset_shutdown");
	hook_call_deferred(&enable_input_devices_data, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
	CPRINTS("HOOK_CHIPSET_RESUME - called board_chipset_resume");
	trace0(0, HOOK, 0, "HOOK_CHIPSET_RESUME - board_chipset_resume");
	/*
	 * Now that we have enabled the rail to the sensors, let's give enough
	 * time for the sensors to boot up.  Without this delay, the very first
	 * i2c transactions always fail because the sensors aren't ready yet.
	 * In testing, a 2ms delay seemed to be reliable, but we'll delay for
	 * 3ms just to be safe.
	 *
	 * Additionally, this hook needs to be run before the motion sense hook
	 * tries to initialize the sensors.
	 */
	msleep(3);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume,
	     MOTION_SENSE_HOOK_PRIO-1);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	CPRINTS("HOOK_CHIPSET_SUSPEND - called board_chipset_resume");
	trace0(0, HOOK, 0, "HOOK_CHIPSET_SUSPEND - board_chipset_suspend");
#if 0 /* TODO not implemented in gpio.inc */
	gpio_set_level(GPIO_PP1800_DX_AUDIO_EN, 0);
	gpio_set_level(GPIO_PP1800_DX_SENSOR_EN, 0);
#endif
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

void board_hibernate_late(void)
{
	/* Turn off LEDs in hibernate */
	gpio_set_level(GPIO_CHARGE_LED_1, 0);
	gpio_set_level(GPIO_CHARGE_LED_2, 0);
}

/* Any glados boards post version 2 should have ROP_LDO_EN stuffed. */
#define BOARD_MIN_ID_LOD_EN 2
/* Make the pmic re-sequence the power rails under these conditions. */
#define PMIC_RESET_FLAGS \
	(RESET_FLAG_WATCHDOG | RESET_FLAG_SOFT | RESET_FLAG_HARD)
static void board_handle_reboot(void)
{
#if 0 /* TODO MCHP KBL hack */
	int flags;
#endif
	CPRINTS("HOOK_INIT - called board_handle_reboot");
	trace0(0, HOOK, 0, "HOOK_INIT - board_handle_reboot");

	if (system_jumped_to_this_image())
		return;

	if (system_get_board_version() < BOARD_MIN_ID_LOD_EN)
		return;

#if 0 /* TODO MCHP KBL hack not PMIC system */
	/* Interrogate current reset flags from previous reboot. */
	flags = system_get_reset_flags();

	if (!(flags & PMIC_RESET_FLAGS))
		return;

	/* Preserve AP off request. */
	if (flags & RESET_FLAG_AP_OFF)
		chip_save_reset_flags(RESET_FLAG_AP_OFF);

	ccprintf("Restarting system with PMIC.\n");
	/* Flush console */
	cflush();

	/* Bring down all rails but RTC rail (including EC power). */
	gpio_set_flags(GPIO_BATLOW_L_PMIC_LDO_EN, GPIO_OUT_HIGH);
	while (1)
		; /* wait here */
#else
	return;
#endif
}
DECLARE_HOOK(HOOK_INIT, board_handle_reboot, HOOK_PRIO_FIRST);

/* MCHP DEBUG */
void board_one_sec(void)
{
	trace0(0, BRD, 0, "HOOK_SECOND");

	if (gpio_get_level(GPIO_CHARGE_LED_2))
		gpio_set_level(GPIO_CHARGE_LED_2, 0);
	else
		gpio_set_level(GPIO_CHARGE_LED_2, 1);
}
DECLARE_HOOK(HOOK_TICK, board_one_sec, HOOK_PRIO_DEFAULT);

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
		0xff, 0x7e, 0x7f, 0xff, 0x14, 0x00, 0x42, 0x48,
		0x81, 0x81, 0xa8, 0xa8, 0xfb, 0xbf, 0x7e, 0x7f,
		0xff,
	},
};
