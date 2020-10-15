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

#define SPI_RX_BUF_BYTES 16
#define SPI_RX_BUF_WORDS (SPI_RX_BUF_BYTES/4)
#define RX_BUF_NEXT_32(i) (((i) + 1) & (SPI_RX_BUF_WORDS - 1))
#define RX_BUF_DEC_32(i, j) (((i) - (j)) & (SPI_RX_BUF_WORDS - 1))
#define RX_BUF_PREV_32(i) RX_BUF_DEC_32((i), 1)
static uint32_t *in_msg;

enum emmc_cmd {
	EMMC_ERROR = -1,
	EMMC_IDLE = 0,
	EMMC_PRE_IDLE,
	EMMC_BOOT,
};

static void emmc_enable_spi(void)
{
	gpio_clear_pending_interrupt(GPIO_BOOTBLOCK_EN_L);
	gpio_enable_interrupt(GPIO_BOOTBLOCK_EN_L);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, emmc_enable_spi, HOOK_PRIO_FIRST);

static void emmc_disable_spi(void)
{
	gpio_disable_interrupt(GPIO_BOOTBLOCK_EN_L);
	IT83XX_SPI_EMMCBMR &= ~IT83XX_SPI_EMMCABM;
	CPRINTS("eMMC emulation disabled");
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, emmc_disable_spi, HOOK_PRIO_FIRST);

static void emmc_init_spi(void)
{
	/* Enable SPI slave alternate function */
	gpio_config_module(MODULE_SPI_FLASH, 1);
	/*
	 * Enable interrupt to detect AP's BOOTBLOCK_EN_L. So EC is able to
	 * switch SPI slave module back to communication mode once
	 * BOOTBLOCK_EN_L goes high (AP Jumped to bootloader).
	 */
	emmc_enable_spi();
	IT83XX_SPI_FTCB1R = 0;
	IT83XX_SPI_FTCB0R = 128;
	/* Response idle state (high) */
	IT83XX_SPI_SPISRDR = 0xff;
	IT83XX_SPI_IMR = 0xff;
	/* Enable Rx fifo full interrupt */
	IT83XX_SPI_IMR &= ~IT83XX_SPI_RX_FIFO_FULL;
	IT83XX_SPI_RX_VLISMR |= IT83XX_SPI_RVLIM;
	/* set pin mux to eMMC spi */
	IT83XX_GCTRL_PIN_MUX0 |= BIT(7);
	/* SPI slave module in eMMC Alternative Boot Mode */
	IT83XX_SPI_EMMCBMR |= IT83XX_SPI_EMMCABM;
}
DECLARE_HOOK(HOOK_INIT, emmc_init_spi, HOOK_PRIO_INIT_SPI + 1);

/* AP has booted */
void ap_jump_to_bl(enum gpio_signal signal)
{
	CPRINTF("AP Jumped to BL");
}

/* Abort an ongoing transfer. */
static void bootblock_stop(void)
{
	IT83XX_SPI_FCR = 0;
}

static int temp_cnt = 0;
void bootblock_transfer(void)
{
	int i = 0, c, byte_sended, buf_idx = 0;
	int remaining = sizeof(bootblock_raw_data);
	const uint32_t timeout_us = 200;
	uint32_t start;

	panic_printf("bootblock_transfer start\n");
	for (i = 0; i < remaining; i += byte_sended) {
		if (!i) {
			/* Tx FIFO reset and count monitor reset */
			IT83XX_SPI_TXFCR = IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;
			/* CPU Tx FIFO1 and FIFO2 access */
			IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;
			for (c = 0; c < 256; c += 4) {
				/* Write response data from out_msg buffer to Tx FIFO */
				IT83XX_SPI_CPUWTFDB0 =
				*(uint32_t *)(bootblock_raw_data + buf_idx);
				buf_idx += 4;
			}
			/*
			 * After writing data to Tx FIFO is finished, this bit will
			 * be to indicate the SPI slave controller.
			 */
			IT83XX_SPI_TXFCR = IT83XX_SPI_TXFS;
			/* End Tx FIFO access */
			IT83XX_SPI_TXRXFAR = 0;
			/* SPI slave read Tx FIFO */
			IT83XX_SPI_FCR = IT83XX_SPI_SPISRTXF;
			byte_sended = 256;
			continue;
		}

		/* TODO: handle idle while TX */
		if (++temp_cnt < 3)
			break;

		start = __hw_clock_source_read();
		while (!(IT83XX_SPI_TXFSR & BIT(0)) &&
			(__hw_clock_source_read() - start < timeout_us))
			;

		/* CPU Tx FIFO1 and FIFO2 access */
		IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;
		for (c = 0; c < 128; c += 4) {
			/* Write response data from out_msg buffer to Tx FIFO */
			IT83XX_SPI_CPUWTFDB0 =
				*(uint32_t *)(bootblock_raw_data + buf_idx);
			buf_idx += 4;
		}
		/*
		 * After writing data to Tx FIFO is finished, this bit will
		 * be to indicate the SPI slave controller.
		 */
		IT83XX_SPI_TXFCR = IT83XX_SPI_TXFS;
		/* End Tx FIFO access */
		IT83XX_SPI_TXRXFAR = 0;
		/* SPI slave read Tx FIFO */
		IT83XX_SPI_FCR = IT83XX_SPI_SPISRTXF;
		byte_sended = 128;
		
	}
}

static enum emmc_cmd emmc_parse_command(int index)
{
	int32_t shift0;
	uint32_t data[3];

	data[0] = htobe32(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[1] = htobe32(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[2] = htobe32(in_msg[index]);

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
		bootblock_transfer();
	}
}

void emmc_isr(void)
{
	enum emmc_cmd cmd;
	in_msg = spi_get_in_msg();

	for (int i = 0; i < SPI_RX_BUF_WORDS; i++) {
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
