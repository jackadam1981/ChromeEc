/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "console.h"
#include "endian.h"
#include "gpio.h"
#include "hooks.h"
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
	/* set pin mux to eMMC spi */
	IT83XX_GCTRL_PIN_MUX0 |= BIT(7);
	/* SPI slave module in eMMC Alternative Boot Mode */
	IT83XX_SPI_EMMCBMR |= IT83XX_SPI_EMMCABM;
	/*
	 * Enable interrupt to detect AP's BOOTBLOCK_EN_L. So EC is able to
	 * switch SPI slave module back to communication mode once
	 * BOOTBLOCK_EN_L goes high (AP Jumped to bootloader).
	 */
	emmc_enable_spi();
}
DECLARE_HOOK(HOOK_INIT, emmc_init_spi, HOOK_PRIO_INIT_SPI + 1);

/* AP has booted */
void ap_jump_to_bl(enum gpio_signal signal)
{
	CPRINTF("AP Jumped to BL");
}

#define RX_BUF_NEXT_32(i) (((i) + 1) & (SPI_RX_BUF_WORDS - 1))
#define RX_BUF_DEC_32(i, j) (((i) - (j)) & (SPI_RX_BUF_WORDS - 1))
#define RX_BUF_PREV_32(i) RX_BUF_DEC_32((i), 1)

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
#if 0
	static int transfer_try;

	dma_chan_t *txdma = dma_get_channel(STM32_DMAC_SPI_EMMC_TX);

	dma_prepare_tx(&dma_tx_option, sizeof(bootblock_raw_data),
		       bootblock_raw_data);
	dma_go(txdma);

	CPRINTS("transfer %d", ++transfer_try);
