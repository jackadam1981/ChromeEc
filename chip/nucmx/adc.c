/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NUCMX-specific ADC module for Chrome EC */

#include "adc.h"
#include "adc_chip.h"
#include "atomic.h"
#include "clock.h"
#include "clock_chip.h"
#include "console.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Maximum time we allow for an ADC conversion */
#define ADC_TIMEOUT_US            SECOND
#define ADC_CLK                   2000000
#define ADC_Regular_DLY           0x011
#define ADC_Regular_ADCCNF2       0x8B07
#define ADC_Regular_GENDLY        0x0100
#define ADC_Regular_MEAST         0x0001

/* ADC conversion mode */
enum nucmx_adc_conversion_mode{
    ADC_CHN_CONVERSION_MODE   = 0,
    ADC_SCAN_CONVERSION_MODE  = 1
};

/* ADC repetitive mode */
enum nucmx_adc_repetitive_mode{
    ADC_ONE_SHOT_CONVERSION_TYPE    = 0,
    ADC_REPETITIVE_CONVERSION_TYPE  = 1
};


/* Global variables */
static task_id_t task_waiting;

/**
 * Preset ADC operation clock.
 *
 * @param   none
 * @return  none
 * @notes   changed when initial or HOOK_FREQ_CHANGE command
 */
void adc_freq_changed (void)
{
    uint8_t prescaler_divider    = 0;

    /* Set clock prescalre divider */
    prescaler_divider = (uint8_t)(clock_get_apb1_freq() / ADC_CLK);
    /* Set clock prescalre divider to ADC module*/
    if (prescaler_divider >= 1)
        prescaler_divider = prescaler_divider - 1;
    if (prescaler_divider > 0x3F)
        prescaler_divider = 0x3F;

	/* Set Core Clock Division Factor in order to obtain the ADC clock */
    NUCMX_ATCTL = (NUCMX_ATCTL &(~(((1<<6)-1)<<NUCMX_ATCTL_SCLKDIV)))|(prescaler_divider<<NUCMX_ATCTL_SCLKDIV);
}
DECLARE_HOOK(HOOK_FREQ_CHANGE, adc_freq_changed, HOOK_PRIO_DEFAULT);

/**
 * Get current voltage data of the specified channel.
 *
 * @param input_ch		nucmx input channel to read
 * @return ADC channel voltage data.(Range: 0~1023)
 */
static int get_channel_data (enum nucmx_adc_input_channel input_ch)
{
    return ((NUCMX_CHNDAT(input_ch)>>NUCMX_CHNDAT_CHDAT)&((1<<10)-1));
}

/**
 * Flush an ADC sequencer and initiate a read.
 *
 * @param   input_ch    operation channel
 * @param   timeout		preset timeout
 * @return  TRUE/FALSE  success/fail
 * @notes   set SW-triggered interrupt conversion and one-shot mode in nucmx chip
 */
static int start_single_and_wait(enum nucmx_adc_input_channel input_ch, int timeout)
{
	int event;

	task_waiting = task_get_current();
	
    /* Set ADC conversion code to SW conversion mode */
    NUCMX_ADCCNF = (NUCMX_ADCCNF &(~(((1<<2)-1)<<NUCMX_ADCCNF_ADCMD)))|(ADC_CHN_CONVERSION_MODE<<NUCMX_ADCCNF_ADCMD);

    /* Set conversion type to one-shot type */
    NUCMX_ADCCNF = (NUCMX_ADCCNF &(~(((1<<1)-1)<<NUCMX_ADCCNF_ADCRPTC)))|(ADC_ONE_SHOT_CONVERSION_TYPE<<NUCMX_ADCCNF_ADCRPTC);

    /* Update number of channel to be converted */
    NUCMX_ASCADD = (NUCMX_ASCADD &(~(((1<<5)-1)<<NUCMX_ASCADD_SADDR)))|(input_ch<<NUCMX_ASCADD_SADDR);

    /* Clear End-of-Conversion Event status */
    SET_BIT(NUCMX_ADCSTS, NUCMX_ADCSTS_EOCEV);
    
    /* Enable ADC End-of-Conversion Interrupt if applicable */
    SET_BIT(NUCMX_ADCCNF, NUCMX_ADCCNF_INTECEN);

    /* Start conversion */
    SET_BIT(NUCMX_ADCCNF, NUCMX_ADCCNF_START);

	/* Wait for interrupt */
	event = task_wait_event(timeout);

	task_waiting = TASK_ID_INVALID;

	return event != TASK_EVENT_TIMER;

}

