/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "backlight.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_ramp.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/battery/max17055.h"
#include "driver/bc12/pi3usb9201.h"
#include "driver/tcpm/fusb302.h"
#include "driver/sync.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static void tcpc_alert_event(enum gpio_signal signal)
{
	schedule_deferred_pd_interrupt(0 /* port */);
}

#include "gpio_list.h"

/******************************************************************************/
/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	[ADC_BOARD_ID] = {"BOARD_ID", 3300, 4096, 0, STM32_AIN(10)},
	[ADC_EC_SKU_ID] = {"EC_SKU_ID", 3300, 4096, 0, STM32_AIN(8)},
	[ADC_BATT_ID] = {"BATT_ID", 3300, 4096, 0, STM32_AIN(7)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/******************************************************************************/
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"typec", 0, 400, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"other", 1, 100, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#define BC12_I2C_ADDR PI3USB9201_I2C_ADDR_3

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_AP_IN_SLEEP_L,   POWER_SIGNAL_ACTIVE_LOW,  "AP_IN_S3_L"},
	{GPIO_PMIC_EC_RESETB,  POWER_SIGNAL_ACTIVE_HIGH, "PMIC_PWR_GOOD"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/******************************************************************************/
/* SPI devices */
const struct spi_device_t spi_devices[] = {
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

const struct pi3usb2901_config_t pi3usb2901_bc12_chips[] = {
	[0] = {
		.i2c_port = I2C_PORT_BC12,
		.i2c_addr = PI3USB9201_I2C_ADDR_3,
	},
};

/******************************************************************************/
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TCPC0,
			.addr = FUSB302_I2C_SLAVE_ADDR,
		},
		.drv = &fusb302_tcpm_drv,
	},
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
};

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_USB_C0_PD_INT_ODL))
		status |= PD_STATUS_TCPC_ALERT_0;

	return status;
}

static int force_discharge;

int board_set_active_charge_port(int charge_port)
{
	CPRINTS("New chg p%d", charge_port);

	/* ignore all request when discharge mode is on */
	if (force_discharge)
		return EC_SUCCESS;

	switch (charge_port) {
	case CHARGE_PORT_USB_C:
		/* Don't charge from a source port */
		if (board_vbus_source_enabled(charge_port))
			return -1;
		break;
	case CHARGE_PORT_NONE:
		/*
		 * To ensure the fuel gauge (max17055) is always powered
		 * even when battery is disconnected, keep VBAT rail on but
		 * set the charging current to minimum.
		 */
		charger_set_current(0);
		break;
	default:
		panic("Invalid charge port\n");
		break;
	}

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_ma = (charge_ma * 95) / 100;
	charge_set_input_current_limit(MAX(charge_ma,
			       CONFIG_CHARGER_INPUT_CURRENT), charge_mv);
}

int board_discharge_on_ac(int enable)
{
	int ret, port;

	if (enable) {
		port = CHARGE_PORT_NONE;
	} else {
		/* restore the charge port state */
		port = charge_manager_get_override();
		if (port == OVERRIDE_OFF)
			port = charge_manager_get_active_charge_port();
	}

	ret = board_set_active_charge_port(port);
	if (ret)
		return ret;
	force_discharge = enable;

	return charger_discharge_on_ac(enable);
}


int extpower_is_present(void)
{
	/*
	 * The charger will indicate VBUS presence if we're sourcing 5V,
	 * so exclude such ports.
	 */
	if (board_vbus_source_enabled(0))
		return 0;
	else
		return tcpm_get_vbus_level(0);
}

int pd_snk_is_vbus_provided(int port)
{
	/* read IT8801 GPIO EN_USBC_CHARGE_L? */
	return EC_ERROR_UNIMPLEMENTED;
}

/* dummy interrupt function for kukui */
void pogo_adc_interrupt(enum gpio_signal signal)
{
}

