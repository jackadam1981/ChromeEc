/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Touch scanning module */

#include "common.h"
#include "console.h"
#include "debug.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "master_slave.h"
#include "registers.h"
#include "spi_comm.h"
#include "timer.h"
#include "touch_scan.h"
#include "util.h"

#define TS_PIN_TO_CR(p) ((((p).port_id + 1) << 16) | (p).mask)
#define TS_GPIO_TO_BASE(p) (0x40010800 + (p) * 0x400)

static uint8_t pmse_data[COL_COUNT + 10][ROW_COUNT * 2];
static uint8_t buf[ROW_COUNT * 2];

static uint32_t accr_list[COL_COUNT];
static uint32_t _arcr_list[ROW_COUNT+1];
static uint32_t * const arcr_list = _arcr_list + 1;

static void set_gpio(const struct ts_pin pin, enum pin_type type)
{
	uint32_t addr, mode, mask;
	uint32_t port = TS_GPIO_TO_BASE(pin.port_id);
	uint32_t pmask = pin.mask;
	if (pmask & 0xff) {
		addr = port;
		mode = pmask;
	} else {
		addr = port + 0x04;
		mode = pmask >> 8;
	}
	mode = mode * mode * mode * mode * 0xf;

	mask = REG32(addr) & ~mode;

	if (type == PIN_COL) {
		/* Alternate output open-drain */
		mask |= 0xdddddddd & mode;
	} else if (type == PIN_PD) {
		mask |= 0x88888888 & mode;
		STM32_GPIO_BSRR(port) = pmask << 16;
	} else if (type == PIN_Z) {
		mask |= 0x55555555 & mode;
		STM32_GPIO_BSRR(port) = pmask;
	} else if (type == PIN_ROW) {
		/* Nothing for PIN_ROW. Already analog input. */
	}

	REG32(addr) = mask;
}

void touch_scan_init(void)
{
	int i;

#ifdef ROW_MODE_IS_PD
	for (i = 0; i < ROW_COUNT; ++i) {
		set_gpio(col_pins[i], PIN_ROW);
		STM32_PMSE_PxPMR(row_pins[i].port_id) |= row_pins[i].mask;
	}
#else
	for (i = 0; i < ROW_COUNT; ++i)
		set_gpio(col_pins[i], ROW_MODE);
#endif
	for (i = 0; i < COL_COUNT; ++i)
		set_gpio(col_pins[i], COL_MODE);

	for (i = 0; i < ROW_COUNT; ++i)
		arcr_list[i] = TS_PIN_TO_CR(row_pins[i]);
	arcr_list[-1] = 0;
	for (i = 0; i < COL_COUNT; ++i)
		accr_list[i] = TS_PIN_TO_CR(col_pins[i]);
}

static void start_adc_sample(int id, int wait_cycle)
{
	/* Clear EOC and STRT bit */
	STM32_ADC_SR(id) &= ~(1 << 1) & ~(1 << 4);

	/* Start conversion */
	STM32_ADC_CR2(id) |= (1 << 0);

	/* Wait for conversion start */
	while (!(STM32_ADC_SR(id) & (1 << 4)))
		;

	/*
	 * ADC sampling time is 28.5 cycle. ADC clock is 4 times
	 * slower than system clock. So we need to wait for ~114 cycles.
	 */
	asm("1: subs %0, #1\n"
	    "   bne 1b\n" :: "r"(wait_cycle));
}

/*
static uint16_t flush_adc(int id)
{
	while (!(STM32_ADC_SR(id) & (1 << 1)))
		;
	return STM32_ADC_DR(id) & ADC_READ_MAX;
}
*/
/*
 * Conversion only takes 12.5 cycles, so it's done when the other ADC
 * is sampling. No need to wait here unless sampling time is shorter than
 * 12.5 cycles.
 */
#define flush_adc(x) STM32_ADC_DR(x)

#ifndef ROW_MODE_IS_PD
static void select_row(int idx)
{
	static int enabled_row = -1;
	if (enabled_row == idx)
		return;
	if (enabled_row != -1)
		enable_row(enabled_row, 0);
	if (idx != -1)
		enable_row(idx, 1);
	enabled_row = idx;
}

static void enable_row(int idx, int enabled)
{
	if (enabled) {
		set_gpio(row_pins[idx], PIN_ROW);
		STM32_PMSE_PxPMR(row_pins[idx].port_id) |= row_pins[idx].mask;
	} else {
		STM32_PMSE_MRCR = 0;
		set_gpio(row_pins[idx], ROW_MODE);
		STM32_PMSE_PxPMR(row_pins[idx].port_id) &= ~row_pins[idx].mask;
	}
}
#else
#define select_row(x) (STM32_PMSE_MRCR = arcr_list[x])
#define enable_row(x, y)
#endif

