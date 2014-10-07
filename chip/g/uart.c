/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

static int done_uart_init_yet;

int uart_init_done(void)
{
	return done_uart_init_yet;
}

void uart_tx_start(void)
{
	/* If interrupt is already enabled, nothing to do */
	if (G_UART_ICTRL(0) & 0x01)
		return;

	/* Do not allow deep sleep while transmit in progress */
	disable_sleep(SLEEP_MASK_UART);

	/*
	 * Re-enable the transmit interrupt, then forcibly trigger the
	 * interrupt.  This works around a hardware problem with the
	 * UART where the FIFO only triggers the interrupt when its
	 * threshold is _crossed_, not just met.
	 */
	REG_WRITE_MASK(G_UART_ICTRL(0), 0x01, 0x01, 0);
	task_trigger_irq(G_IRQNUM_UART0_TXINT);
}

void uart_tx_stop(void)
{
	/* Disable the TX interrupt */
	REG_WRITE_MASK(G_UART_ICTRL(0), 0x01, 0x00, 0);

	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

void uart_tx_flush(void)
{
	/* Wait until TX buffer is completely empty. */
	while (!(G_UART_STATE(0) & 0x10))
		;
}

int uart_tx_ready(void)
{
	/* True if the TX buffer is not completely full */
	return !(G_UART_STATE(0) & 0x01);
}

int uart_tx_in_progress(void)
{
	/* Transmit is in progress if the TX idle bit is not set */
	return !(G_UART_STATE(0) & 0x20);
}

int uart_rx_available(void)
{
	/* True if the RX buffer is not completely empty. */
	/* TODO(wfrichar): Ask Scott to make a single status bit for this. */
	return G_UART_RFIFO(0) & 0x0fc0;
}

void uart_write_char(char c)
{
	/* Wait for space in transmit FIFO. */
	while (!uart_tx_ready())
		;

	G_UART_WDATA(0) = c;
}

int uart_read_char(void)
{
	return G_UART_RDATA(0);
}

static void uart_clear_rx_fifo(int channel)
{
	/* Note: there's only one UART right now */
	while (uart_rx_available())
		(void) uart_read_char();
}

void uart_disable_interrupt(void)
{
	task_disable_irq(G_IRQNUM_UART0_TXINT);
	task_disable_irq(G_IRQNUM_UART0_RXINT);
}

void uart_enable_interrupt(void)
{
	task_enable_irq(G_IRQNUM_UART0_TXINT);
	task_enable_irq(G_IRQNUM_UART0_RXINT);
}

/**
 * Interrupt handler for UART0
 */
void uart_ec_interrupt(void)
{
	/* Clear transmit and receive interrupt status */
	G_UART_ISTATECLR(0) = 0x03;

	/* Read input FIFO until empty, then fill output FIFO */
	uart_process_input();
	uart_process_output();
}

/* Note: handling both in one routine. */
DECLARE_IRQ(G_IRQNUM_UART0_TXINT, uart_ec_interrupt, 1);
DECLARE_IRQ(G_IRQNUM_UART0_RXINT, uart_ec_interrupt, 1);

/* Clock initialization taken from gChips example code */

#define PCLK_FREQ  26000000
#define DEFAULT_UART_FREQ 1000000
#define UART_NCO_WIDTH 16

#define RESOLUTION 12
#define LOAD_VAL (0x1 << 11)
#define MAX_TRIM (7*16)

static void switch_osc_to_xtl(void);

static void clock_on_xo0(void)
{
	/* turn on xo0 clock */
	/* don't know which control word it might be in */
#ifdef G_PMU_PERICLKSET0_DXO0_LSB
	G_PMU_PERICLKSET0 = (1 << G_PMU_PERICLKSET0_DXO0_LSB);
#endif

#ifdef G_PMU_PERICLKSET1_DXO0_LSB
	G_PMU_PERICLKSET1 = (1 << G_PMU_PERICLKSET1_DXO0_LSB);
#endif
}

/* Converts an integer setting to the RC trim code format (7, 4-bit values) */
static unsigned val_to_trim_code(unsigned val)
{
	unsigned base = val / 7;
	unsigned mod = val % 7;
	unsigned code = 0x0;
	int digit;

	/* Increasing count from right to left */
	for (digit = 0; digit < 7; digit++) {
		/* Check for mod */
		if (digit <= mod)
			code |= ((base & 0xF) << 4 * digit);
		else
			code |= (((base - 1) & 0xF) << 4 * digit);
	}

	return code;
}

static unsigned calib_rc_trim(void)
{
	unsigned size, iter;
	signed mid;
	signed diff;

	/* Switch to crystal for calibration. This should work since we are
	 * technically on an uncalibrated RC trim clock. */
	switch_osc_to_xtl();

	clock_on_xo0();

	/* Clear the HOLD signal on dxo */
	G_XO_OSC_CLRHOLD = G_XO_OSC_CLRHOLD_RC_TRIM_MASK;

	/* Reset RC calibration counters */
	G_XO_OSC_RC_CAL_RSTB = 0x0;
	G_XO_OSC_RC_CAL_RSTB = 0x1;

	/* Write the LOAD val */
	G_XO_OSC_RC_CAL_LOAD = LOAD_VAL;

	/* Begin binary search */
	mid = 0;
	size = MAX_TRIM / 2;
	for (iter = 0; iter <= 7; iter++) {
		/* Set the trim value */
		G_XO_OSC_RC = val_to_trim_code(mid) << G_XO_OSC_RC_TRIM_LSB;

		/* Do a calibration */
		G_XO_OSC_RC_CAL_START = 0x1;

		/* NOTE: There is a small race condition because of the delay
		 * in dregfile. The start doesn't actually appear for 2 clock
		 * cycles after the write. So, poll until done goes low. */
		while (G_XO_OSC_RC_CAL_DONE)
			;

		/* Wait until it's done */
		while (!G_XO_OSC_RC_CAL_DONE)
			;

		/* Check the counter value */
		diff = LOAD_VAL - G_XO_OSC_RC_CAL_COUNT;

		/* Test to see whether we are still outside of our desired
		 * resolution */
		if ((diff < -RESOLUTION) || (diff > RESOLUTION))
			mid = (diff > 0) ? (mid - size / 2) : (mid + size / 2);

		size = (size + 1) >> 1;	/* round up before division */
	}

	/* Set the final trim value */
	G_XO_OSC_RC = (val_to_trim_code(mid) << G_XO_OSC_RC_TRIM_LSB) |
	    (0x1 << G_XO_OSC_RC_EN_LSB);

	/* Set the HOLD signal on dxo */
	G_XO_OSC_SETHOLD = G_XO_OSC_SETHOLD_RC_TRIM_MASK;

	/* Switch back to the RC trim now that we are calibrated */
	G_PMU_OSC_HOLD_CLR = 0x1;	/* make sure the hold signal is clear */
	G_PMU_OSC_SELECT = G_PMU_OSC_SELECT_RC_TRIM;
	/* Make sure the hold signal is set for future power downs */
	G_PMU_OSC_HOLD_SET = 0x1;

	return mid;
}

static void switch_osc_to_rc_trim(void)
{
	unsigned trimmed;
	unsigned saved_trim, fuse_trim, default_trim;
	unsigned trim_code;

	/* check which clock we are running on */
	unsigned osc_sel = G_PMU_OSC_SELECT_STAT;

	if (osc_sel == G_PMU_OSC_SELECT_RC_TRIM) {
		/* already using the rc_trim so nothing to do here */
		/* make sure the hold signal is set for future power downs */
		G_PMU_OSC_HOLD_SET = 0x1;
		return;
	}

	/* Turn on DXO clock so we can write in the trim code in */
	clock_on_xo0();

	/* disable the RC_TRIM Clock */
	REG_WRITE_MASK(G_PMU_OSC_CTRL,
		       G_PMU_OSC_CTRL_RC_TRIM_READYB_MASK, 0x1,
		       G_PMU_OSC_CTRL_RC_TRIM_READYB_LSB);

	/* power up the clock if not already powered up */
	G_PMU_CLRDIS = 1 << G_PMU_SETDIS_RC_TRIM_LSB;

	/* Try to find the trim code */
	saved_trim = G_XO_OSC_RC_STATUS;
	fuse_trim = G_PMU_FUSE_RD_RC_OSC_26MHZ;
	default_trim = G_XO_OSC_RC;

	/* Check for the trim code in the always-on domain before looking at
	 * the fuse */
	if (saved_trim & G_XO_OSC_RC_STATUS_EN_MASK) {
		trim_code = (saved_trim & G_XO_OSC_RC_STATUS_TRIM_MASK)
		    >> G_XO_OSC_RC_STATUS_TRIM_LSB;
		trimmed = 1;
	} else if (fuse_trim & G_PMU_FUSE_RD_RC_OSC_26MHZ_EN_MASK) {
		trim_code = (fuse_trim & G_PMU_FUSE_RD_RC_OSC_26MHZ_TRIM_MASK)
		    >> G_PMU_FUSE_RD_RC_OSC_26MHZ_TRIM_LSB;
		trimmed = 1;
	} else {
		trim_code = (default_trim & G_XO_OSC_RC_TRIM_MASK)
		    >> G_XO_OSC_RC_TRIM_LSB;
		trimmed = 0;
	}

	/* Write the trim code to dxo */
	if (trimmed) {
		/* clear the hold signal */
		G_XO_OSC_CLRHOLD = 1 << G_XO_OSC_CLRHOLD_RC_TRIM_LSB;
		G_XO_OSC_RC = (trim_code << G_XO_OSC_RC_TRIM_LSB) | /* write */
		    ((trimmed & 0x1) << G_XO_OSC_RC_EN_LSB); /* enable */
		/* set the hold signal */
		G_XO_OSC_SETHOLD = 1 << G_XO_OSC_SETHOLD_RC_TRIM_LSB;
	}

	/* enable the RC_TRIM Clock */
	REG_WRITE_MASK(G_PMU_OSC_CTRL, G_PMU_OSC_CTRL_RC_TRIM_READYB_MASK,
		       0x0, G_PMU_OSC_CTRL_RC_TRIM_READYB_LSB);

	/* Switch the select signal */
	G_PMU_OSC_HOLD_CLR = 0x1;	/* make sure the hold signal is clear */
	G_PMU_OSC_SELECT = G_PMU_OSC_SELECT_RC_TRIM;
	/* make sure the hold signal is set for future power downs */
	G_PMU_OSC_HOLD_SET = 0x1;

	/* If we didn't find a valid trim code, then we need to calibrate */
	if (!trimmed)
		calib_rc_trim();
	/* We saved the trim code and went back to the RC trim inside
	 * calib_rc_trim */
}

static void switch_osc_to_xtl(void)
{
	unsigned int saved_trim, fuse_trim, trim_code, final_trim;
	unsigned int fsm_status, max_trim;
	unsigned int fsm_done;
	/* check which clock we are running on */
	unsigned int osc_sel = G_PMU_OSC_SELECT_STAT;

	if (osc_sel == G_PMU_OSC_SELECT_XTL) {
		/* already using the crystal so nothing to do here */
		/* make sure the hold signal is set for future power downs */
		G_PMU_OSC_HOLD_SET = 0x1;
		return;
	}

	if (osc_sel == G_PMU_OSC_SELECT_RC)
		/* RC untrimmed clock. We must go through the trimmed clock
		 * first to avoid glitching */
		switch_osc_to_rc_trim();

	/* disable the XTL Clock */
	REG_WRITE_MASK(G_PMU_OSC_CTRL, G_PMU_OSC_CTRL_XTL_READYB_MASK,
		       0x1, G_PMU_OSC_CTRL_XTL_READYB_LSB);

	/* power up the clock if not already powered up */
	G_PMU_CLRDIS = 1 << G_PMU_SETDIS_XTL_LSB;

	/* Try to find the trim code */
	trim_code = 0;
	saved_trim = G_XO_OSC_XTL_TRIM_STAT;
	fuse_trim = G_PMU_FUSE_RD_XTL_OSC_26MHZ;

	/* Check for the trim code in the always-on domain before looking at
	 * the fuse */
	if (saved_trim & G_XO_OSC_XTL_TRIM_STAT_EN_MASK) {
		/* nothing to do */
		/* trim_code = (saved_trim & G_XO_OSC_XTL_TRIM_STAT_CODE_MASK)
		   >> G_XO_OSC_XTL_TRIM_STAT_CODE_LSB; */
		/* print_trickbox_message("XTL TRIM CODE FOUND IN 3P3"); */
	} else if (fuse_trim & G_PMU_FUSE_RD_XTL_OSC_26MHZ_EN_MASK) {
		/* push the fuse trim code as the saved trim code */
		/* print_trickbox_message("XTL TRIM CODE FOUND IN FUSE"); */
		trim_code = (fuse_trim & G_PMU_FUSE_RD_XTL_OSC_26MHZ_TRIM_MASK)
		    >> G_PMU_FUSE_RD_XTL_OSC_26MHZ_TRIM_LSB;
		/* make sure the hold signal is clear */
		G_XO_OSC_CLRHOLD = 0x1 << G_XO_OSC_CLRHOLD_XTL_LSB;
		G_XO_OSC_XTL_TRIM =
		    (trim_code << G_XO_OSC_XTL_TRIM_CODE_LSB) |
		    (0x1 << G_XO_OSC_XTL_TRIM_EN_LSB);
	} else
		/* print_trickbox_message("XTL TRIM CODE NOT FOUND"); */
		;

	/* Run the crystal FSM to calibrate the crystal trim */
	fsm_done = G_XO_OSC_XTL_FSM;
	if (fsm_done & G_XO_OSC_XTL_FSM_DONE_MASK) {
		/* If FSM done is high, it means we already ran it so let's not
		 * run it again */
		/* DO NOTHING */
	} else {
		G_XO_OSC_XTL_FSM_EN = 0x0;	/* reset FSM */
		G_XO_OSC_XTL_FSM_EN = G_XO_OSC_XTL_FSM_EN_KEY;
		while (!(fsm_done & G_XO_OSC_XTL_FSM_DONE_MASK))
			fsm_done = G_XO_OSC_XTL_FSM;
	}

	/* Check the status and final trim value */
	max_trim = (G_XO_OSC_XTL_FSM_CFG & G_XO_OSC_XTL_FSM_CFG_TRIM_MAX_MASK)
	    >> G_XO_OSC_XTL_FSM_CFG_TRIM_MAX_LSB;
	final_trim = (fsm_done & G_XO_OSC_XTL_FSM_TRIM_MASK)
	    >> G_XO_OSC_XTL_FSM_TRIM_LSB;
	fsm_status = (fsm_done & G_XO_OSC_XTL_FSM_STATUS_MASK)
	    >> G_XO_OSC_XTL_FSM_STATUS_LSB;

	/* Check status bit and trim value */
	if (fsm_status) {
		if (final_trim >= max_trim)
			/* print_trickbox_error("ERROR: XTL FSM status was
			   high, but final XTL trim is greater than or equal to
			   max trim"); */
			;
	} else {
		if (final_trim != max_trim)
			/* print_trickbox_error("ERROR: XTL FSM status was low,
			   but final XTL trim does not equal max trim"); */
			;
	}

	/* save the trim for future powerups */
	/* make sure the hold signal is clear (may have already been cleared) */
	G_XO_OSC_CLRHOLD = 0x1 << G_XO_OSC_CLRHOLD_XTL_LSB;
	G_XO_OSC_XTL_TRIM =
	    (final_trim << G_XO_OSC_XTL_TRIM_CODE_LSB) |
	    (0x1 << G_XO_OSC_XTL_TRIM_EN_LSB);
	/* make sure the hold signal is set for future power downs */
	G_XO_OSC_SETHOLD = 0x1 << G_XO_OSC_SETHOLD_XTL_LSB;

	/* enable the XTL Clock */
	REG_WRITE_MASK(G_PMU_OSC_CTRL, G_PMU_OSC_CTRL_XTL_READYB_MASK,
		       0x0, G_PMU_OSC_CTRL_XTL_READYB_LSB);

	/* Switch the select signal */
	G_PMU_OSC_HOLD_CLR = 0x1;	/* make sure the hold signal is clear */
	G_PMU_OSC_SELECT = G_PMU_OSC_SELECT_XTL;
	/* make sure the hold signal is set for future power downs */
	G_PMU_OSC_HOLD_SET = 0x1;
}

void uart_init(void)
{
	long long setting = (16 * (1 << UART_NCO_WIDTH) *
			     (long long)CONFIG_UART_BAUD_RATE / PCLK_FREQ);

	/* Switch to crystal clock since RC clock not accurate enough */
	switch_osc_to_rc_trim();

	/* turn on uart0 clock */
	/* don't know which control word it might be in */
#ifdef G_PMU_PERICLKSET0_DUART0_LSB
	G_PMU_PERICLKSET0 = (1 << G_PMU_PERICLKSET0_DUART0_LSB);
#endif

#ifdef G_PMU_PERICLKSET1_DUART0_LSB
	G_PMU_PERICLKSET1 = (1 << G_PMU_PERICLKSET1_DUART0_LSB);
#endif

	/* set up pinmux */
	G_PINMUX_DIOA0_SEL = G_PINMUX_UART0_TX_SEL;
	G_PINMUX_UART0_RX_SEL = G_PINMUX_DIOA1_SEL;

	/* IE must be set to 1 to work as a digital pad (for any direction) */
	/* turn on input driver (IE field) */
	REG_WRITE_MASK(G_PINMUX_DIOA0_CTL, G_PINMUX_DIOA0_CTL_IE_MASK,
		       1, G_PINMUX_DIOA0_CTL_IE_LSB);
	/* turn on input driver (IE field) */
	REG_WRITE_MASK(G_PINMUX_DIOA1_CTL, G_PINMUX_DIOA1_CTL_IE_MASK,
		       1, G_PINMUX_DIOA1_CTL_IE_LSB);

	/* set frequency */
	G_UART_NCO(0) = setting;

	/* TODO(wfrichar): Do we need to configure 8-N-1 explicitly? */

	/* Interrupt when RX fifo has anything, when TX fifo <= half empty */
	/* Also reset both FIFOs */
	G_UART_FIFO(0) = 0x63;

	/* TX enable, RX enable, HW flow control disabled, no loopback */
	G_UART_CTRL(0) = 0x03;

	/* drain and ignore any incoming bytes */
	uart_clear_rx_fifo(0);

	/* enable RX interrupts in block */
	/* Note: doesn't do anything unless turned on in NVIC */
	G_UART_ICTRL(0) = 0x02;

	/* Enable interrupts for UART0 only */
	uart_enable_interrupt();

	done_uart_init_yet = 1;
}
