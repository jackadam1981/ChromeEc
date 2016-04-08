/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Reef board-specific configuration */

//#include "adc.h"
#include "adc_chip.h"
#include "als.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "driver/als_opt3001.h"
#include "driver/accel_kionix.h"
#include "driver/accel_kx022.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/charger/bd99955.h"	/* FIXME(dhendrix): Need to vet this against BD99956 */
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "math_util.h"
#include "motion_sense.h"
#include "motion_lid.h"
//#include "pi3usb9281.h"	/* using Parade PS8751 instead? */
#include "power.h"
#include "power_button.h"
#include "spi.h"
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

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

//#define I2C_ADDR_BD99992 0x60

/* Exchange status with PD MCU. */
static void pd_mcu_interrupt(enum gpio_signal signal)
{
#ifdef HAS_TASK_PDCMD
	/* Exchange status with PD MCU to determine interrupt cause */
	host_command_pd_send_status(0);
#endif
}

void vbus0_evt(enum gpio_signal signal)
{
	/* VBUS present GPIO is inverted */
//	usb_charger_vbus_change(0, !gpio_get_level(signal));
	task_wake(TASK_ID_PD_C0);
}

void vbus1_evt(enum gpio_signal signal)
{
	/* VBUS present GPIO is inverted */
//	usb_charger_vbus_change(1, !gpio_get_level(signal));
	task_wake(TASK_ID_PD_C1);
}

#if 0
void usb0_evt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12, 0);
}

void usb1_evt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_USB_CHG_P1, USB_CHG_EVENT_BC12, 0);
}
#endif

/*
 * enable_input_devices() is called by the tablet_mode ISR, but changes the
 * state of GPIOs, so its definition must reside after including gpio_list.
 * Use DECLARE_DEFERRED to generate enable_input_devices_data.
 */
static void enable_input_devices(void);
DECLARE_DEFERRED(enable_input_devices);

void tablet_mode_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&enable_input_devices_data, 0);
}

#include "gpio_list.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_PCH_RSMRST_L,       1, "RSMRST_L"},
	{GPIO_PCH_SLP_S0_L,       1, "PMU_SLP_S0_N"},
	{GPIO_PCH_SLP_S3_L,       1, "SLP_S3_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,       1, "SLP_S4_DEASSERTED"},
	{GPIO_SUSPWRNACK,         1, "SUSPWRNACK_DEASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

#if 0
/* FIXME(dhendrix): How to handle TEMP_SENSOR_AMBIENT and TEMP_SENSOR_CHARGER? */
/* ADC channels */
const struct adc_t adc_channels[] = {
	/* Vbus sensing. Converted to mV, full ADC is equivalent to 30V. */
	/* FIXME(dhendrix): double-check these values */
	[ADC_VBUS] = {"VBUS", 30000, 1024, 0, 1},
	/* Adapter current output or battery discharging current */
	[ADC_AMON_BMON] = {"AMON_BMON", 25000, 3072, 0, 3},
	/* System current consumption */
	[ADC_PSYS] = {"PSYS", 1, 1, 0, 4},

};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
#endif

const struct i2c_port_t i2c_ports[]  = {
#if 0
	{"pmic",     MEC1322_I2C0_0, 400,  GPIO_I2C0_0_SCL, GPIO_I2C0_0_SDA},
	{"muxes",    MEC1322_I2C0_1, 400,  GPIO_I2C0_1_SCL, GPIO_I2C0_1_SDA},
	{"pd_mcu",   MEC1322_I2C1,   500,  GPIO_I2C1_SCL,   GPIO_I2C1_SDA},
	{"sensors",  MEC1322_I2C2,   400,  GPIO_I2C2_SCL,   GPIO_I2C2_SDA  },
	{"batt",     MEC1322_I2C3,   100,  GPIO_I2C3_SCL,   GPIO_I2C3_SDA  },
#endif
	/* FIXME(dhendrix): verify speeds */
	{"usb_c0_pd", NPCX_I2C_PORT0_0, 400, GPIO_EC_I2C_USB_C0_PD_SCL, GPIO_EC_I2C_USB_C0_PD_SDA},
	{"usb_c1_pd", NPCX_I2C_PORT0_1, 400, GPIO_EC_I2C_USB_C1_PD_SCL, GPIO_EC_I2C_USB_C1_PD_SDA},
	{"gyro",      NPCX_I2C_PORT1,   400, GPIO_EC_I2C_GYRO_SCL,      GPIO_EC_I2C_GYRO_SDA},
	{"sensors",   NPCX_I2C_PORT2,   400, GPIO_EC_I2C_SENSOR_SCL,    GPIO_EC_I2C_SENSOR_SDA},
	{"batt",      NPCX_I2C_PORT3,   100, GPIO_EC_I2C_POWER_SCL,     GPIO_EC_I2C_POWER_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	/* FIXME(dhendrix): why is CONFIG_TCPC_I2C_BASE_ADDR set to
	   0x9c in include/config.h? */
//	{I2C_PORT_TCPC, CONFIG_TCPC_I2C_BASE_ADDR},
//	{I2C_PORT_TCPC, CONFIG_TCPC_I2C_BASE_ADDR + 2},
	{I2C_PORT_TCPC, 0x96},	/* PS8751A, 7-bit addr? */
	{I2C_PORT_TCPC, 0x7c},	/* ANX3429, 7-bit addr? */
};

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_USB_C0_PD_INT))
		status |= PD_STATUS_TCPC_ALERT_0;
	if (!gpio_get_level(GPIO_USB_C1_PD_INT_ODL))
		status |= PD_STATUS_TCPC_ALERT_1;

	return status;
}

