/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Boxy board-specific configuration */

#include "adc.h"
#include "board.h"
#include "button.h"
#include "cec.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "driver/cec/it83xx.h"
#include "driver/ppc/syv682x_public.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/temp_sensor/thermistor.h"
#include "driver/usb_mux/it5205.h"
#include "gpio.h"
#include "hooks.h"
#include "intc.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "switch.h"
#include "system.h"
#include "tablet_mode.h"
#include "task.h"
#include "tcpm/tcpci.h"
#include "temp_sensor.h"
#include "uart.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"

#define CPRINTUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_VSNS_PP3300_A] = { .name = "PP3300_A_PGOOD",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH0 },
	[ADC_TEMP_SENSOR_1] = { .name = "TEMP_SENSOR1",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH2 },
	[ADC_TEMP_SENSOR_2] = { .name = "TEMP_SENSOR2",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH3 },
	[ADC_VBUS_C0] = { .name = "VBUS_C0", /* 113/1113 voltage divider */
			  .factor_mul = ADC_MAX_MVOLT * 1113,
			  .factor_div = (ADC_READ_MAX + 1) * 113,
			  .shift = 0,
			  .channel = CHIP_ADC_CH4 },
	[ADC_VBUS_C1] = { .name = "VBUS_C1", /* 113/1113 voltage divider */
			  .factor_mul = ADC_MAX_MVOLT * 1113,
			  .factor_div = (ADC_READ_MAX + 1) * 113,
			  .shift = 0,
			  .channel = CHIP_ADC_CH6 },
	[ADC_TEMP_SENSOR_3] = { .name = "TEMP_SENSOR3",
				.factor_mul = ADC_MAX_MVOLT,
				.factor_div = ADC_READ_MAX + 1,
				.shift = 0,
				.channel = CHIP_ADC_CH13 },
	/* 0.01 ohm shunt resistor and 50 V/V INA -> 500 mV/A */
	[ADC_PPVAR_PWR_IN_IMON] = { .name = "ADC_PPVAR_PWR_IN_IMON",
				    .factor_mul = ADC_MAX_MVOLT * 2,
				    .factor_div = ADC_READ_MAX + 1,
				    .shift = 0,
				    .channel = CHIP_ADC_CH15 },
	/* 5/39 voltage divider */
	[ADC_SNS_PPVAR_PWR_IN] = { .name = "ADC_SNS_PPVAR_PWR_IN",
				   .factor_mul = ADC_MAX_MVOLT * 39,
				   .factor_div = (ADC_READ_MAX + 1) * 5,
				   .shift = 0,
				   .channel = CHIP_ADC_CH16 },
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* TCPCs */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		.drv = &it83xx_tcpm_drv,
	},
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		.drv = &it83xx_tcpm_drv,
	},
};

/* PPCs */
struct ppc_config_t ppc_chips[] = {
	[USBC_PORT_C0] = {
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = SYV682X_ADDR0_FLAGS,
		.drv = &syv682x_drv,
	},
	[USBC_PORT_C1] = {
		.i2c_port = I2C_PORT_USB_C1,
		.i2c_addr_flags = SYV682X_ADDR0_FLAGS,
		.drv = &syv682x_drv,
	},
};
BUILD_ASSERT(ARRAY_SIZE(ppc_chips) == USBC_PORT_COUNT);

unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

/* USB Muxes */
const struct usb_mux_chain usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.mux =
			&(const struct usb_mux){
				.usb_port = 0,
				.i2c_port = I2C_PORT_USB_C0,
				.i2c_addr_flags = IT5205_I2C_ADDR1_FLAGS,
				.driver = &it5205_usb_mux_driver,
			},
	},
	{
		.mux =
			&(const struct usb_mux){
				.usb_port = 1,
				.i2c_port = I2C_PORT_USB_C1,
				.i2c_addr_flags = IT5205_I2C_ADDR1_FLAGS,
				.driver = &it5205_usb_mux_driver,
			},
	},
};

