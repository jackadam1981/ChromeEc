/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "board.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/* Acceptable margin between requested VBUS and measured value */
#define MARGIN_MV 400 /* mV */

#define PDO_FIXED_FLAGS (PDO_FIXED_EXTERNAL)

/* we are not acting as a source */
const uint32_t pd_src_pdo[] = {
		PDO_FIXED(5000,   500, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);

/* Fake PDOs : we just want our pre-defined voltages */
const uint32_t pd_snk_pdo[] = {
		PDO_FIXED(5000,   500, PDO_FIXED_FLAGS),
		PDO_FIXED(12000,  500, PDO_FIXED_FLAGS),
		PDO_FIXED(20000,  500, PDO_FIXED_FLAGS),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
	/* No battery, nothing to do */
	return;
}

int pd_is_valid_input_voltage(int mv)
{
	return 1;
}

int pd_snk_is_vbus_provided(int port)
{
	/* VBUS_WAKE is broken (not detecting 5V), use the ADC instead */
	return adc_read_channel(ADC_CH_VBUS_SENSE) > 4000;
}

void pd_transition_voltage(int idx)
{
	/* No operation: sink only */
}

int pd_set_power_supply_ready(int port)
{
	/* Never acting as a source */
	return EC_ERROR_INVAL;
}

void pd_power_supply_reset(int port)
{
}

int pd_board_checks(void)
{
	static int blinking;
	int vbus;
	int led5 = 0, led12 = 0, led20 = 0;
	unsigned select_mv = pd_get_max_voltage();

	/* LED blinking state for the default indicator */
	blinking = (blinking + 1) & 3;

	vbus = adc_read_channel(ADC_CH_VBUS_SENSE);

	if (select_mv > 0) {
		/* is current VBUS voltage matching the request ? */
		int diff = vbus - select_mv;
		int correct = (diff < MARGIN_MV) && (diff > -MARGIN_MV);
		/*
		 * turn on the LED if the voltage is correct
		 * or we are in on-period of the duty cycle.
		 */
		int led_value = correct || !blinking;
		/* decide which LED is used */
		if (led_value) {
			led5  = (current_cap + 1) & 1;
			led12 = ((current_cap + 1) >> 1) & 1;
			led20 = ((current_cap + 1) >> 2) & 1;
		}
	}
	/* switch  LEDs */
	gpio_set_level(GPIO_LED_PP5000, led5);
	gpio_set_level(GPIO_LED_PP12000, led12);
	gpio_set_level(GPIO_LED_PP20000, led20);

	return EC_SUCCESS;
}

int pd_check_power_swap(int port)
{
	/* Always refuse power swap */
	return 0;
}

int pd_check_data_swap(int port, int data_role)
{
	/* Always allow data swap */
	return 1;
}

void pd_execute_data_swap(int port, int data_role)
{
	gpio_set_level(GPIO_LED_CRC, data_role == PD_ROLE_DFP);
}

void pd_check_pr_role(int port, int pr_role, int partner_pr_swap)
{
}

void pd_check_dr_role(int port, int dr_role, int partner_dr_swap)
{
	gpio_set_level(GPIO_LED_CRC, dr_role == PD_ROLE_DFP);
}

int pd_custom_vdm(int port, int cnt, uint32_t *payload,
		  uint32_t **rpayload)
{
	return 0;
}
