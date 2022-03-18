/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "adc.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "power.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/******************************************************************************/
/* ADP_ID control */
struct adpater_id_params tio1_power[] = {
	{
	.min_voltage = 0x3FF,
	.max_voltage = 0x3FF,
	.charge_voltage = 20000,
	.charge_current = 6000,
	.watt = 120000,
	.obp95 = 0x2D3,
	.obp85 = 0x286,
	},
};

struct adpater_id_params tio2_power[] = {
	{
	.min_voltage = 0,
	.max_voltage = 0x19,
	.charge_voltage = 20000,
	.charge_current = 8500,
	.watt = 170000,
	.obp95 = 0x400,
	.obp85 = 0x394,
	},
	{
	.min_voltage = 0x19,
	.max_voltage = 0x34,
	.charge_voltage = 20000,
	.charge_current = 2250,
	.watt = 45000,
	.obp95 = 0x10F,
	.obp85 = 0x0F2,
	},
	{
	.min_voltage = 0x49,
	.max_voltage = 0x69,
	.charge_voltage = 20000,
	.charge_current = 3250,
	.watt = 65000,
	.obp95 = 0x187,
	.obp85 = 0x15E,
	},
	{
	.min_voltage = 0xC1,
	.max_voltage = 0xDD,
	.charge_voltage = 20000,
	.charge_current = 6000,
	.watt = 120000,
	.obp95 = 0x2D3,
	.obp85 = 0x286,
	},
	{
	.min_voltage = 0x8C,
	.max_voltage = 0xAE,
	.charge_voltage = 20000,
	.charge_current = 7500,
	.watt = 150000,
	.obp95 = 0x387,
	.obp85 = 0x328,
	},
	{
	.min_voltage = 0x182,
	.max_voltage = 0x199,
	.charge_voltage = 20000,
	.charge_current = 8500,
	.watt = 170000,
	.obp95 = 0x400,
	.obp85 = 0x394,
	},
};

struct adpater_id_params tiny_power[] = {
	{
	.min_voltage = 0x19,
	.max_voltage = 0x34,
	.charge_voltage = 20000,
	.charge_current = 2250,
	.watt = 45000,
	.obp95 = 0x10F,
	.obp85 = 0x0F2,
	},
	{
	.min_voltage = 0x49,
	.max_voltage = 0x69,
	.charge_voltage = 20000,
	.charge_current = 3250,
	.watt = 65000,
	.obp95 = 0x187,
	.obp85 = 0x15E,
	},
	{
	.min_voltage = 0x8C,
	.max_voltage = 0xAE,
	.charge_voltage = 20000,
	.charge_current = 4500,
	.watt = 90000,
	.obp95 = 0x21E,
	.obp85 = 0x1E5,
	},
	{
	.min_voltage = 0xC1,
	.max_voltage = 0xDD,
	.charge_voltage = 20000,
	.charge_current = 6000,
	.watt = 120000,
	.obp95 = 0x2D3,
	.obp85 = 0x286,
	},
	{
	.min_voltage = 0xED,
	.max_voltage = 0x11C,
	.charge_voltage = 20000,
	.charge_current = 6750,
	.watt = 135000,
	.obp95 = 0x32D,
	.obp85 = 0x2D7,
	},
	{
	.min_voltage = 0x135,
	.max_voltage = 0x16A,
	.charge_voltage = 20000,
	.charge_current = 7500,
	.watt = 150000,
	.obp95 = 0x387,
	.obp85 = 0x328,
	},
	{
	.min_voltage = 0x182,
	.max_voltage = 0x1BD,
	.charge_voltage = 20000,
	.charge_current = 8500,
	.watt = 170000,
	.obp95 = 0x400,
	.obp85 = 0x394,
	},
	{
	.min_voltage = 0x27B,
	.max_voltage = 0x2CB,
	.charge_voltage = 20000,
	.charge_current = 11500,
	.watt = 230000,
	.obp95 = 0x569,
	.obp85 = 0x4D7,
	},
};

struct adpater_id_params *power_type[8];
static int adp_id_value_debounce;

void obp_pinter_95(void)
{
	/* Trigger the PROCHOT */
	gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
	CPRINTF("Adapter voltage over then 95%% trigger prochot.");
}