/* USB-A ports */
const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_USB_A0_VBUS,
	GPIO_EN_USB_A1_VBUS,
};

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_LED_RED] = {
		.channel = 1,
		.flags = PWM_CONFIG_DSLEEP | PWM_CONFIG_ACTIVE_LOW,
		.freq_hz = 2400,
	},

	[PWM_CH_LED_GREEN] = {
		.channel = 2,
		.flags = PWM_CONFIG_DSLEEP | PWM_CONFIG_ACTIVE_LOW,
		.freq_hz = 2400,
	},

	[PWM_CH_LED_BLUE] = {
		.channel = 3,
		.flags = PWM_CONFIG_DSLEEP | PWM_CONFIG_ACTIVE_LOW,
		.freq_hz = 2400,
	}

};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/* Thermistors */
const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SENSOR_1] = { .name = "Memory",
			    .type = TEMP_SENSOR_TYPE_BOARD,
			    .read = get_temp_3v3_51k1_47k_4050b,
			    .idx = ADC_TEMP_SENSOR_1 },
	[TEMP_SENSOR_2] = { .name = "SoC power",
			    .type = TEMP_SENSOR_TYPE_BOARD,
			    .read = get_temp_3v3_51k1_47k_4050b,
			    .idx = ADC_TEMP_SENSOR_2 },
	[TEMP_SENSOR_3] = { .name = "Ambient",
			    .type = TEMP_SENSOR_TYPE_BOARD,
			    .read = get_temp_3v3_51k1_47k_4050b,
			    .idx = ADC_TEMP_SENSOR_3 },
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/* CEC ports */
const struct cec_config_t cec_config[] = {
	/* HDMI1 */
	[CEC_PORT_0] = {
		.drv = &it83xx_cec_drv,
		.drv_config = NULL,
		.offline_policy = cec_default_policy,
	},
};
BUILD_ASSERT(ARRAY_SIZE(cec_config) == CEC_PORT_COUNT);

void board_init(void)
{
	/* Enable PPC interrupt */
	gpio_enable_interrupt(GPIO_USB_C0_FAULT_L);
	gpio_enable_interrupt(GPIO_USB_C1_FAULT_L);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void board_reset_pd_mcu(void)
{
	/*
	 * Nothing to do.  TCPC C0 is internal.
	 */
}

__override void board_power_5v_enable(int enable)
{
	/*
	 * Nothing to do. 5V should always be enabled while in Z1 or above.
	 */
}

void board_set_charge_limit(int port, int supplier, int charge_ma, int max_ma,
			    int charge_mv)
{
	int insufficient_power =
		(charge_ma * charge_mv) <
		(CONFIG_CHARGER_MIN_POWER_MW_FOR_POWER_ON * 1000);
	/* TODO(b/259467280) blink LED on error */
	(void)insufficient_power;
}

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}

enum adc_channel board_get_vbus_adc(int port)
{
	if (port == 0)
		return ADC_VBUS_C0;
	if (port == 1)
		return ADC_VBUS_C1;
	CPRINTUSB("Unknown vbus adc port id: %d", port);
	return ADC_VBUS_C0;
}

/* Vconn control for integrated ITE TCPC */
void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/*
	 * We ignore the cc_pin and PPC vconn because polarity and PPC vconn
	 * should already be set correctly in the PPC driver via the pd
	 * state machine.
	 */
}

/******************************************************************************/
/*
 * Since boxy has no battery, it must source all of its power from either
 * USB-C or the barrel jack (preferred). Fizz operates in continuous safe
 * mode (charge_manager_leave_safe_mode() will never be called), which
 * modifies port selection as follows:
 *
 * - Dual-role / dedicated capability of the port partner is ignored.
 * - Charge ceiling on PD voltage transition is ignored.
 * - CHARGE_PORT_NONE will never be selected.
 */

int board_set_active_charge_port(int port)
{
	const int active_port = charge_manager_get_active_charge_port();
	int i;

	CPRINTUSB("Requested charge port change to %d", port);

	if (port < 0 || CHARGE_PORT_COUNT <= port)
		return EC_ERROR_INVAL;

	if (port == active_port)
		return EC_SUCCESS;

	/* Don't sink from a source port */
	if (board_vbus_source_enabled(port)) {
		CPRINTUSB("Don't sink from a source port C%d", port);
		return EC_ERROR_INVAL;
	}

	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		if (charge_manager_get_active_charge_port() != CHARGE_PORT_NONE)
			/* Change is only permitted while the system is off */
			return EC_ERROR_INVAL;

		/*
		 * Current setting is no charge port but the AP is on, so the
		 * charge manager is out of sync (probably because we're
		 * reinitializing after sysjump). Reject requests that aren't
		 * in sync with our outputs.
		 */

		/* TODO: add this part after two type-c function is finished. */
	}

	CPRINTUSB("New charger p%d", port);

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; (i < ppc_cnt) && (i < board_get_usb_pd_port_count()); i++) {
		if (i == port)
			continue;

		if (ppc_vbus_sink_enable(i, 0))
			CPRINTUSB("C%d: sink path disable failed.", i);
		else
			CPRINTUSB("C%d: sink path disable.", i);
	}

	/* Enable requested charge port. */
	if (ppc_vbus_sink_enable(port, 1)) {
		CPRINTUSB("C%d: sink path enable failed.", port);
		return EC_ERROR_UNKNOWN;
	}
	CPRINTUSB("C%d: sink path enable.", port);

	return EC_SUCCESS;
}