const enum gpio_signal hibernate_wake_pins[] = {
//	GPIO_AC_PRESENT,
	GPIO_LID_OPEN,
//	GPIO_KB_PWR_BTN_ODL,
	GPIO_POWER_BUTTON_L,
};

const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
//		.port_addr = 0xa8,
//		.driver = &pi3usb30532_usb_mux_driver,
		.port_addr = 0xa0,
//		.driver = &anx3429_usb_mux_driver,
	},
	{
		.port_addr = 0x20,
//		.driver = &ps8740_usb_mux_driver,
//		.driver = &ps8751a_usb_mux_driver,
	}
};

/**
 * Reset PD MCU
 */
void board_reset_pd_mcu(void)
{
	gpio_set_level(GPIO_USB_PD_RST_L, 0);
	usleep(100);
	gpio_set_level(GPIO_USB_PD_RST_L, 1);
}

const struct temp_sensor_t temp_sensors[] = {
	/* FIXME(dhendrix): tweak action_delay_sec */
	{"Battery", TEMP_SENSOR_TYPE_BATTERY, charge_temp_sensor_get_val, 0, 4},
	/* FIXME(dhendrix): implement function to get ambient temp */
//	{"Ambient", TEMP_SENSOR_TYPE_BOARD, board_get_ambient_temp, 0, 4},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/*
 * Thermal limits for each temp sensor.  All temps are in degrees K.  Must be in
 * same order as enum temp_sensor_id.  To always ignore any temp, use 0.
 */
struct ec_thermal_config thermal_params[] = {
	/* {Twarn, Thigh, Thalt}, fan_off, fan_max */
	{{0, 0, 0}, 0, 0},	/* Battery */
//	{{0, 0, 0}, 0, 0},	/* Ambient */
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);

/* ALS instances. Must be in same order as enum als_id. */
struct als_t als[] = {
	/* FIXME(dhendrix): tune attenuation_factor */
	{"TI", opt3001_init, opt3001_read_lux, 5},
};
BUILD_ASSERT(ARRAY_SIZE(als) == ALS_COUNT);

const struct button_config buttons[CONFIG_BUTTON_COUNT] = {
	{"Volume Down", KEYBOARD_BUTTON_VOLUME_DOWN, GPIO_EC_VOLDN_BTN_L,
	 30 * MSEC, 0},
	{"Volume Up", KEYBOARD_BUTTON_VOLUME_UP, GPIO_EC_VOLUP_BTN_L,
	 30 * MSEC, 0},
};

static void board_pmic_init(void)
{
	/* No need to re-init PMIC since settings are sticky across sysjump */
	if (system_jumped_to_this_image())
		return;

#if 0
	/* Set CSDECAYEN / VCCIO decays to 0V at assertion of SLP_S0# */
	i2c_write8(I2C_PORT_PMIC, I2C_ADDR_BD99992, 0x30, 0x4a);

	/*
	 * Set V100ACNT / V1.00A Control Register:
	 * Nominal output = 1.0V.
	 */
	i2c_write8(I2C_PORT_PMIC, I2C_ADDR_BD99992, 0x37, 0x1a);

	/*
	 * Set V085ACNT / V0.85A Control Register:
	 * Lower power mode = 0.7V.
	 * Nominal output = 1.0V.
	 */
	i2c_write8(I2C_PORT_PMIC, I2C_ADDR_BD99992, 0x38, 0x7a);

	/* VRMODECTRL - enable low-power mode for VCCIO and V0.85A */
	i2c_write8(I2C_PORT_PMIC, I2C_ADDR_BD99992, 0x3b, 0x18);
#endif
	gpio_set_level(GPIO_V5A_EN, 1);
}
DECLARE_HOOK(HOOK_INIT, board_pmic_init, HOOK_PRIO_DEFAULT);

/* Initialize board. */
static void board_init(void)
{
	struct charge_port_info charge_none;
	int i;

	/* Enable PD interrupts */
	gpio_enable_interrupt(GPIO_USB_C0_PD_INT);
	gpio_enable_interrupt(GPIO_USB_C1_PD_INT_ODL);

	/* Enable charger interrupts */
//	gpio_enable_interrupt(GPIO_CHARGER_EC_INT_ODL);
	gpio_enable_interrupt(GPIO_PD_MCU_INT);

	/* Enable tablet mode interrupt for input device enable */
//	gpio_enable_interrupt(GPIO_TABLET_MODE_L);

#if 0
	/* FIXME(dhendrix): This is tied high on reef? */
	/* Provide AC status to the PCH */
	gpio_set_level(GPIO_PCH_ACOK, extpower_is_present());
#endif

	/* Initialize all pericom charge suppliers to 0 */
	charge_none.voltage = USB_CHARGER_VOLTAGE_MV;
	charge_none.current = 0;
	/* TODO: Implement BC1.2 + VBUS detection */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++) {
		charge_manager_update_charge(CHARGE_SUPPLIER_PROPRIETARY,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_CDP,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_DCP,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_BC12_SDP,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_OTHER,
					     i,
					     &charge_none);
		charge_manager_update_charge(CHARGE_SUPPLIER_VBUS,
					     i,
					     &charge_none);
	}
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

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
	enum bd99955_charge_port bd99955_port;

	CPRINTS("New chg p%d", charge_port);

	switch (charge_port) {
	case 0:
		bd99955_port = BD99955_CHARGE_PORT_VBUS;
		break;
	case 1:
		bd99955_port = BD99955_CHARGE_PORT_VCC;
		break;
	case CHARGE_PORT_NONE:
		bd99955_port = BD99955_CHARGE_PORT_NONE;
		break;
	default:
		panic("Invalid charge port\n");
		break;
	}

	return bd99955_select_input_port(bd99955_port);
}

