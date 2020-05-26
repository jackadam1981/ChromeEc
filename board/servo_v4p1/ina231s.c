/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "ina2xx.h"
#include "ioexpanders.h"
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "update_fw.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb_gpio.h"
#include "usb_i2c.h"
#include "usb_pd.h"
#include "usb_spi.h"
#include "usb-stream.h"
#include "util.h"

#define PP_DUT_IDX	0
#define PP_CHG_IDX	1
#define SR_CHG_IDX	2

void init_ina231s(void)
{
	/* Calibrate INA0 (PP DUT) with 1mA/LSB scale */
	ina2xx_init(PP_DUT_IDX, 0x8000, INA2XX_CALIB_1MA(5 /*mOhm*/));

	/* Calibrate INA1 (PP CHG) with 1mA/LSB scale */
	ina2xx_init(PP_CHG_IDX, 0x8000, INA2XX_CALIB_1MA(5 /*mOhm*/));

	/* Calibrate INA2 (SR CHG) with 1mA/LSB scale*/
	ina2xx_init(SR_CHG_IDX, 0x8000, INA2XX_CALIB_1MA(5 /*mOhm*/));
}

/* Return bus voltage in milliVolts */
int pp_dut_voltage(void)
{
	return ina2xx_get_voltage(PP_DUT_IDX);
}

/* Return current in milliAmps */
int pp_dut_current(void)
{
	return ina2xx_get_current(PP_DUT_IDX);
}

/* Return power in milliWatts */
int pp_dut_power(void)
{
	return ina2xx_get_power(PP_DUT_IDX);
}

/* Return bus voltage in milliVolts */
int pp_chg_voltage(void)
{
	return ina2xx_get_voltage(PP_CHG_IDX);
}

/* Return current in milliAmps */
int pp_chg_current(void)
{
	return ina2xx_get_current(PP_CHG_IDX);
}

/* Return power in milliWatts */
int pp_chg_power(void)
{
	return ina2xx_get_power(PP_CHG_IDX);
}

/* Return bus voltage in milliVolts */
int sr_chg_voltage(void)
{
	return ina2xx_get_voltage(SR_CHG_IDX);
}

/* Return current in milliAmps */
int sr_chg_current(void)
{
	return ina2xx_get_current(SR_CHG_IDX);
}

/* Return power in milliWatts */
int sr_chg_power(void)
{
	return ina2xx_get_power(SR_CHG_IDX);
}