static void board_charge_manager_init(void)
{
	int port;

	/*
	 * Initialize all charge suppliers to 0. The charge manager waits until
	 * all ports have reported in before doing anything.
	 */
	for (int i = 0; i < CHARGE_PORT_COUNT; i++) {
		for (int j = 0; j < CHARGE_SUPPLIER_COUNT; j++)
			charge_manager_update_charge(j, i, NULL);
	}

	/* Initialize the power source supplier */
	port = pd_snk_is_vbus_provided(CHARGE_PORT_TYPEC0) ?
		       CHARGE_PORT_TYPEC0 :
		       CHARGE_PORT_TYPEC1;
	CPRINTUSB("Power source is p%d (%s)", port,
		  port == CHARGE_PORT_TYPEC0 ? "USB-C0" : "USB-C1");
	typec_set_input_current_limit(port, 3000, 5000);
}
DECLARE_HOOK(HOOK_INIT, board_charge_manager_init,
	     HOOK_PRIO_INIT_CHARGE_MANAGER + 1);

__override int extpower_is_present(void)
{
	/*
	 * There's no battery, so running this method implies we have power.
	 */
	return 1;
}

void ppc_interrupt(enum gpio_signal signal)
{
	if (signal == GPIO_USB_C0_FAULT_L)
		syv682x_interrupt(USBC_PORT_C0);
	if (signal == GPIO_USB_C1_FAULT_L)
		syv682x_interrupt(USBC_PORT_C1);
}

/* I2C Ports */
const struct i2c_port_t i2c_ports[] = {
	{ .name = "eeprom",
	  .port = I2C_PORT_EEPROM,
	  .kbps = 400,
	  .scl = GPIO_EC_I2C_EEPROM_SCL,
	  .sda = GPIO_EC_I2C_EEPROM_SDA },

	{ .name = "usbc1",
	  .port = I2C_PORT_USB_C1,
	  .kbps = 1000,
	  .scl = GPIO_EC_I2C_USB_C1_SCL,
	  .sda = GPIO_EC_I2C_USB_C1_SDA },

	{ .name = "usbc0",
	  .port = I2C_PORT_USB_C0,
	  .kbps = 1000,
	  .scl = GPIO_EC_I2C_USB_C0_SCL,
	  .sda = GPIO_EC_I2C_USB_C0_SDA },

	{ .name = "hdmi1_edid",
	  .port = I2C_PORT_HDMI1_EDID,
	  .kbps = 100,
	  .scl = GPIO_EC_I2C_HDMI1_EDID_SCL,
	  .sda = GPIO_EC_I2C_HDMI1_EDID_SDA },

	{ .name = "hdmi1_src_ddc",
	  .port = I2C_PORT_HDMI1_SRC_DDC,
	  .kbps = 100,
	  .scl = GPIO_EC_I2C_HDMI1_SRC_DDC_SCL,
	  .sda = GPIO_EC_I2C_HDMI1_SRC_DDC_SDA },
};

const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
/* Must come after other header files and interrupt handler declarations */
#include "gpio_list.h"

/*
 * Power monitoring and management.
 *
 * The overall goal is to gracefully manage the power demand so that the power
 * budgets are met without letting the system fall into power deficit (perhaps
 * causing a brownout).
 *
 * The actual system power demand is calculated from the VBUS voltage and the
 * input current (read from a shunt), averaged over 10 readings. The power
 * budget limit is from the charge manager.
 *
 * Throttles which can be applied:
 *  - Throttle Type-C power from 3A to 1.5A if sourcing.
 *
 * The SoC power will also be throttled by PSYS if the system power reaches 97%
 * of the charger rating. We prefer throttling the Type-C port over throttling
 * the SoC since this has less user impact.
 *
 * The strategy is to determine what the state of the throttles should be, and
 * to then turn throttles off or on as needed to match this.
 *
 * This function runs on demand, or every 2 ms when the CPU is up, and
 * continually monitors the power usage, applying the throttles when necessary.
 *
 * All measurements are in milliwatts.
 */