#endif
	//IT83XX_GPIO_GPCRE3 = 0x40;
	//IT83XX_GPIO_GPDRE  |= BIT(3);
	panic_printf("bootblock_transfer start\n");
	//panic_printf("i=%d rem=%d buf_idx=%d\n", i, remaining, buf_idx);
	IT83XX_GPIO_GPDRH |= BIT(3);
	//IT83XX_SPI_SPISRDR = 0xff;
	for (i = 0; i < remaining; i += byte_sended) {
		//panic_printf("i=%d rem=%d buf_idx=%d\n", i, remaining, buf_idx);
		if (!i) {
			panic_printf("===bootblock_transfer 256 bytes===\n");
			/* Tx FIFO reset and count monitor reset */
			IT83XX_SPI_TXFCR = IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;
			/* CPU Tx FIFO1 and FIFO2 access */
			IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;
			for (c = 0; c < 256; c += 4) {
				/* Write response data from out_msg buffer to Tx FIFO */
				IT83XX_SPI_CPUWTFDB0 = *(uint32_t *)(bootblock_raw_data + buf_idx);
				//IT83XX_SPI_CPUWTFDB0 = 0x66666666;
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
			//remaining -= 256;
			byte_sended = 256;
			continue;
		}

		//if (remaining > 0) {
		//while (!(IT83XX_SPI_TXFSR & BIT(0)))
		//	;

		//while (1) {
		//	if (IT83XX_SPI_TXFSR & BIT(0))
		//		break;
		//}


		if (++temp_cnt < 3)
			break;
#if 1
		//IT83XX_GPIO_GPDRE &= ~BIT(3);

		while (1) {
			if (IT83XX_SPI_TXFSR & BIT(0))
				break;
		}
		IT83XX_GPIO_GPDRH &= ~BIT(3);
		/* CPU Tx FIFO1 and FIFO2 access */
		IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;

		for (c = 0; c < 128; c += 4) {
			/* Write response data from out_msg buffer to Tx FIFO */
			IT83XX_SPI_CPUWTFDB0 = *(uint32_t *)(bootblock_raw_data + buf_idx);
			//IT83XX_SPI_CPUWTFDB0 = parten;
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
		IT83XX_GPIO_GPDRH |= BIT(3);
		byte_sended = 128;

#if 0
		parten ++;
		if (buf_idx < remaining) {
			//panic_printf("=%x=\n", buf_idx);
			while (1) {
				if (IT83XX_SPI_TXFSR & BIT(0))
					break;
			}
		} else {
			IT83XX_SPI_FCR = 0;
			panic_printf("===we should break===\n");
		}
#endif
		//panic_printf("next block\n");
		//}
#endif
		
	}

	panic_printf("bootblock_transfer end\n");
#if 0

	/* Tx FIFO reset and count monitor reset */
	IT83XX_SPI_TXFCR = IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;
	/* CPU Tx FIFO1 and FIFO2 access */
	IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;
	for (c = 0; c < 256; c += 4, i += 4) {
		/* Write response data from out_msg buffer to Tx FIFO */
		IT83XX_SPI_CPUWTFDB0 = *(uint32_t *)(bootblock_raw_data + i);
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

	/* Tx FIFO reset and count monitor reset */
	//IT83XX_SPI_TXFCR = IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;

	for (; i < ; i += 4) {
		while (!(IT83XX_SPI_TXFSR & BIT(0)))
			;
		for (c = 0; c < 128; c += 4)
		/* CPU Tx FIFO1 and FIFO2 access */
		IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;
		/* Write response data from out_msg buffer to Tx FIFO */
		IT83XX_SPI_CPUWTFDB0 = *(uint32_t *)(bootblock_raw_data + i);
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
#endif
}

static enum emmc_cmd emmc_parse_command(int index)
{
	int32_t shift0;
	uint32_t data[3];

	if (in_msg[index] == 0xffffffff)
		return EMMC_ERROR;

	data[0] = htobe32(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[1] = htobe32(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[2] = htobe32(in_msg[index]);

	if ((data[0] & 0xff000000) != 0x40000000) {
		/* Figure out alignment (cmd starts with 01) */
		//CPRINTS("data0:%x data1:%x", data[0], data[1]);
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
#if 0
	enum emmc_cmd cmd;

	in_msg = spi_get_in_msg();
#endif

	CPRINTS("bootblock_raw_data size=%d addr=%x in_msg=%x",
		sizeof(bootblock_raw_data),
		(uint32_t)bootblock_raw_data, (uint32_t)in_msg);

	while (1) {
		task_wait_event(-1);

		bootblock_transfer();
	}

#if 0
	while (1) {
		task_wait_event(-1);

		for (int i = 0; i < SPI_RX_BUF_WORDS; i++) {

			if (in_msg[i] == 0xffffffff)
				continue;

			cmd = emmc_parse_command(i);

			/*
			 * Host sends GO_IDLE_STATE to abort the transfer (e.g.
			 * when an incorrect number of lanes is used) and when
			 * the transfer is complete.
			 * Also react to GO_PRE_IDLE_STATE in case we missed
			 * GO_IDLE_STATE command.
			 */
			if (cmd == EMMC_IDLE || cmd == EMMC_PRE_IDLE) {
				bootblock_stop();
				break;
			}

			if (cmd == EMMC_BOOT) {
				bootblock_transfer();
				break;
			}
		}
		IT83XX_SPI_IMR &= ~IT83XX_SPI_RX_REACH;
		IT83XX_SPI_ISR = 0xff;
	}
#endif
}

#if 0
static void dummy_func(void)
{

}
#endif

void emmc_isr(void)
{
	enum emmc_cmd cmd;
	in_msg = spi_get_in_msg();

	for (int i = 0; i < SPI_RX_BUF_WORDS; i++) {

		if (in_msg[i] == 0xffffffff)
			continue;

		cmd = emmc_parse_command(i);
		if (cmd == EMMC_IDLE || cmd == EMMC_PRE_IDLE) {
			bootblock_stop();
			break;
		}

		if (cmd == EMMC_BOOT) {
			task_wake(TASK_ID_EMMC);
			//bootblock_transfer();
			break;
		}
	}
}
