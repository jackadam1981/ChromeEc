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
#include "ioexpanders.h"
#include "pathsel.h"
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

void init_pathsel(void)
{
	/* Connect TypeA port to DUT hub */
	usb3_a0_to_dut();
	usb3_a0_mux_sel(1);
	/* Connect data lines */
	usb3_a0_mux_en_l(0);
	/* Enable power */
	usb3_a0_pwr_en(1);

	/* Connect TypeA port to DUT hub */
	usb3_a1_to_dut();
	usb3_a1_mux_sel(1);
	/* Connect data lines */
	gpio_set_level(GPIO_USB3_A1_MUX_EN_L, 0);
	/* Enable power */
	usb3_a1_pwr_en(1);
}

void usb3_a0_to_dut(void)
{
	usb3_a0_mux_sel(1);
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_SEL, 1);
}

void usb3_a1_to_dut(void)
{
	usb3_a1_mux_sel(1);
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_SEL, 1);
}

void usb3_a0_to_host(void)
{
	usb3_a0_mux_sel(0);
}

void usb3_a1_to_host(void)
{
	usb3_a1_mux_sel(0);
}

void host_to_dut(void)
{
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_SEL, 0);
	uservo_fastboot_mux_sel(MUX_SEL_FASTBOOT);
}

void uservo_to_host(void)
{
	uservo_fastboot_mux_sel(MUX_SEL_USERVO);
}
