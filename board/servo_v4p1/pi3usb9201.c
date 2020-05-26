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
#include "pi3usb9201.h"
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

#define PI3USB9201_ADDR	0x5f

inline void init_pi3usb9201(void)
{
	/*
	 * Write 0x08 (Client mode detection and Enable USB switch auto ON) to
	 *	control Reg 2
	 * Write 0x08 (Client Mode) to Control Reg 1
	 */
	i2c_write16(1, PI3USB9201_ADDR, CTRL_REG1, 0x0808);
}

inline void write_pi3usb9201(enum pi3usb9201_reg_t reg,
					enum pi3usb9201_dat_t dat)
{
	i2c_write8(1, PI3USB9201_ADDR, reg, dat);
}

inline uint8_t read_pi3usb9201(enum pi3usb9201_reg_t reg)
{
	int tmp;

	i2c_read8(1, PI3USB9201_ADDR, reg, &tmp);

	return tmp;
}
