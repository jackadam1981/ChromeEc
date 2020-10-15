/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "console.h"
#include "endian.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "intc.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include "bootblock_data.h"

#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

static uint32_t *in_msg;
static uint32_t emmc_bl_done;
static int tx_cnt;

enum emmc_cmd {
	EMMC_ERROR = -1,
	EMMC_IDLE = 0,
	EMMC_PRE_IDLE,
	EMMC_BOOT,
};

static void emmc_enable_spi(void)
{
	/* Set pin mux to eMMC spi */
	IT83XX_GCTRL_PIN_MUX0 |= BIT(7);
	/* SPI slave module in eMMC Alternative Boot Mode */
	IT83XX_SPI_EMMCBMR |= IT83XX_SPI_EMMCABM;

	/* Response idle state (high) */
	IT83XX_SPI_SPISRDR = 0xff;
	/* FIFO will be overwritten once it's full */
	IT83XX_SPI_GCR2 = 0;
	/* Enable Rx fifo full interrupt only */
	IT83XX_SPI_IMR = 0xff;
	IT83XX_SPI_IMR &= ~IT83XX_SPI_RX_FIFO_FULL;
	IT83XX_SPI_RX_VLISMR |= IT83XX_SPI_RVLIM;
	/* Write to clear */
	IT83XX_SPI_ISR = IT83XX_SPI_RX_FIFO_FULL;
	/*
	 * Enable interrupt to detect AP's BOOTBLOCK_EN_L. So EC is able to
	 * switch SPI slave module back to communication mode once
	 * BOOTBLOCK_EN_L goes high (AP Jumped to bootloader).
	 */
	gpio_clear_pending_interrupt(GPIO_BOOTBLOCK_EN_L);
	gpio_enable_interrupt(GPIO_BOOTBLOCK_EN_L);

	disable_sleep(SLEEP_MASK_EMMC);
	CPRINTS("eMMC emulation enabled");
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, emmc_enable_spi, HOOK_PRIO_FIRST);

static void emmc_disable_spi(void)
{
	/* TODO: remove me after stress test */
	return;

	/* Set pin mux to spi for communication */
	IT83XX_GCTRL_PIN_MUX0 &= ~BIT(7);
	/* Disable eMMC Alternative Boot Mode */
	IT83XX_SPI_EMMCBMR &= ~IT83XX_SPI_EMMCABM;

	/* TODO: check SPI slave for communication still works. */
	/* Restore setting of SPI slave for communication */
	/* Ready to receive */
	IT83XX_SPI_SPISRDR = EC_SPI_OLD_READY;
	/* FIFO won't be overwritten once it's full */
	IT83XX_SPI_GCR2 = IT83XX_SPI_RXF2OC | IT83XX_SPI_RXF1OC
			| IT83XX_SPI_RXFAR;
	/* Enable Rx valid length interrupt only */
	IT83XX_SPI_IMR = 0xff;
	IT83XX_SPI_RX_VLISMR &= ~IT83XX_SPI_RVLIM;
	/* write clear slave status */
	IT83XX_SPI_RX_VLISR = IT83XX_SPI_RVLI;
	/* Disable interrupt to detect AP's BOOTBLOCK_EN_L */
	gpio_disable_interrupt(GPIO_BOOTBLOCK_EN_L);

	enable_sleep(SLEEP_MASK_EMMC);
	CPRINTS("eMMC emulation disabled");
}

static void emmc_init_spi(void)
{
	/* Enable alternate function */
	gpio_config_module(MODULE_SPI_FLASH, 1);
	/*
	 * Set SPI slave module work as eMMC Alternative Boot Mode.
	 * (CS# pin isn't required, and droping data until CMD goes low)
	 */
	emmc_enable_spi();
}
DECLARE_HOOK(HOOK_INIT, emmc_init_spi, HOOK_PRIO_INIT_SPI + 1);

/* Abort an ongoing transfer. */
static void bootblock_stop(void)
{
	/* Tx FIFO reset and count monitor reset */
	IT83XX_SPI_TXFCR = IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;
	/*
	 * Send the setting (0xff,  idle state) of SPI status register if master
	 * clocks in data.
	 */
	IT83XX_SPI_FCR = 0;
}

/* AP has booted */
void ap_jump_to_bl(enum gpio_signal signal)
{
	emmc_disable_spi();
	bootblock_stop();
	/* End Rx FIFO access */
	IT83XX_SPI_TXRXFAR = 0;
	/* Rx FIFO reset and count monitor reset */
	IT83XX_SPI_FCR = IT83XX_SPI_RXFR | IT83XX_SPI_RXFCMR;

	/* TODO: remove me after stress test */
	tx_cnt = 0;
	emmc_bl_done++;

	CPRINTS("AP Jumped to BL %d", emmc_bl_done);
}