/* Throttles we can apply. */
#define THROT_TYPE_C BIT(0)

/* Power gain if Type-C port is limited. */
#define POWER_GAIN_TYPE_C 7500

/*
 * Thresholds at which to start and stop throttling Type-C. Compared against the
 * gap between current power and max power.
 *
 * PSYS will start throttling SoC power when system power reaches 97% of the
 * charger rating (e.g. 63W for a 65W charger), so the low threshold must be
 * at least 2W. We use 4W to ensure we throttle Type-C before we start
 * throttling SoC power.
 *
 * We add 5W of hysteresis to avoid switching frequently during minor power
 * variations.
 */
#define THROT_LOW_THRESHOLD 4000
#define THROT_HIGH_THRESHOLD 9000

/* Power is averaged over 20 ms, with a reading every 2 ms.  */
#define POWER_DELAY_MS 2
#define POWER_READINGS (20 / POWER_DELAY_MS)

static void power_monitor(void);
DECLARE_DEFERRED(power_monitor);

static void power_monitor(void)
{
	static uint32_t current_state;
	static uint32_t history[POWER_READINGS];
	static uint8_t index;
	int32_t delay;
	uint32_t new_state = 0, diff;

	/* If CPU is off or suspended, no need to throttle or restrict power. */
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF | CHIPSET_STATE_SUSPEND)) {
		/* Slow down monitoring, assume no throttling required. */
		delay = 20 * MSEC;

		/*
		 * Clear the first entry of the power table so that
		 * it is re-initilalised when the CPU starts.
		 */
		history[0] = 0;
	} else {
		int32_t charger_mw;

		delay = POWER_DELAY_MS * MSEC;

		/* Get current charger limit. */
		charger_mw = charge_manager_get_power_limit_uw() / 1000;

		if (charger_mw == 0) {
			/*
			 * If unknown, e.g. charge manager not initialised yet,
			 * don't change the throttles.
			 */
			new_state = current_state;
		} else {
			int32_t gap, total, power;
			int i;

			/* Read power usage. */
			power = (charge_manager_get_charger_voltage() *
				 adc_read_channel(ADC_PPVAR_PWR_IN_IMON)) /
				1000;

			/* Init power table. */
			if (history[0] == 0) {
				for (i = 0; i < POWER_READINGS; i++) {
					history[i] = power;
				}
			}

			/* Update power readings and calculate the average. */
			history[index] = power;
			index = (index + 1) % POWER_READINGS;
			total = 0;
			for (i = 0; i < POWER_READINGS; i++) {
				total += history[i];
			}
			power = total / POWER_READINGS;

			/* Calculate the gap. */
			gap = charger_mw - power;

			/*
			 * If the Type-C port is sourcing power, check whether
			 * it should be throttled.
			 */
			bool throt_type_c = false;

			if (ppc_is_sourcing_vbus(0)) {
				if (current_state & THROT_TYPE_C) {
					/*
					 * Stop throttling if the gap without
					 * throttling would be greater than the
					 * high threshold.
					 */
					throt_type_c = gap - POWER_GAIN_TYPE_C <
						       THROT_HIGH_THRESHOLD;
				} else {
					/*
					 * Start throttling if the gap is less
					 * than the low threshold.
					 */
					throt_type_c = gap <
						       THROT_LOW_THRESHOLD;
				}
			}
			if (throt_type_c)
				new_state |= THROT_TYPE_C;
		}
	}

	/* Turn the throttles on or off if they have changed. */
	diff = new_state ^ current_state;
	current_state = new_state;
	if (diff & THROT_TYPE_C) {
		enum tcpc_rp_value rp = (new_state & THROT_TYPE_C) ?
						TYPEC_RP_1A5 :
						TYPEC_RP_3A0;

		ccprints("%s: %s throttling Type-C", __func__,
			 (new_state & THROT_TYPE_C) ? "start" : "stop");

		ppc_set_vbus_source_current_limit(0, rp);
		tcpm_select_rp_value(0, rp);
		pd_update_contract(0);
	}

	hook_call_deferred(&power_monitor_data, delay);
}

/* Start power monitoring after ADCs have been initialised. */
DECLARE_HOOK(HOOK_INIT, power_monitor, HOOK_PRIO_INIT_ADC + 1);
