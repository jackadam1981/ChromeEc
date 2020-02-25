/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT83xx ADC module for Chrome EC */

#include "adc.h"
#include "adc_chip.h"
#include "clock.h"
#include "console.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Global variables */
static struct mutex adc_lock;
static int adc_init_done;
static volatile task_id_t task_waiting;

/* Data structure of ADC channel control registers. */
const struct adc_ctrl_t adc_ctrl_regs[] = {
	{&IT83XX_ADC_VCH0CTL, &IT83XX_ADC_VCH0DATM, &IT83XX_ADC_VCH0DATL,
		&IT83XX_GPIO_GPCRI0},
	{&IT83XX_ADC_VCH1CTL, &IT83XX_ADC_VCH1DATM, &IT83XX_ADC_VCH1DATL,
		&IT83XX_GPIO_GPCRI1},
	{&IT83XX_ADC_VCH2CTL, &IT83XX_ADC_VCH2DATM, &IT83XX_ADC_VCH2DATL,
		&IT83XX_GPIO_GPCRI2},
	{&IT83XX_ADC_VCH3CTL, &IT83XX_ADC_VCH3DATM, &IT83XX_ADC_VCH3DATL,
		&IT83XX_GPIO_GPCRI3},
	{&IT83XX_ADC_VCH4CTL, &IT83XX_ADC_VCH4DATM, &IT83XX_ADC_VCH4DATL,
		&IT83XX_GPIO_GPCRI4},
	{&IT83XX_ADC_VCH5CTL, &IT83XX_ADC_VCH5DATM, &IT83XX_ADC_VCH5DATL,
		&IT83XX_GPIO_GPCRI5},
	{&IT83XX_ADC_VCH6CTL, &IT83XX_ADC_VCH6DATM, &IT83XX_ADC_VCH6DATL,
		&IT83XX_GPIO_GPCRI6},
	{&IT83XX_ADC_VCH7CTL, &IT83XX_ADC_VCH7DATM, &IT83XX_ADC_VCH7DATL,
		&IT83XX_GPIO_GPCRI7},
#ifdef IT83XX_CHIP_ADC_PIN_ORDER_CHANGE
	{&IT83XX_ADC_VCH13CTL, &IT83XX_ADC_VCH13DATM, &IT83XX_ADC_VCH13DATL,
		&IT83XX_GPIO_GPCRL1},
	{&IT83XX_ADC_VCH14CTL, &IT83XX_ADC_VCH14DATM, &IT83XX_ADC_VCH14DATL,
		&IT83XX_GPIO_GPCRL2},
	{&IT83XX_ADC_VCH15CTL, &IT83XX_ADC_VCH15DATM, &IT83XX_ADC_VCH15DATL,
		&IT83XX_GPIO_GPCRL3},
	{&IT83XX_ADC_VCH16CTL, &IT83XX_ADC_VCH16DATM, &IT83XX_ADC_VCH16DATL,
		&IT83XX_GPIO_GPCRL0},
#else
	{&IT83XX_ADC_VCH13CTL, &IT83XX_ADC_VCH13DATM, &IT83XX_ADC_VCH13DATL,
		&IT83XX_GPIO_GPCRL0},
	{&IT83XX_ADC_VCH14CTL, &IT83XX_ADC_VCH14DATM, &IT83XX_ADC_VCH14DATL,
		&IT83XX_GPIO_GPCRL1},
	{&IT83XX_ADC_VCH15CTL, &IT83XX_ADC_VCH15DATM, &IT83XX_ADC_VCH15DATL,
		&IT83XX_GPIO_GPCRL2},
	{&IT83XX_ADC_VCH16CTL, &IT83XX_ADC_VCH16DATM, &IT83XX_ADC_VCH16DATL,
		&IT83XX_GPIO_GPCRL3},
#endif
};
BUILD_ASSERT(ARRAY_SIZE(adc_ctrl_regs) == CHIP_ADC_COUNT);

