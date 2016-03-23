/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "backlight.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "driver/tcpm/fusb302.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "shi_chip.h"
#include "switch.h"
#include "timer.h"
#include "thermal.h"
#include "usb_charge.h"
#include "usb_pd_tcpm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

void tcpc_alert_event(enum gpio_signal signal)
{
	/* Exchange status with TCPCs */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
}

void s0s3_toggle_interrupt(enum gpio_signal signal)
{
	/* Exchange status with TCPCs */
	ccprintf("TOGGLE TO %d\n", gpio_get_level(GPIO_AP_EC_S3_S0_L));
}

#include "gpio_list.h"

/******************************************************************************/
/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	[ADC_BOARD_ID] = {
		"BOARD_ID", NPCX_ADC_CH0, ADC_MAX_VOLT, ADC_READ_MAX+1, 0 },
	[ADC_PP900_AP] = {
		"PP900_AP", NPCX_ADC_CH1, ADC_MAX_VOLT, ADC_READ_MAX+1, 0 },
	[ADC_PP1200_LPDDR] = {
		"PP1200_LPDDR", NPCX_ADC_CH2, ADC_MAX_VOLT, ADC_READ_MAX+1, 0 },
	[ADC_PPVAR_CLOGIC] = {
		"PPVAR_CLOGIC",
		NPCX_ADC_CH3, ADC_MAX_VOLT, ADC_READ_MAX+1, 0 },
	[ADC_PPVAR_LOGIC] = {
		"PPVAR_LOGIC", NPCX_ADC_CH4, ADC_MAX_VOLT, ADC_READ_MAX+1, 0 },
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/******************************************************************************/
/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_KBLIGHT] = { 2, 0, 10000 },
	[PWM_CH_LED] =     { 3, PWM_CONFIG_DSLEEP_CLK, 100 },
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/******************************************************************************/
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"tcpc0",   NPCX_I2C_PORT0_0, 1000, GPIO_I2C0_SCL0, GPIO_I2C0_SDA0},
	{"tcpc1",   NPCX_I2C_PORT0_1, 1000, GPIO_I2C0_SCL1, GPIO_I2C0_SDA1},
	{"sensors", NPCX_I2C_PORT1,   1000, GPIO_I2C1_SCL,  GPIO_I2C1_SDA},
	{"charger", NPCX_I2C_PORT2,    400, GPIO_I2C2_SCL,  GPIO_I2C2_SDA},
	{"battery", NPCX_I2C_PORT3,    100, GPIO_I2C3_SCL,  GPIO_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/******************************************************************************/
/* Wake-up pins for hibernate */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/******************************************************************************/
/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 40,
	.debounce_down_us = 6 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 1500,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = SECOND,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xf6, 0x55, 0xfa, 0xc8  /* full set */
	},
};

const struct button_config buttons[CONFIG_BUTTON_COUNT] = {
	{"Volume Down", KEYBOARD_BUTTON_VOLUME_DOWN, GPIO_VOLUME_DOWN_L,
	 30 * MSEC, 0},
	{"Volume Up", KEYBOARD_BUTTON_VOLUME_UP, GPIO_VOLUME_UP_L,
	 30 * MSEC, 0},
};

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	{I2C_PORT_TCPC0, FUSB302_I2C_SLAVE_ADDR},
	{I2C_PORT_TCPC1, FUSB302_I2C_SLAVE_ADDR},
};

void board_reset_pd_mcu(void)
{
}

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_USB_C0_PD_INT_L))
		status |= PD_STATUS_TCPC_ALERT_0;
	if (!gpio_get_level(GPIO_USB_C1_PD_INT_L))
		status |= PD_STATUS_TCPC_ALERT_1;

	return status;
}

int charger_select_input_port(int port);

int board_set_active_charge_port(int charge_port)
{
	CPRINTS("New chg p%d", charge_port);
	return charger_select_input_port(charge_port);
}

void board_set_charge_limit(int charge_ma)
{
	charge_set_input_current_limit(MAX(charge_ma,
				       CONFIG_CHARGER_INPUT_CURRENT));

}

int charger_get_extpower_present(void);

int extpower_is_present(void)
{
	return charger_get_extpower_present();
}

struct power_sequence_info {
	enum gpio_signal gpio;
	int level;
	int stage;
};
static const struct power_sequence_info power_control_outputs[] = {
        { GPIO_AP_CORE_EN, 1, 7 },
        { GPIO_LPDDR_PWR_EN, 1 , 5},
        { GPIO_PPVAR_CLOGIC_EN, 1, 3 },
        { GPIO_PPVAR_LOGIC_EN, 1 , 1},

        { GPIO_PP900_AP_EN, 1 , 1},
        { GPIO_PP900_DDRPLL_EN, 1, 2 },
        { GPIO_PP900_PLL_EN, 1 , 2},
        { GPIO_PP900_PMU_EN, 1 , 2},
        { GPIO_PP900_USB_EN, 1, 4 },
        { GPIO_PP900_PCIE_EN, 1, 2 },

        { GPIO_PP1200_HSIC_EN, 1, 6 },

        { GPIO_PP1800_SENSOR_EN_L, 0, 12},
        { GPIO_PP1800_LID_EN_L, 0, 12 },
        { GPIO_PP1800_PMU_EN_L, 0, 4 },
        { GPIO_PP1800_AP_AVDD_EN_L, 0, 4 },
        { GPIO_PP1800_USB_EN_L, 0 , 4},
        { GPIO_PP1800_S0_EN_L, 0, 9 },
        { GPIO_PP1800_SIXAXIS_EN_L, 0, 6 },

        { GPIO_PP3300_TRACKPAD_EN_L, 0, 6 },
        { GPIO_PP3300_USB_EN_L, 0, 8 },
        { GPIO_PP3300_S0_EN_L, 0, 10 },

        { GPIO_PP5000_EN, 1, 5 },

        { GPIO_SYS_RST, 1, 11 },
};


static int command_pwrseq(int argc, char **argv)
{
	const struct power_sequence_info *output_signal;
	int i;
	int seq;
	char *e;

	seq = strtoi(argv[1], &e, 10);
	ccprintf("Stage %d\n", seq);
        for (i = 0; i < ARRAY_SIZE(power_control_outputs); ++i) {
                output_signal = &power_control_outputs[i];
		if (output_signal->stage  == seq) {
                	gpio_set_level(output_signal->gpio, output_signal->level);
		}
        }

        return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pwrseq, command_pwrseq,
                        "[on|off]",
                        "Get or set fast charging profile",
                        NULL);

static void board_init(void)
{
	struct charge_port_info charge_none;
	int i;

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

