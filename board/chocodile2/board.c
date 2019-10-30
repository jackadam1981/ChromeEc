/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "demo.h"
#include "gpio.h"
#include "hooks.h"
#include "lcd.h"
#include "registers.h"
#include "spi.h"
#include "i2c.h"

/* Must come after other header files and GPIO interrupts*/
#include "gpio_list.h"

/******************************************************************************/

/* SPI devices */
const struct spi_device_t spi_devices[] = {
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_VCONN_VSENSE] =  {
	"VCONN_VSENSE",  3000, 4096, 0, STM32_AIN(ADC_VCONN_VSENSE)},
	[ADC_HOST_VBUS_VSENSE] = {
	"HOST_VBUS_VSENSE", 3000, 4096, 0, STM32_AIN(ADC_HOST_VBUS_VSENSE)},
	[ADC_CHARGE_VBUS_VSENSE] = {
	"CHARGE_VBUS_VSENSE", 3000, 4096, 0, STM32_AIN(ADC_CHARGE_VBUS_VSENSE)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

void tcpc_alert_clear(int port)
{
	/* Do nothing */
}