/* Data structure of voltage comparator channel control registers. */
const struct vcmp_ctrl_t vcmp_ctrl_regs[] = {
	{&IT83XX_ADC_VCMP0CTL, &IT83XX_ADC_CMP0THRDATM, &IT83XX_ADC_CMP0THRDATL,
		&IT83XX_GPIO_GPCRJ3},
	{&IT83XX_ADC_VCMP1CTL, &IT83XX_ADC_CMP1THRDATM, &IT83XX_ADC_CMP1THRDATL,
		&IT83XX_GPIO_GPCRJ4},
	{&IT83XX_ADC_VCMP2CTL, &IT83XX_ADC_CMP2THRDATM, &IT83XX_ADC_CMP2THRDATL,
		&IT83XX_GPIO_GPCRJ5},
	{&IT83XX_ADC_VCMP3CTL, &IT83XX_ADC_CMP3THRDATM, &IT83XX_ADC_CMP3THRDATL,
		&IT83XX_GPIO_GPCRL5},
	{&IT83XX_ADC_VCMP4CTL, &IT83XX_ADC_CMP4THRDATM, &IT83XX_ADC_CMP4THRDATL,
		&IT83XX_GPIO_GPCRL6},
	{&IT83XX_ADC_VCMP5CTL, &IT83XX_ADC_CMP5THRDATM, &IT83XX_ADC_CMP5THRDATL,
		&IT83XX_GPIO_GPCRL7},
};
BUILD_ASSERT(ARRAY_SIZE(vcmp_ctrl_regs) == CHIP_VCMP_COUNT);

static void adc_enable_channel(int ch)
{
	if (ch < CHIP_ADC_CH4)
		/*
		 * for channel 0, 1, 2, and 3
		 * bit4 ~ bit0 : indicates voltage channel[x]
		 *               input is selected for measurement (enable)
		 * bit5 : data valid interrupt of adc.
		 * bit7 : W/C data valid flag
		 */
		*adc_ctrl_regs[ch].adc_ctrl = 0xa0 + ch;
	else
		/*
		 * for channel 4 ~ 7 and 13 ~ 16.
		 * bit4 : voltage channel enable (ch 4~7 and 13 ~ 16)
		 * bit5 : data valid interrupt of adc.
		 * bit7 : W/C data valid flag
		 */
		*adc_ctrl_regs[ch].adc_ctrl = 0xb0;

	task_clear_pending_irq(IT83XX_IRQ_ADC);
	task_enable_irq(IT83XX_IRQ_ADC);

	/* bit 0 : adc module enable */
	IT83XX_ADC_ADCCFG |= 0x01;
}

static void adc_disable_channel(int ch)
{
	if (ch < CHIP_ADC_CH4)
		/*
		 * for channel 0, 1, 2, and 3
		 * bit4 ~ bit0 : indicates voltage channel[x]
		 *               input is selected for measurement (disable)
		 * bit 7 : W/C data valid flag
		 */
		*adc_ctrl_regs[ch].adc_ctrl = 0x9F;
	else
		/*
		 * for channel 4 ~ 7 and 13 ~ 16.
		 * bit4 : voltage channel disable (ch 4~7 and 13 ~ 16)
		 * bit7 : W/C data valid flag
		 */
		*adc_ctrl_regs[ch].adc_ctrl = 0x80;

	/* bit 0 : adc module disable */
	IT83XX_ADC_ADCCFG &= ~0x01;

	task_disable_irq(IT83XX_IRQ_ADC);
}

static int adc_data_valid(enum chip_adc_channel adc_ch)
{
	return (adc_ch <= CHIP_ADC_CH7) ?
		(IT83XX_ADC_ADCDVSTS & BIT(adc_ch)) :
		(IT83XX_ADC_ADCDVSTS2 & (1 << (adc_ch - CHIP_ADC_CH13)));
}

int adc_read_channel(enum adc_channel ch)
{
	uint32_t events;
	/* voltage 0 ~ 3v = adc data register raw data 0 ~ 3FFh (10-bit ) */
	uint16_t adc_raw_data;
	int valid = 0;
	int adc_ch, mv;

	if (!adc_init_done)
		return ADC_READ_ERROR;

	mutex_lock(&adc_lock);

	task_waiting = task_get_current();
	adc_ch = adc_channels[ch].channel;
	adc_enable_channel(adc_ch);
	/* Wait for interrupt */
	events = task_wait_event_mask(TASK_EVENT_ADC_DONE, ADC_TIMEOUT_US);
	task_waiting = TASK_ID_INVALID;

	if (events & TASK_EVENT_ADC_DONE) {
		/* data valid of adc channel[x] */
		if (adc_data_valid(adc_ch)) {
			/* read adc raw data msb and lsb */
			adc_raw_data = (*adc_ctrl_regs[adc_ch].adc_datm << 8) +
				*adc_ctrl_regs[adc_ch].adc_datl;

			/* W/C data valid flag */
			if (adc_ch <= CHIP_ADC_CH7)
				IT83XX_ADC_ADCDVSTS = BIT(adc_ch);
			else
				IT83XX_ADC_ADCDVSTS2 =
					(1 << (adc_ch - CHIP_ADC_CH13));

			mv = adc_raw_data * adc_channels[ch].factor_mul /
				adc_channels[ch].factor_div +
				adc_channels[ch].shift;
			valid = 1;
		}
	}
	adc_disable_channel(adc_ch);

	mutex_unlock(&adc_lock);

	return valid ? mv : ADC_READ_ERROR;
}