/**
 * ADC read specific channel.
 *
 * @param   ch    operation channel
 * @return  ADC converted voltage or error message
 */
int adc_read_channel(enum adc_channel ch)
{
	const struct adc_t *adc = adc_channels + ch;
	static struct mutex adc_lock;
	int value;

	mutex_lock(&adc_lock);

    /* Enable ADC clock (bit4 mask = 0x10) */
	clock_enable_peripheral(CGC_OFFSET_ADC, CGC_ADC_MASK,
			CGC_MODE_RUN | CGC_MODE_SLEEP);

	if (start_single_and_wait(adc->input_ch, ADC_TIMEOUT_US))
	{
	    if ( (adc->input_ch == ((NUCMX_ASCADD>>NUCMX_ASCADD_SADDR)&((1<<5)-1))) && 
	        (IS_BIT_SET(NUCMX_CHNDAT(adc->input_ch), NUCMX_CHNDAT_NEW)) )
	    {
            value = get_channel_data(adc->input_ch) * adc->factor_mul /
		    adc->factor_div + adc->shift;
	    }
	    else
		    value = ADC_READ_ERROR;
    }
	else
	    value = ADC_READ_ERROR;

    /* Disable ADC clock (bit4 mask = 0x10) */
	clock_disable_peripheral(CGC_OFFSET_ADC, CGC_ADC_MASK,
		CGC_MODE_RUN | CGC_MODE_SLEEP);

	mutex_unlock(&adc_lock);
	
	return value;
}


/**
 * ADC read all channels.
 *
 * @param   data    all ADC converted voltage
 * @return  ADC     converted error message
 */
int adc_read_all_channels(int *data)
{
	int i;

	for (i = 0; i < ADC_CH_COUNT; ++i)
	{
		if (ADC_READ_ERROR == (data[i] = adc_read_channel(i)))
			return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

/**
 * ADC interrupt handler
 *
 * @param   none
 * @return  none
 * @notes   Only handle SW-triggered conversion in nucmx chip
 */
void adc_interrupt(void)
{
    if (IS_BIT_SET(NUCMX_ADCSTS, NUCMX_ADCSTS_EOCEV))
    {
        /* Disable End-of-Conversion Interrupt */
        CLEAR_BIT(NUCMX_ADCCNF, NUCMX_ADCCNF_INTECEN);

        /* Stop conversion */
        SET_BIT(NUCMX_ADCCNF, NUCMX_ADCCNF_STOP);
        
        /* Clear End-of-Conversion Event status */
        SET_BIT(NUCMX_ADCSTS, NUCMX_ADCSTS_EOCEV);
        
	    /* Wake up the task which was waiting on the interrupt, if any */
        if (task_waiting != TASK_ID_INVALID)
	        task_wake(task_waiting);
    }
}
DECLARE_IRQ(NUCMX_IRQ_ADC, adc_interrupt, 2);

/**
 * ADC Console commands
 *
 * @param   concole argc
 * @param   concole argv
 * @return  error message
 * @notes   Not support temperature sensor in nucmx chip
 */
#ifdef CONFIG_CMD_ECTEMP
static int command_ectemp(int argc, char **argv)
{
/* TODO: (Benson_TBD_12) not sure formula between temperature and ADC */
	int t = adc_read_channel(ADC_CH_0);
	ccprintf("EC temperature is %d K = %d C\n", t, K_TO_C(t));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ectemp, command_ectemp,
			NULL,
			"Print EC temperature",
			NULL);
#endif

/**
 * ADC initial.
 *
 * @param none
 * @return none
 */
static void adc_init(void)
{
	/* Configure pins from GPIOs to ADCs */
	gpio_config_module(MODULE_ADC, 1);

    /* Enable ADC */
    SET_BIT(NUCMX_ADCCNF, NUCMX_ADCCNF_ADCEN);

	/* Set Core Clock Division Factor in order to obtain the ADC clock */
    adc_freq_changed();

    /* Set regular speed */
    NUCMX_ATCTL = (NUCMX_ATCTL &(~(((1<<3)-1)<<NUCMX_ATCTL_DLY)))|((ADC_Regular_DLY - 1)<<3);
    NUCMX_ADCCNF2 = ADC_Regular_ADCCNF2;
    NUCMX_GENDLY = ADC_Regular_GENDLY;
    NUCMX_MEAST = ADC_Regular_MEAST;
    
	task_waiting = TASK_ID_INVALID;

	/* Enable IRQs */
	task_enable_irq(NUCMX_IRQ_ADC);

}
DECLARE_HOOK(HOOK_INIT, adc_init, HOOK_PRIO_DEFAULT);