/**
 * Set the charge limit based upon desired maximum.
 *
 * @param charge_ma     Desired charge limit (mA).
 */
void board_set_charge_limit(int charge_ma)
{
	charge_set_input_current_limit(MAX(charge_ma,
					   CONFIG_CHARGER_INPUT_CURRENT));
}

int extpower_is_present(void)
{
	return bd99955_extpower_is_present();
}

#if 0
/* FIXME(dhendrix): On Glados but not Amenia? */
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
#endif

/* Enable or disable input devices, based upon chipset state and tablet mode */
static void enable_input_devices(void)
{
	int kb_enable = 1;

	/* Disable KB if chipset is off */
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		kb_enable = 0;

	keyboard_scan_enable(kb_enable, KB_SCAN_DISABLE_LID_ANGLE);
}

/* Called on AP S5 -> S3 transition */
static void board_chipset_startup(void)
{
	hook_call_deferred(&enable_input_devices_data, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S5 transition */
static void board_chipset_shutdown(void)
{
	hook_call_deferred(&enable_input_devices_data, 0);
	/* FIXME(dhendrix): Drive USB_PD_RST_ODL low to prevent
	   leakage? (see comment in schematic) */
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);

void board_set_gpio_hibernate_state(void)
{
	int i;
	const uint32_t hibernate_pins[][2] = {
#if 0
		/* Turn off LEDs in hibernate */
		{GPIO_BAT_LED_BLUE, GPIO_INPUT | GPIO_PULL_UP},
		{GPIO_BAT_LED_AMBER, GPIO_INPUT | GPIO_PULL_UP},
		/*
		 * Set PD wake low so that it toggles high to generate a wake
		 * event once we leave hibernate.
		 */
		{GPIO_USB_PD_WAKE, GPIO_OUTPUT | GPIO_LOW},
#endif
		/*
		 * In hibernate, this pin connected to GND. Set it to output
		 * low to eliminate the current caused by internal pull-up.
		 */
		/* FIXME(dhendrix): What do to with PROCHOT? */
//		{GPIO_PLATFORM_EC_PROCHOT, GPIO_OUTPUT | GPIO_LOW},

		/*
		 * BD99956 handles charge input automatically. We'll disable
		 * charge output in hibernate. Charger will assert ACOK_OD
		 * when VBUS or VCC are plugged in.
		 */
		{GPIO_USB_C0_5V_EN,       GPIO_INPUT  | GPIO_PULL_DOWN},
		{GPIO_USB_C1_5V_EN,       GPIO_INPUT  | GPIO_PULL_DOWN},
	};

	/* Change GPIOs' state in hibernate for better power consumption */
	for (i = 0; i < ARRAY_SIZE(hibernate_pins); ++i)
		gpio_set_flags(hibernate_pins[i][0], hibernate_pins[i][1]);

	gpio_config_module(MODULE_KEYBOARD_SCAN, 0);

	/*
	 * Calling gpio_config_module sets disabled alternate function pins to
	 * GPIO_INPUT.  But to prevent keypresses causing leakage currents
	 * while hibernating we want to enable GPIO_PULL_UP as well.
	 */
	gpio_set_flags_by_mask(0x2, 0x03, GPIO_INPUT | GPIO_PULL_UP);
	gpio_set_flags_by_mask(0x1, 0xFF, GPIO_INPUT | GPIO_PULL_UP);
	gpio_set_flags_by_mask(0x0, 0xE0, GPIO_INPUT | GPIO_PULL_UP);
}

#if 0
static void board_handle_reboot(void)
{
	int flags;

	if (system_jumped_to_this_image())
		return;

	if (system_get_board_version() < BOARD_MIN_ID_LOD_EN)
		return;

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
}
DECLARE_HOOK(HOOK_INIT, board_handle_reboot, HOOK_PRIO_FIRST);
#endif

/* Motion sensors */
/* Mutexes */
static struct mutex g_lid_mutex;
static struct mutex g_base_mutex;

/* Matrix to rotate accelrator into standard reference frame */
const matrix_3x3_t base_standard_ref = {
	{ 0,  FLOAT_TO_FP(1),  0},
	{ FLOAT_TO_FP(-1), 0,  0},
	{ 0,  0,  FLOAT_TO_FP(1)}
};

/* KX022 private data */
struct kionix_accel_data g_kx022_data = {
	.variant = KX022,
};

/* FIXME(dhendrix): Copied from Amenia, probably need to tweak for Reef */
struct motion_sensor_t motion_sensors[] = {
	/*
	 * Note: bmi160: supports accelerometer and gyro sensor
	 * Requirement: accelerometer sensor must init before gyro sensor
	 * DO NOT change the order of the following table.
	 */
	[LID_ACCEL] = {
	 .name = "Lid Accel",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .rot_standard_ref = NULL, /* Identity matrix. */
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default use EC settings */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 10000 | ROUND_UP_FLAG,
			 .ec_rate = 100 * MSEC,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 10000 | ROUND_UP_FLAG,
			 .ec_rate = 100 * MSEC,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S3] = {
			 .odr = 0,
			 .ec_rate = 0
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0
		 },
	 },
	},

	[LID_GYRO] = {
	 .name = "Lid Gyro",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .default_range = 1000, /* dps */
	 .rot_standard_ref = NULL, /* Identity Matrix. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC does not need in S0 */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S3] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
	 },
	},

	[LID_MAG] = {
	 .name = "Lid Mag",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_MAG,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .default_range = 1 << 11, /* 16LSB / uT, fixed */
	 .rot_standard_ref = NULL, /* Identity Matrix. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC does not need in S0 */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S3] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* Sensor off in S3/S5 */
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
	 },
	},

	[BASE_ACCEL] = {
	 .name = "Base Accel",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_KXCJ9,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &kionix_accel_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = &g_kx022_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = KXCJ9_ADDR1,
	 .rot_standard_ref = &base_standard_ref, /* Identity matrix. */
	 .default_range = 2, /* g, enough for laptop. */
	 .config = {
		/* AP: by default use EC settings */
		[SENSOR_CONFIG_AP] = {
			.odr = 10000 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		},
		/* EC use accel for angle detection */
		[SENSOR_CONFIG_EC_S0] = {
			.odr = 10000 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		},
		/* unused */
		[SENSOR_CONFIG_EC_S3] = {
			.odr = 0,
			.ec_rate = 0,
		},
		[SENSOR_CONFIG_EC_S5] = {
			.odr = 0,
			.ec_rate = 0,
		},
	 },
	},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

void board_hibernate(void)
{
	CPRINTS("Enter Pseudo G3");

	/*
	 * Clean up the UART buffer and prevent any unwanted garbage characters
	 * before power off and also ensure above debug message is printed.
	 */
	cflush();

	/* FIXME(dhendrix): What to do here? */
//	gpio_set_level(GPIO_G3_SLEEP_EN, 1);

	/* Power to EC should shut down now */
	while (1)
		;
}