void adc_interrupt(void)
{
	/*
	 * Clear the interrupt status.
	 *
	 * NOTE:
	 * The ADC interrupt pending flag won't be cleared unless
	 * we W/C data valid flag of ADC module as well.
	 * (If interrupt type setting is high-level triggered)
	 */
	task_clear_pending_irq(IT83XX_IRQ_ADC);
	/*
	 * We disable ADC interrupt here, because current setting of
	 * interrupt type is high-level triggered.
	 * The interrupt will be triggered again and again until
	 * we W/C data valid flag if we don't disable it.
	 */
	task_disable_irq(IT83XX_IRQ_ADC);
	/* Wake up the task which was waiting for the interrupt */
	if (task_waiting != TASK_ID_INVALID)
		task_set_event(task_waiting, TASK_EVENT_ADC_DONE, 0);
}

/* Set voltage comparator condition */
void set_voltage_comparator_condition(int index, int ch)
{
	int temp;

	/* Threshold volt = 3v * CMP0THRDAT[9:0] / 1024 */
	temp = vcmp_channels[index].threshold *
		vcmp_channels[index].comparator_resolution / 3;
	*vcmp_ctrl_regs[ch].vcmp_datl = (uint8_t)temp;
	*vcmp_ctrl_regs[ch].vcmp_datm = (uint8_t)(temp >> 8);


	/* Select which ADC channel to input voltage into comparator */
	*vcmp_ctrl_regs[ch].vcmp_ctrl |= vcmp_channels[index].adcx_channel;

	/* Select comparator trigger mode: greater/less equal than CMP0THRDAT[9:0] */
	if (vcmp_channels[index].greater_or_less_equal)
		*vcmp_ctrl_regs[ch].vcmp_ctrl |= ADC_VCMP_GREATER_THRESHOLD;
	else
		*vcmp_ctrl_regs[ch].vcmp_ctrl &= ~ADC_VCMP_GREATER_THRESHOLD;

	/* Select comparator trigger mode: edge/level trigger */
	if (vcmp_channels[index].trigger_level)
		*vcmp_ctrl_regs[ch].vcmp_ctrl |= ADC_VCMP_EDGE_TRIGGER;
	else
		*vcmp_ctrl_regs[ch].vcmp_ctrl &= ~ADC_VCMP_EDGE_TRIGGER;

	/* Select comparator0, 1, 2 scan period. 3, 4, 5 ?, need modify?(add in structure) */
	IT83XX_ADC_VCMPSCP = ADC_VCMP_SCAN_PERIOD_1MS;

	/* Enable comparator0 output to GPJ3 */
	//IT83XX_GPIO_GRC15 |= BIT(0); //for FRS

	/* Select GPJ3 alternate mode */
	//IT83XX_GPIO_GPCRJ3 = 0x00; //for FRS
}