static void enable_col(int idx, int enabled)
{
	if (enabled) {
		set_gpio(col_pins[idx], PIN_COL);
		STM32_PMSE_PxPMR(col_pins[idx].port_id) |= col_pins[idx].mask;
	} else {
		set_gpio(col_pins[idx], COL_MODE);
		STM32_PMSE_PxPMR(col_pins[idx].port_id) &= ~col_pins[idx].mask;
	}
}

void scan_column(uint8_t *data)
{
	int j;

#ifdef CONFIG_FAST_SCAN
	uint16_t val;

	STM32_PMSE_MRCR = 1 << 31;
	start_adc_sample(0, LONG_CYCLE);
	val = flush_adc(0);
	STM32_PMSE_MRCR = 0;

	if (val < 100) {
		memset(data, 0, ROW_COUNT);
		return;
	}

	for (val = 0; val < 2; ++val) {
#endif
	select_row(0);
	start_adc_sample(0, LONG_CYCLE);
	select_row(1);
	start_adc_sample(1, LONG_CYCLE);

	for (j = 2; j < ROW_COUNT; ++j) {
		data[j - 2] = ADC_DATA_WINDOW(flush_adc(j & 1));
		select_row(j);
		start_adc_sample(j & 1, SHORT_CYCLE);
	}

	while (!(STM32_ADC_SR(ROW_COUNT & 1) & (1 << 1)))
		;
	data[ROW_COUNT - 2] = ADC_DATA_WINDOW(flush_adc(ROW_COUNT & 1));
	while (!(STM32_ADC_SR((ROW_COUNT & 1) ^ 1) & (1 << 1)))
		;
	data[ROW_COUNT - 1] = ADC_DATA_WINDOW(flush_adc((ROW_COUNT & 1) ^ 1));
#ifdef CONFIG_FAST_SCAN
	}
#endif
}

void touch_scan_slave_start(void)
{
	int col, i;
	struct spi_comm_packet *resp = (struct spi_comm_packet *)buf;

	for (col = 0; col < COL_COUNT * 2; ++col) {
		if (col >= COL_COUNT) {
			enable_col(col - COL_COUNT, 1);
			STM32_PMSE_MCCR = accr_list[col - COL_COUNT];
		}
		if (master_slave_sync(20) != EC_SUCCESS)
			return;
		scan_column(resp->data);
		resp->cmd_sts = EC_SUCCESS;
		for (i = 0; i < ROW_COUNT; ++i)
			if (resp->data[i] >= THRESHOLD)
				resp->size = i;
		if (col != 0)
			spi_slave_send_response_flush(1);
		master_slave_sync(20);
		spi_slave_send_response_async(resp);
		if (col >= COL_COUNT) {
			enable_col(col - COL_COUNT, 0);
			STM32_PMSE_MCCR = 0;
		}
	}
	spi_slave_send_response_flush(0);
	master_slave_sync(20);
}

int touch_scan_full_matrix(void)
{
	struct spi_comm_packet cmd;
	const struct spi_comm_packet *resp;
	int col, row;
	timestamp_t st = get_time();
	uint8_t *dptr = NULL, *last_dptr = NULL;

	cmd.cmd_sts = TS_CMD_FULL_SCAN;
	cmd.size = 0;

	if (spi_master_send_command(&cmd))
		return EC_ERROR_UNKNOWN;

	for (col = 0; col < COL_COUNT * 2; ++col) {
		if (col < COL_COUNT) {
			enable_col(col, 1);
			STM32_PMSE_MCCR = accr_list[col];
		}
		if (master_slave_sync(20) != EC_SUCCESS)
			return EC_ERROR_UNKNOWN;

		last_dptr = dptr;
		if (col < COL_COUNT + 10)
			dptr = pmse_data[col];
		else
			dptr = buf;

		scan_column(dptr + ROW_COUNT);

		if (col > 0) {
			/* Flush the data from the slave for the last column */
			resp = spi_master_wait_response_done();
			if (resp == NULL)
				return EC_ERROR_UNKNOWN;
			memcpy(last_dptr, resp->data, resp->size);
			memset(last_dptr + resp->size, 0,
					ROW_COUNT - resp->size);
		}

		master_slave_sync(20);

		if (spi_master_wait_response_async() != EC_SUCCESS)
			return EC_ERROR_UNKNOWN;

		if (col < COL_COUNT) {
			enable_col(col, 0);
			STM32_PMSE_MCCR = 0;
		}
	}

	resp = spi_master_wait_response_done();
	if (resp == NULL)
		return EC_ERROR_UNKNOWN;
	memcpy(last_dptr, resp->data, resp->size);
	memset(last_dptr + resp->size, 0, ROW_COUNT - resp->size);
	master_slave_sync(20);

	debug_printf("Sampling took %d us\n", get_time().val - st.val);

	for (row = 0; row < ROW_COUNT * 2; ++row) {
		for (col = 0; col < COL_COUNT + 10; ++col) {
			if (pmse_data[col][row] < THRESHOLD)
				debug_printf("  - ");
			else
				debug_printf("%3d ", pmse_data[col][row]);
		}
		debug_printf("\n");
	}

	return EC_SUCCESS;
}