static void board_init(void)
{
	int data = 0x5566;
	/* If the reset cause is external, pulse PMIC force reset. */
	if (system_get_reset_flags() == RESET_FLAG_RESET_PIN) {
		gpio_set_level(GPIO_PMIC_FORCE_RESET_ODL, 0);
		msleep(100);
		gpio_set_level(GPIO_PMIC_FORCE_RESET_ODL, 1);
	}

	/* Set SPI1 PB13/14/15 pins to high speed */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xfc000000;

	/* Enable TCPC alert interrupts */
	gpio_enable_interrupt(GPIO_USB_C0_PD_INT_ODL);

#ifdef SECTION_IS_RW
	/* Enable interrupts from BMI160 sensor. */
	gpio_enable_interrupt(GPIO_ACCEL_INT_ODL);
#endif /* SECTION_IS_RW */

	/* Enable interrupt from PMIC. */
	gpio_enable_interrupt(GPIO_PMIC_EC_RESETB);
	/* set EN_USBC_CHARGE_L to 1 */
	i2c_write8(I2C_PORT_IOEXPANDER, IT8801_DEFAULT_ADDR, 0x13, 0x20);
	i2c_write8(I2C_PORT_IOEXPANDER, IT8801_DEFAULT_ADDR, 0x06, 0x00);
	i2c_read8(I2C_PORT_IOEXPANDER, IT8801_DEFAULT_ADDR, 0x13, &data);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Motion sensors */
/* Mutexes */
#ifdef SECTION_IS_RW
static struct mutex g_lid_mutex;

static struct bmi160_drv_data_t g_bmi160_data;

/* Matrix to rotate accelerometer into standard reference frame */
static const mat33_fp_t lid_standard_ref = {
	{FLOAT_TO_FP(1), 0, 0},
	{0, FLOAT_TO_FP(1), 0},
	{0, 0, FLOAT_TO_FP(1)}
};

#ifdef CONFIG_MAG_BMI160_BMM150
/* Matrix to rotate accelrator into standard reference frame */
static const mat33_fp_t mag_standard_ref = {
	{0, FLOAT_TO_FP(-1), 0},
	{FLOAT_TO_FP(-1), 0, 0},
	{0, 0, FLOAT_TO_FP(-1)}
};
#endif /* CONFIG_MAG_BMI160_BMM150 */

struct motion_sensor_t motion_sensors[] = {
	/*
	 * Note: bmi160: supports accelerometer and gyro sensor
	 * Requirement: accelerometer sensor must init before gyro sensor
	 * DO NOT change the order of the following table.
	 */
	[LID_ACCEL] = {
	 .name = "Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .rot_standard_ref = &lid_standard_ref,
	 .default_range = 4,  /* g */
	 .min_frequency = BMI160_ACCEL_MIN_FREQ,
	 .max_frequency = BMI160_ACCEL_MAX_FREQ,
	 .config = {
		 /* Enable accel in S0 */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 10000 | ROUND_UP_FLAG,
			 .ec_rate = 100 * MSEC,
		 },
	 },
	},
	[LID_GYRO] = {
	 .name = "Gyro",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .default_range = 1000, /* dps */
	 .rot_standard_ref = &lid_standard_ref,
	 .min_frequency = BMI160_GYRO_MIN_FREQ,
	 .max_frequency = BMI160_GYRO_MAX_FREQ,
	},
#ifdef CONFIG_MAG_BMI160_BMM150
	[LID_MAG] = {
	 .name = "Lid Mag",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_MAG,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .default_range = BIT(11), /* 16LSB / uT, fixed */
	 .rot_standard_ref = &mag_standard_ref,
	 .min_frequency = BMM150_MAG_MIN_FREQ,
	 .max_frequency = BMM150_MAG_MAX_FREQ(SPECIAL),
	},
#endif /* CONFIG_MAG_BMI160_BMM150 */
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

#endif /* SECTION_IS_RW */

/* FIXME */
int charger_is_sourcing_otg_power(int port)
{
	return 0;
}