void volt_comp_interrupt(void)
{
	int ch;
	int index;

	/* Record triggered voltage comparator channel, trigger multi vcmp at same time? */
	ch = IT83XX_ADC_VCMPSTS & 0x07;
	ch |= (IT83XX_ADC_VCMPSTS2 & 0x07) << 3;

	/* Stop voltage comparator */
	*vcmp_ctrl_regs[ch].vcmp_ctrl &= ~ADC_VCMP_CMPEN;

	/* Write clear voltage comparator interrupt status */
	if (ch <= 2)
		IT83XX_ADC_VCMPSTS = BIT(ch);
	else
		IT83XX_ADC_VCMPSTS2 = BIT(ch - 3);

	/* Clear MCU IRQ status */
	task_clear_pending_irq(IT83XX_IRQ_V_COMP);

	/* Find channel index in vcmp_channels[] */
	for (index = 0; index < VCMP_CH_COUNT; index++) {
		if (ch == vcmp_channels[index].vcmpx_channel)
			break;
	}

	/* Set voltage comparator condition */
	if (ch == CHIP_VCMP_CH0) {
		if (*vcmp_ctrl_regs[ch].vcmp_ctrl == ADC_VCMP_GREATER_THRESHOLD) {
			//vcmp_channels[index].threshold = 200;
			//vcmp_channels[index].greater_or_less_equal = LESS_EQUAL_THRESHOLD;
			ccprints("Cmp0 INT detect High, switch detect Low");
		} else {
			//vcmp_channels[index].threshold = 2800;
			//vcmp_channels[index].greater_or_less_equal = GREATER_THRESHOLD;
			ccprints("Cmp0 INT detect Low, switch detect High");
		}
		set_voltage_comparator_condition(index, ch);
	}

	/* Start voltage comparator */
	*vcmp_ctrl_regs[ch].vcmp_ctrl |= ADC_VCMP_CMPEN;
}

/*
 * ADC analog accuracy initialization (only once after VSTBY power on)
 *
 * Write 1 to this bit and write 0 to this bit immediately once and
 * only once during the firmware initialization and do not write 1 again
 * after initialization since IT83xx takes much power consumption
 * if this bit is set as 1
 */
static void adc_accuracy_initialization(void)
{
	/* bit3 : start adc accuracy initialization */
	IT83XX_ADC_ADCSTS |= 0x08;
	/* short delay for adc accuracy initialization */
	IT83XX_GCTRL_WNCKR = 0;
	/* bit3 : stop adc accuracy initialization */
	IT83XX_ADC_ADCSTS &= ~0x08;
}

/* Voltage comparator initialization */
static void voltage_comparator_init(void)
{
	int ch;
	int index;

	/* If needn't init voltage comparator, then return. */
	if (VCMP_CH_COUNT == 0)
		return;

	for (index = 0; index < VCMP_CH_COUNT; index++) {
		/* Find voltage comparator channel */
		ch = vcmp_channels[index].vcmpx_channel;

		/* Set voltage comparator condition */
		set_voltage_comparator_condition(index, ch);

		/* Write clear voltage comparator interrupt status */
		if (index <= 2)
			IT83XX_ADC_VCMPSTS = BIT(index);
		else
			IT83XX_ADC_VCMPSTS2 = BIT(index - 3);

		/* Enable comparator interrupt */
		*vcmp_ctrl_regs[ch].vcmp_ctrl |= ADC_VCMP_CMPINTEN;

		/* Clear GPIO status, when? waiting ADC not trigger */

		/* Clear MCU IRQ status */
		task_clear_pending_irq(IT83XX_IRQ_V_COMP);

		task_enable_irq(IT83XX_IRQ_V_COMP);
		/* Start voltage comparator */
		*vcmp_ctrl_regs[ch].vcmp_ctrl |= ADC_VCMP_CMPEN;
	}
}

/* ADC module Initialization */
static void adc_init(void)
{
	int index;
	int ch;

	/* ADC analog accuracy initialization */
	adc_accuracy_initialization();

	for (index = 0; index < ADC_CH_COUNT; index++) {
		ch = adc_channels[index].channel;

		/* enable adc channel[x] function pin */
		*adc_ctrl_regs[ch].adc_pin_ctrl = 0x00;
	}
	/*
	 * bit7@ADCSTS     : ADCCTS1 = 0
	 * bit5@ADCCFG     : ADCCTS0 = 0
	 * bit[5-0]@ADCCTL : SCLKDIV
	 * The ADC channel conversion time is 30.8*(SCLKDIV+1) us.
	 * (Current setting is 61.6us)
	 *
	 * NOTE: A sample time delay (60us) also need to be included in
	 * conversion time, so the final result is ~= 121.6us.
	 */
	IT83XX_ADC_ADCSTS &= ~BIT(7);
	IT83XX_ADC_ADCCFG &= ~BIT(5);
	IT83XX_ADC_ADCCTL = 1;

	task_waiting = TASK_ID_INVALID;
	/* disable adc interrupt */
	task_disable_irq(IT83XX_IRQ_ADC);

	voltage_comparator_init();

	adc_init_done = 1;
}
DECLARE_HOOK(HOOK_INIT, adc_init, HOOK_PRIO_INIT_ADC);
