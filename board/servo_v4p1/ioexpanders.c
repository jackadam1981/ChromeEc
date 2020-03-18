/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "ioexpanders.h"
#include "tca6416a.h"
#include "tca6424a.h"

/******************************************************************************
 * Initialize IOExpanders.
 */

static int dut_chg_en_state;

/* Enable all ioexpander outputs. */
void init_ioexpanders(void)
{
	/* Configure TCA6416A */

	/*
	 * Write all GPIO to output 0, except for
	 * BIT-2 (USB3_A1_MUX_SEL defaults to DUT HUB.
	 */
	tca6416a_write_byte(1, TCA6416A_OUT_PORT_A, 0x04);
	tca6416a_write_byte(1, TCA6416A_OUT_PORT_B, 0);

	/*
	 * Write GPIO direction:
	 *	Board_ID_DET0, Board_ID_DET1, Board_ID_DET2, and DONGLE_DET are
	 *	set to inputs, all others to output.
	 */
	tca6416a_write_byte(1, TCA6416A_DIR_PORT_A, 0);
	tca6416a_write_byte(1, TCA6416A_DIR_PORT_B, 0xb8);

	/* Init TCA6424A */

	/* Write all GPIO to output 0 */
	tca6424a_write_byte(1, TCA6424A_OUT_PORT_A, 0);
	tca6424a_write_byte(1, TCA6424A_OUT_PORT_C, 0x2);

	/*
	 * Write GPIO direction:
	 *	USERVO_FAULT_L, USB3_A0_FAULT_L, USB3_A1_FAULT_L,
	 *	USB_DUTCHG_FLT_ODL, PP3300_DP_FAULT_L, DAC_BUF1_LATCH_FAULT_L,
	 *	DAC_BUF2_LATCH_FAULT_L, PP5000_SRC_SEL, and unused are
	 *	set to inputs, all others to output.
	 */
	tca6424a_write_byte(1, TCA6424A_DIR_PORT_A, 0);
	tca6424a_write_byte(1, TCA6424A_DIR_PORT_B, 0xff);
	tca6424a_write_byte(1, TCA6424A_DIR_PORT_C, 0x02);

	/* Clear any faults */
	read_faults();
}

inline void sbu_uart_sel(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 0, en);
}

inline void atmel_reset_l(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 1, en);
}

inline void sbu_flip_sel(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 2, en);
}

inline void usb3_a0_mux_sel(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 3, en);
}

inline void usb3_a0_mux_en_l(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 4, en);
}

inline void usb3_a0_pwr_en(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 5, en);
}

inline void uart_18_sel(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 6, en);
}

inline void uservo_power_en(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_A, 7, en);
}

inline void uservo_fastboot_mux_sel(enum uservo_fastboot_mux_sel_t sel)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_B, 0, sel);
}

inline void usb3_a1_pwr_en(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_B, 1, en);
}

inline void usb3_a1_mux_sel(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_B, 2, en);
}

inline int board_id_det(void)
{
	int id;

	id = tca6416a_read_byte(1, TCA6416A_IN_PORT_B);
	if (id < 0)
		return id;

	/* Board ID consists of bits 5, 4, and 3 */
	return (id >> 3) & 0x7;
}

inline void cmux_en(int en)
{
	tca6416a_write_bit(1, TCA6416A_OUT_PORT_B, 6, en);
}

inline int dongle_det(void)
{
	return tca6416a_read_bit(1, TCA6416A_IN_PORT_B, 7);
}

inline void en_pp5000_alt_3p3(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 0, en);
}

inline void en_pp3300_eth(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 1, en);
}

inline void en_pp3300_dp(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 2, en);
}

inline void fault_clear_cc(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 3, en);
}

inline void en_vout_buf_cc1(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 4, en);
}

inline void en_vout_buf_cc2(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 5, en);
}

void dut_chg_en(int en)
{
	dut_chg_en_state = en;
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 6, en);
}

int get_dut_chg_en(void)
{
	return dut_chg_en_state;
}

inline void host_or_chg_ctl(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_A, 7, en);
}

inline int read_faults(void)
{
	int fault;

	fault = tca6424a_read_byte(1, TCA6424A_IN_PORT_B);

	/* Clear faults on this port */
	tca6424a_read_bit(1, TCA6424A_IN_PORT_C, 6);
	tca6424a_read_bit(1, TCA6424A_IN_PORT_C, 2);

	return fault;
}

inline void vbus_dischrg_en(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_C, 0, en);
}

inline void usbh_pwrdn_l(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_C, 1, en);
}

inline void tca_gpio_dbg_led_k_odl(int en)
{
	tca6424a_write_bit(1, TCA6424A_OUT_PORT_C, 7, !en);
}