void obp_pinter_85(void)
{
	/* Release the PROCHOT */
	gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
	CPRINTF("Adapter voltage less then 85%% release prochot.");
}

struct npcx_adc_thresh_t adc_obp_point_95 = {
	.adc_ch = ADC_PWR_IN_IMON,
	.adc_thresh_cb = obp_pinter_95,
	.thresh_assert = 3300,	/* Default */
};

struct npcx_adc_thresh_t adc_obp_point_85 = {
	.adc_ch = ADC_PWR_IN_IMON,
	.adc_thresh_cb = obp_pinter_85,
	.lower_or_higher = 1,
	.thresh_assert = 0,	/* Default */
};

static void set_up_adc_irqs(void)
{
	/* Set interrupt thresholds for the ADC. */
	CPRINTS("%s", __func__);
	npcx_adc_register_thresh_irq(NPCX_ADC_THRESH1, &adc_obp_point_95);
	npcx_adc_register_thresh_irq(NPCX_ADC_THRESH2, &adc_obp_point_85);
	npcx_set_adc_repetitive(adc_channels[ADC_PWR_IN_IMON].input_ch, 1);
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH1, 1);
	npcx_adc_thresh_int_enable(NPCX_ADC_THRESH2, 1);
}

/*       Scalar change to   Scalar change to
 *      downgrade voltage    3.3V voltage
 *               |                |
 *               |   SIO collect  |   SIO collect
 *               |   1st adapter  |   2nd adapter
 *               |   information  |   information
 *               |   |  |  |  |   |   |  |  |  |
 * -------------------------------------------------------
 *  |            |                |
 *  |---220 ms---|-----400 ms-----|
 *
 * Tiny: Twice adapter ADC values are less than 0x3FF.
 * TIO1: Twice adapter ADC values are 0x3FF.
 * TIO2: First adapter ADC value less than 0x3FF.
 *       Second adpater ADC value is 0x3FF.
 */
DECLARE_DEFERRED(adp_id_deferred);
void adp_id_deferred(void)
{
	struct charge_port_info pi = { 0 };
	int i = 0;
	int adp_type = 0;
	int adp_id_value;
	int adp_finial_adc_value;
	int power_type_len;

	adp_id_value = adc_read_channel(ADC_ADP_ID);

	if (!adp_id_value_debounce) {
		adp_id_value_debounce = adp_id_value;
		/* for delay the 400ms to get the next APD_ID value */
		hook_call_deferred(&adp_id_deferred_data, 400 * MSEC);
	} else if (adp_id_value_debounce == 0x3FF && adp_id_value == 0x3FF) {
		adp_finial_adc_value = adp_id_value;
		adp_type = TIO1;
	} else if (adp_id_value_debounce < 0x3FF && adp_id_value == 0x3FF) {
		adp_finial_adc_value = adp_id_value_debounce;
		adp_type = TIO2;
	} else if (adp_id_value_debounce < 0x3FF && adp_id_value < 0x3FF) {
		adp_finial_adc_value = adp_id_value;
		adp_type = TINY;
	}

	switch (adp_type) {
	case TIO1:
		power_type_len = sizeof(tio1_power);
		memcpy(power_type, &tio1_power, power_type_len);
		break;
	case TIO2:
		power_type_len = sizeof(tio2_power);
		memcpy(power_type, &tio2_power, power_type_len);
		break;
	case TINY:
		power_type_len = sizeof(tiny_power);
		memcpy(power_type, &tiny_power, power_type_len);
		break;
	}

	for (i = 0; (i < power_type_len) && adp_type; i++) {
		if (adp_finial_adc_value <= power_type[i]->max_voltage) {
			adc_obp_point_95.thresh_assert = power_type[i]->obp95;
			adc_obp_point_85.thresh_assert = power_type[i]->obp85;
			pi.voltage = power_type[i]->charge_voltage;
			pi.current = power_type[i]->charge_current;
			set_up_adc_irqs();
			charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				     DEDICATED_CHARGE_PORT, &pi);
			break;
		}
	}
}

static void adp_id_init(void)
{
	/* Delay 220ms to get the first ADP_ID value */
	hook_call_deferred(&adp_id_deferred_data, 220 * MSEC);
}
DECLARE_HOOK(HOOK_INIT, adp_id_init, HOOK_PRIO_DEFAULT);