static void bootblock_send_data_over_spi(uint32_t *tx, int tx_size, int rst_tx)
{
	int i;

	/* Tx FIFO reset and count monitor reset */
	if (rst_tx)
		IT83XX_SPI_TXFCR = IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;
	/* CPU Tx FIFO1 and FIFO2 access */
	IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;

	/* Write response data from out_msg buffer to Tx FIFO */
	for (i = 0; i < (tx_size / 4); i++)
		IT83XX_SPI_CPUWTFDB0 = tx[i];
	/*
	 * After writing data to Tx FIFO is finished, this bit will
	 * be to indicate the SPI slave controller.
	 */
	IT83XX_SPI_TXFCR = IT83XX_SPI_TXFS;
	/* End Tx FIFO access */
	IT83XX_SPI_TXRXFAR = 0;
	/* SPI slave read Tx FIFO */
	IT83XX_SPI_FCR = IT83XX_SPI_SPISRTXF;
}

static void bootblock_transfer(void)
{
	int i, byte_sended, raw_idx = 0;
	int remaining = sizeof(bootblock_raw_data);
	uint32_t *raw = (uint32_t *)bootblock_raw_data;
	const uint32_t timeout_us = 200;
	uint32_t start;

	/* TODO: remove me after stress test */
	CPRINTS("%s %d", __func__, ++tx_cnt);

	for (i = 0; i < remaining; i += byte_sended) {
		if (!i) {
			bootblock_send_data_over_spi(&raw[raw_idx], 256, 1);
			raw_idx += (256 / 4);
			byte_sended = 256;
			continue;
		}

		start = __hw_clock_source_read();
		while (!(IT83XX_SPI_TXFSR & BIT(0)) &&
			(__hw_clock_source_read() - start < timeout_us))
			;
		if (IT83XX_SPI_ISR & IT83XX_SPI_RX_FIFO_FULL)
			break;

		bootblock_send_data_over_spi(&raw[raw_idx], 128, 0);
		raw_idx += (128 / 4);
		byte_sended = 128;
	}
}

static enum emmc_cmd emmc_parse_command(int index)
{
	int32_t shift0;
	uint32_t data[3];

	data[0] = htobe32(in_msg[index]);
	data[1] = htobe32(in_msg[index+1]);
	data[2] = htobe32(in_msg[index+2]);

	if ((data[0] & 0xff000000) != 0x40000000) {
		/* Figure out alignment (cmd starts with 01) */
		/* Number of leading ones. */
		shift0 = __builtin_clz(~data[0]);

		data[0] = (data[0] << shift0) | (data[1] >> (32-shift0));
		data[1] = (data[1] << shift0) | (data[2] >> (32-shift0));
	}

	if (data[0] == 0x40000000 && data[1] == 0x0095ffff) {
		/* 400000000095 GO_IDLE_STATE */
		CPRINTS("goIdle");
		return EMMC_IDLE;
	}

	if (data[0] == 0x40f0f0f0 && data[1] == 0xf0fdffff) {
		/* 40f0f0f0f0fd GO_PRE_IDLE_STATE */
		CPRINTS("goPreIdle");
		return EMMC_PRE_IDLE;
	}

	if (data[0] == 0x40ffffff && data[1] == 0xfae5ffff) {
		/* 40fffffffae5 BOOT_INITIATION */
		CPRINTS("bootInit");
		return EMMC_BOOT;
	}

	CPRINTS("eMMC error");
	return EMMC_ERROR;
}

void emmc_task(void *u)
{
	while (1) {
		task_wait_event(-1);
		/* Enable tx fifo to transfer bootblock */
		interrupt_disable();
		bootblock_transfer();
		interrupt_enable();
	}
}

void emmc_isr(void)
{
	enum emmc_cmd cmd;

	in_msg = spi_get_in_msg();

	for (int i = 0; i < 8; i++) {
		if (in_msg[i] == 0xffffffff)
			continue;

		cmd = emmc_parse_command(i);

		if (cmd == EMMC_IDLE || cmd == EMMC_PRE_IDLE) {
			bootblock_stop();
			break;
		}

		if (cmd == EMMC_BOOT) {
			task_wake(TASK_ID_EMMC);
			break;
		}
	}
}
