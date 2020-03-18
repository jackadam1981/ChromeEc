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

/******************************************************************************
 * Initialize board.
 */

/*
 * Support TCA6416A and TCA6424A I2C ioexpander.
 *      TCA0 - TCA6416A
 *      TCA1 - TCA6424A
 */

#define TCA0_ADDR               0x21
#define TCA0_IN_PORT_A          0x0
#define TCA0_IN_PORT_B          0x1
#define TCA0_OUT_PORT_A         0x2
#define TCA0_OUT_PORT_B         0x3
#define TCA0_DIR_PORT_A         0x6
#define TCA0_DIR_PORT_B         0x7

#define TCA1_ADDR               0x23
#define TCA1_IN_PORT_A          0x0
#define TCA1_IN_PORT_B          0x1
#define TCA1_IN_PORT_C          0x2
#define TCA1_OUT_PORT_A         0x4
#define TCA1_OUT_PORT_B         0x5
#define TCA1_OUT_PORT_C         0x6
#define TCA1_DIR_PORT_A         0xc
#define TCA1_DIR_PORT_B         0xd
#define TCA1_DIR_PORT_C         0xe

static int dut_chg_en_state;

/* Enable all ioexpander outputs. */
void init_ioexpanders(void)
{
	/* Init TCA6416A */

	/*
	 * Write all GPIO to output 0, except for
	 * BIT-2 (USB3_A1_MUX_SEL defaults to DUT HUB.
	 */
	i2c_write8(1, TCA0_ADDR, TCA0_OUT_PORT_A, 0x04);
	i2c_write8(1, TCA0_ADDR, TCA0_OUT_PORT_B, 0x0);

	/*
	 * Write GPIO direction:
	 *	Board_ID_DET0, Board_ID_DET1, Board_ID_DET2, and DONGLE_DET are
	 *	set to inputs, all others to output.
	 */
	i2c_write8(1, TCA0_ADDR, TCA0_DIR_PORT_A, 0x00);
	i2c_write8(1, TCA0_ADDR, TCA0_DIR_PORT_B, 0xb8);

	/* Init TCA6424A */

	/* Write all GPIO to output 0 */
	i2c_write8(1, TCA1_ADDR, TCA1_OUT_PORT_A, 0x00);
	i2c_write8(1, TCA1_ADDR, TCA1_OUT_PORT_C, 0x2);

	/*
	 * Write GPIO direction:
	 *	USERVO_FAULT_L, USB3_A0_FAULT_L, USB3_A1_FAULT_L,
	 *	USB_DUTCHG_FLT_ODL, PP3300_DP_FAULT_L, DAC_BUF1_LATCH_FAULT_L,
	 *	DAC_BUF2_LATCH_FAULT_L, PP5000_SRC_SEL, and unused are
	 *	set to inputs, all others to output.
	 */
	i2c_write8(1, TCA1_ADDR, TCA1_DIR_PORT_A, 0x0);
	i2c_write8(1, TCA1_ADDR, TCA1_DIR_PORT_B, 0xff);
	i2c_write8(1, TCA1_ADDR, TCA1_DIR_PORT_C, 0x02);

	/* Clear any faults */
	read_faults();
}

/* Write a GPIO output on the TCA6416A or TCA6424A I2C ioexpander. */
void write_ioexpander(int addr, int bank, int gpio, int val)
{
	int tmp;

	/* Read output port register */
	i2c_read8(1, addr, bank, &tmp);
	if (val)
		tmp |= BIT(gpio);
	else
		tmp &= ~BIT(gpio);

	/* Write back modified output port register */
	i2c_write8(1, addr, bank, tmp);
}

/* Read a single GPIO input on the TCA6416A or TCA6424A I2C ioexpander. */
int read_ioexpander_bit(int addr, int bank, int bit)
{
	int tmp;
	int mask = 1 << bit;

	/* Read input port register */
	i2c_read8(1, addr, bank, &tmp);

	return (tmp & mask) >> bit;
}

inline void sbu_uart_sel(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 0, en);
}

inline void atmel_reset_l(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 1, en);
}

inline void sbu_flip_sel(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 2, en);
}

inline void usb3_a0_mux_sel(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 3, en);
}

inline void usb3_a0_mux_en_l(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 4, en);
}

inline void usb3_a0_pwr_en(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 5, en);
}

inline void uart_18_sel(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 6, en);
}

inline void uservo_power_en(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_A, 7, en);
}

inline void uservo_fastboot_mux_sel(enum uservo_fastboot_mux_sel_t sel)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_B, 0, sel);
}

inline void usb3_a1_pwr_en(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_B, 1, en);
}

inline void usb3_a1_mux_sel(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_B, 2, en);
}

inline int board_id_det(void)
{
	int id;

	id = read_ioexpander_bit(TCA0_ADDR, TCA0_IN_PORT_B, 5) << 2;
	id |= read_ioexpander_bit(TCA0_ADDR, TCA0_IN_PORT_B, 4) << 1;
	id |= read_ioexpander_bit(TCA0_ADDR, TCA0_IN_PORT_B, 3);

	return id;
}

inline void cmux_en(int en)
{
	write_ioexpander(TCA0_ADDR, TCA0_OUT_PORT_B, 6, en);
}

inline int dongle_det(void)
{
	return read_ioexpander_bit(TCA0_ADDR, TCA0_IN_PORT_B, 7);
}

inline void en_pp5000_alt_3p3(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 0, en);
}

inline void en_pp3300_eth(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 1, en);
}

inline void en_pp3300_dp(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 2, en);
}

inline void fault_clear_cc(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 3, en);
}

inline void en_vout_buf_cc1(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 4, en);
}

inline void en_vout_buf_cc2(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 5, en);
}

void dut_chg_en(int en)
{
	dut_chg_en_state = en;
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 6, en);
}

int get_dut_chg_en(void)
{
	return dut_chg_en_state;
}

inline void host_or_chg_ctl(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_A, 7, en);
}

inline int read_faults(void)
{
	int fault;

	fault = read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 7) << 7;
	fault |= read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 6) << 6;
	fault |= read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 5) << 5;
	fault |= read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 4) << 4;
	fault |= read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 3) << 3;
	fault |= read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 2) << 2;
	fault |= read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 1) << 1;
	fault |= read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_B, 0) << 0;

	/* Clear faults on this port */
	read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_C, 6);
	read_ioexpander_bit(TCA1_ADDR, TCA1_IN_PORT_C, 2);

	return fault;
}

inline void vbus_dischrg_en(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_C, 0, en);
}

inline void usbh_pwrdn_l(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_C, 1, en);
}

inline void tca_gpio_dbg_led_k_odl(int en)
{
	write_ioexpander(TCA1_ADDR, TCA1_OUT_PORT_C, 7, !en);
}
