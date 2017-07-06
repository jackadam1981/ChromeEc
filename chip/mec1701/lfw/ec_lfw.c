/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MEC1701 SoC little FW
 *
 */

#include <stdint.h>

#include "config.h"
#include "gpio.h"
#include "spi.h"
#include "spi_flash.h"
#include "util.h"
#include "timer.h"
#include "dma.h"
#include "registers.h"
#include "cpu.h"
#include "clock.h"
#include "system.h"
#include "version.h"
#include "hwtimer.h"
#include "gpio_list.h"

#include "ec_lfw.h"

/*
 * Disable LFW debug data integrity check using SHA256
 * #define LFW_DEBUG_USE_CHIP_SHA256
 */

#ifdef CONFIG_CHIP_LFW_USE_ROM_SPI
#include "rom_api_chip.h"
#endif

#ifdef LFW_DEBUG_USE_CHIP_SHA256
#include "rom_api_chip.h"
#include "sha256_chip.h"
#endif

#define LFW_SPI_BYTE_TRANSFER_TIMEOUT_US (1 * MSEC)
#define LFW_SPI_BYTE_TRANSFER_POLL_INTERVAL_US 100



__attribute__ ((section(".intvector")))
const struct int_vector_t hdr_int_vect = {
	/* init sp, unused. set by MEC ROM loader */
	(void *)lfw_stack_top,  /* preserve ROM log. was (void *)0x11FA00, */
	&lfw_main,	/* was &lfw_main, */	  /* reset vector */
	&fault_handler,   /* NMI handler */
	&fault_handler,   /* HardFault handler */
	&fault_handler,   /* MPU fault handler */
	&fault_handler    /* Bus fault handler */
};


/* SPI devices - from board.c */
const struct spi_device_t spi_devices[] = {
	{ CONFIG_SPI_FLASH_PORT, 4, GPIO_SHD_CS0 },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);


/*
 * At POR or EC reset MEC17xx Boot-ROM should only load LFW and jumps
 * into LFW entry point located at offset 0x04 of LFW.
 * Entry point is programmed into SPI Header by Python SPI image
 * builder at chip/mec1701/util/pack_ec.py
 *
 * EC_RO/RW calling LFW should enter through this routine if you
 * want the vector table updated. The stack should be set to
 * LFW linker file parameter lfw_stack_top because we do not
 * know if the callers stack is OK.
 *
 * Make sure lfw_stack_top will not overwrite panic data!
 * from include/panic.h
 * Panic data goes at the end of RAM.  This is safe because we don't context
 * switch away from the panic handler before rebooting, and stacks and data
 * start at the beginning of RAM.
 *
 * chip/mec1701/config_chip.h
 * #define CONFIG_RAM_SIZE 0x00008000
 * #define CONFIG_RAM_BASE 0x120000 - 0x8000 = 0x118000
 *
 *  #define PANIC_DATA_PTR ((struct panic_data *)\
 *	(CONFIG_RAM_BASE + CONFIG_RAM_SIZE - sizeof(struct panic_data)))
 *
 * LFW stack located by ec_lfw.ld linker file 256 bytes below top of
 * data SRAM.
 * PROVIDE( lfw_stack_top = 0x11F000 );
 *
 * !!!WARNING!!!
 * MEC1701 BootROM zeros all memory therefore any chip reset will destroy
 * panic data.
 */


#ifdef LFW_DEBUG_USE_CHIP_SHA256


/* 10 ms */
#define TMOUT_SHA256_HW_UPDATE		(10000ul)
/* 1 ms */
#define TMOUT_SHA256_HW_FINAL		(1000ul)

/*
 * One clock/byte max or (48MHz) 1.33 us/64 byte chunk + AHB transfer time.
 * Huge guard band of 32 us. (32 1MHz clocks)
 */
static int hash_done(uint64_t mtimeout)
{
	uint64_t m;
	uint32_t u2, u3;

	m = 0;
	u2 = MEC17XX_TMR32_CNT(0);
	while (rom_hash_busy()) {
		u3 = MEC17XX_TMR32_CNT(0);
		if (u3 <= u2)
			m += (u2 - u3);
		else
			m += u2 + (0xfffffffful - u3);

		if (m > mtimeout)
			return EC_ERROR_TIMEOUT;

		u2 = u3;
	}

	return EC_SUCCESS;
}


#endif /* #ifdef LFW_DEBUG_USE_CHIP_SHA256 */


void timer_init(void)
{
	uint32_t val = 0;

	/* Ensure timer is not running */
	MEC17XX_TMR32_CTL(0) &= ~(1 << 5);

	/* Enable timer */
	MEC17XX_TMR32_CTL(0) |= (1 << 0);

	val = MEC17XX_TMR32_CTL(0);

	/* Pre-scale = 48 -> 1MHz -> Period = 1us */
	val = (val & 0xffff) | (47 << 16);

	MEC17XX_TMR32_CTL(0) = val;

	/* Set preload to use the full 32 bits of the timer */
	MEC17XX_TMR32_PRE(0) = 0xffffffff;

	/* Override the count */
	MEC17XX_TMR32_CNT(0) = 0xffffffff;

	/* Auto restart */
	MEC17XX_TMR32_CTL(0) |= (1 << 3);

	/* Start counting in timer 0 */
	MEC17XX_TMR32_CTL(0) |= (1 << 5);

}

#ifdef CONFIG_CHIP_LFW_USE_ROM_SPI
static int spi_flash_readloc(uint32_t *buf, uint32_t offset, uint32_t bytes,
			     uint32_t spi_cmd)
{
	uint64_t m, m2;
	uint32_t u, u2, u3;

	if (offset + bytes > CONFIG_FLASH_SIZE)
		return EC_ERROR_INVAL;

	u = rom_qmspi_cfg_read_dma(spi_cmd,
				    offset,
				    (uint32_t)buf,
				    bytes,
				    MEC17XX_DMAC_QMSPI0_RX);
	if (u != bytes)
		return EC_ERROR_PARAM3; /* bytes too large */

	rom_qmspi_start_dma(MEC17XX_DMAC_QMSPI0_RX, 0);

	u = 0;
	m2 = 0;
	m = LFW_SPI_BYTE_TRANSFER_TIMEOUT_US * bytes;
	u2 = MEC17XX_TMR32_CNT(0);
	while (!rom_qmspi_is_done(&u)) {
		u3 = MEC17XX_TMR32_CNT(0);
		if (u3 <= u2)
			m2 += (u2 - u3);
		else
			m2 += u2 + (0xfffffffful - u3);

		if (m2 > m) {
			trace11(0, LFW, 0,
				"spi_flash_readloc: tmout QMSPI.Status=0x%08x",
				u);
			return EC_ERROR_TIMEOUT;
		}
		u2 = u3;
	}

	return EC_SUCCESS;
}

#else

/*
 * Use copy of SPI flash read compiled for LFW (no semaphores).
 * LFW timeout code does not use interrupts so reset timer
 * before starting SPI read to minimize probability of
 * timer wrap.
 */
static int spi_flash_readloc(uint8_t *buf_usr,
				unsigned int offset,
				unsigned int bytes)
{
	uint8_t cmd[4] = {SPI_FLASH_READ,
				(offset >> 16) & 0xFF,
				(offset >> 8) & 0xFF,
				offset & 0xFF};

	if (offset + bytes > CONFIG_FLASH_SIZE)
		return EC_ERROR_INVAL;

	__hw_clock_source_set(0); /* restart free run timer */
	return spi_transaction(SPI_FLASH_DEVICE, cmd, 4, buf_usr, bytes);
}
#endif

#ifdef LFW_DEBUG_USE_CHIP_SHA256

#define SPI_CHUNK_M64 (SPI_CHUNK_SIZE & 0x3F)
#define SPI_AL4 ((CONFIG_RW_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE) & 0x3F)

const uint32_t qmspi_freq_tbl[2] = {
	QMSPI_FREQ_24M,
	QMSPI_FREQ_12M
};

const uint32_t qmspi_rd_cmd_tbl[2] = {
	QMSPI_READ_112_FAST,
	QMSPI_READ_111_FAST
};

/*
 * Last 32-bytes of EC_RO/RW is a SHA-256 of the preceding bytes
 */
int spi_image_load(uint32_t offset, uint32_t *digest, uint32_t *block2)
{
	/* = 0x1000 + 0x000E0000 = 0xE1000 */
	uint32_t *buf = (uint32_t *) (CONFIG_RW_MEM_OFF +
				    CONFIG_PROGRAM_MEMORY_BASE);
	uint32_t i, m, n;
	int rc;

	BUILD_ASSERT(((CONFIG_RW_MEM_OFF +
			CONFIG_PROGRAM_MEMORY_BASE) & 0x03) == 0);
	BUILD_ASSERT(CONFIG_RO_SIZE == CONFIG_RW_SIZE);
	BUILD_ASSERT(SPI_CHUNK_M64 == 0);
	BUILD_ASSERT(SPI_AL4 == 0);

	trace11(0, LFW, 0, "spi_load_image offset = 0x%08x", offset);

	for (m = 0; m < 2; m++) {
		trace1(0, LFW, 0, "SPI Freq = %d MHz", (24 >> m));
		/* Why fill all but last 4-bytes? */
		memset((void *)buf, 0xFF, (CONFIG_RO_SIZE - 4));
		for (n = 0; n < 2; n++) {
			trace11(0, LFW, 0,
				"SPI Read Cmd = 0x%08x", qmspi_rd_cmd_tbl[n]);
			rom_qmspi_init(qmspi_freq_tbl[m],
				QMSPI_SPI_MODE0, QMSPI_IFCTRL_DFLT);
			for (i = 0; i < CONFIG_RO_SIZE; i += SPI_CHUNK_SIZE) {
				rc = spi_flash_readloc(&buf[i >> 2], offset + i,
					SPI_CHUNK_SIZE, qmspi_rd_cmd_tbl[n]);
				if (rc != EC_SUCCESS) {
					trace12(0, LFW, 0,
						"read chunk %d failed: rc=%d",
						i, rc);
					break;
				}
			}

			if (rc == EC_SUCCESS) {
				rom_sha_init(MEC17XX_ROM_SHA_MODE_256, digest);
				rom_sha_update((const uint32_t *)buf,
					(CONFIG_RO_SIZE - 32) >> 6, 0x05);
				rc = hash_done(TMOUT_SHA256_HW_UPDATE);
				if (rc != EC_SUCCESS) {
					trace0(0, LFW, 0,
						"SHA256 update timeout");
					continue;
				}

				i = ((CONFIG_RO_SIZE - 32) & ~(0x3Ful)) >> 2;
				rom_sha_final(block2, (CONFIG_RO_SIZE - 32),
					(const uint8_t *)&buf[i], 0x05);
				rc = hash_done(TMOUT_SHA256_HW_FINAL);
				if (rc != EC_SUCCESS) {
					trace0(0, LFW, 0,
						"SHA256 finalize timeout");
					continue;
				}

				i = 0;
				while (i < 32/4) {
					if (digest[i] !=
						buf[(CONFIG_RO_SIZE - 32)/4
							+ i]) {
						trace0(0, LFW, 0,
						  "spi load SHA256 mismatch");
						break;
					}
					i++;
				}
				if (i == 8) {
					trace0(0, LFW, 0,
						"spi_load_image SHA256 OK");
					return EC_SUCCESS;
				}
			}
		}
	}

	return EC_ERROR_UNCHANGED;
}

#else

int spi_image_load(uint32_t offset)
{
	uint8_t *buf = (uint8_t *) (CONFIG_RW_MEM_OFF +
				    CONFIG_PROGRAM_MEMORY_BASE);
	uint32_t i;

	BUILD_ASSERT(CONFIG_RO_SIZE == CONFIG_RW_SIZE);

	/* Why fill all but last 4-bytes? */
	memset((void *)buf, 0xFF, (CONFIG_RO_SIZE - 4));

	for (i = 0; i < CONFIG_RO_SIZE; i += SPI_CHUNK_SIZE)
		spi_flash_readloc(&buf[i], offset + i, SPI_CHUNK_SIZE);

	return 0;
}
#endif


void udelay(unsigned us)
{
	uint32_t t0 = __hw_clock_source_read();

	while (__hw_clock_source_read() - t0 < us)
		;
}

void usleep(unsigned us)
{
	udelay(us);
}

#ifndef CONFIG_CHIP_LFW_USE_ROM_SPI
int timestamp_expired(timestamp_t deadline, const timestamp_t *now)
{
	timestamp_t now_val;

	if (!now) {
		now_val = get_time();
		now = &now_val;
	}

	return ((int64_t)(now->val - deadline.val) >= 0);
}

/*
 * LFW does not use interrupts so no ISR will fire to
 * increment high 32-bits of timestap_t. Force high
 * word to zero. NOTE: There is a risk of false timeout
 * errors due to timer wrap. We will reset timer before
 * each SPI transaction.
 */
timestamp_t get_time(void)
{
	timestamp_t ts;

	ts.le.hi = 0;	/* clksrc_high; */
	ts.le.lo = __hw_clock_source_read();
	return ts;
}
#endif

void uart_write_c(char c)
{
	/* Put in carriage return prior to newline to mimic uart_vprintf() */
	if (c == '\n')
		uart_write_c('\r');

	/* Wait for space in transmit FIFO. */
	while (!(MEC17XX_UART_LSR(0) & (1 << 5)))
		;
	MEC17XX_UART_TB(0) = c;
}

void uart_puts(const char *str)
{
	if (!str || !*str)
		return;

	do {
		uart_write_c(*str++);
	} while (*str);
}

void fault_handler(void)
{
	uart_puts("EXCEPTION!\nTriggering watchdog reset\n");
	/* trigger reset in 1 ms */
	usleep(1000);
	MEC17XX_PCR_SYS_RST = MEC17XX_PCR_SYS_SOFT_RESET;
	while (1)
		;

}

void jump_to_image(uintptr_t init_addr)
{
	void (*resetvec)(void) = (void(*)(void))init_addr;

	resetvec();
}

void uart_init(void)
{
	/* Set UART to reset on VCC1_RESET instaed of nSIO_RESET */
	MEC17XX_UART_CFG(0) &= ~(1 << 1);

	/* Baud rate = 115200. 1.8432MHz clock. Divisor = 1 */

	/* Set CLK_SRC = 0 */
	MEC17XX_UART_CFG(0) &= ~(1 << 0);

	/* Set DLAB = 1 */
	MEC17XX_UART_LCR(0) |= (1 << 7);

	/* PBRG0/PBRG1 */
	MEC17XX_UART_PBRG0(0) = 1;
	MEC17XX_UART_PBRG1(0) = 0;

	/* Set DLAB = 0 */
	MEC17XX_UART_LCR(0) &= ~(1 << 7);

	/* Set word length to 8-bit */
	MEC17XX_UART_LCR(0) |= (1 << 0) | (1 << 1);

	/* Enable FIFO */
	MEC17XX_UART_FCR(0) = (1 << 0);

	/* Activate UART */
	MEC17XX_UART_ACT(0) |= (1 << 0);

	gpio_config_module(MODULE_UART, 1);
}

/*
 * If any of VTR POR, VBAT POR, chip resets, or WDT reset are active
 * force VBAT image type to none causing load of EC_RO.
 */
void system_init(void)
{
	uint32_t wdt_sts = MEC17XX_VBAT_STS & MEC17XX_VBAT_STS_ANY_RST;
	uint32_t rst_sts = MEC17XX_PCR_PWR_RST_STS &
				MEC17XX_PWR_RST_STS_VTR;

	trace12(0, LFW, 0,
		"VBAT_STS = 0x%08x  PCR_PWR_RST_STS = 0x%08x",
		wdt_sts, rst_sts);

	if (rst_sts || wdt_sts)
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX)
					= SYSTEM_IMAGE_UNKNOWN;
}

enum system_image_copy_t system_get_image_copy(void)
{
	return MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX);
}


/*
 * lfw_main is entered by MEC1701 BootROM or
 * EC_RO/RW calling it directly.
 * NOTE: Based on LFW from MEC1322
 * Upon chip reset, BootROM loads image = LFW+EC_RO and enters LFW.
 * LFW checks reset type:
 *   VTR POR, chip reset, WDT reset then set VBAT Load type to Unknown.
 * LFW reads VBAT Load type:
 *   SYSTEM_IMAGE_RO then read EC_RO from SPI flash and jump into it.
 *   SYSTEM_IMAGE_RO then read EC_RW from SPI flash and jump into it.
 *   Other then jump into EC image loaded by Boot-ROM.
 *
 */
void lfw_main(void)
{

	uintptr_t init_addr;
#ifdef LFW_DEBUG_USE_CHIP_SHA256
	/* struct sha12_ctx shactx; */
	uint32_t sha256_block2[64/4 * 2];
	uint32_t sha256_digest[32/4];
#endif

	/* install vector table */
	*((uintptr_t *) 0xe000ed08) = (uintptr_t) &hdr_int_vect;

	/* Use 48 MHz processor clock to power through boot */
	MEC17XX_PCR_PROC_CLK_CTL = 1;

#ifdef CONFIG_WATCHDOG
	/* Reload watchdog which may be running in case of sysjump */
	MEC17XX_WDG_KICK = 1;
#ifdef CONFIG_WATCHDOG_HELP
	/* Stop aux timer */
	MEC17XX_TMR16_CTL(0) &= ~1;
#endif
#endif
	/*
	 * TFDP functions will compile to nothing if CONFIG_MEC1701_TFDP
	 * is not defined.
	 */
	tfdp_power(1);
	tfdp_enable(1, 1);
	trace0(0, LFW, 0, "LFW first trace");

	timer_init();
	clock_init();
	cpu_init();
	dma_init();
	uart_init();
	system_init();

#ifdef LFW_DEBUG_USE_CHIP_SHA256
	rom_aes_sha_power(1);
	rom_aes_sha_reset();
#endif


#ifdef CONFIG_CHIP_LFW_USE_ROM_SPI
	gpio_config_module(MODULE_SPI_FLASH, 1);
	rom_qmspi_init(QMSPI_FREQ_24M, QMSPI_SPI_MODE0, QMSPI_IFCTRL_DFLT);
#else
	spi_enable(CONFIG_SPI_FLASH_PORT, 1);
#endif

	uart_puts("littlefw ");
	uart_puts(current_image_data.version);
	uart_puts("\n");

	switch (system_get_image_copy()) {
	case SYSTEM_IMAGE_RW:
		trace0(0, LFW, 0, "LFW EC_RW Load");
		uart_puts("lfw-RW load\n");

		init_addr = CONFIG_RW_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE;
#ifdef LFW_DEBUG_USE_CHIP_SHA256
		spi_image_load(CONFIG_EC_WRITABLE_STORAGE_OFF +
			       CONFIG_RW_STORAGE_OFF,
			       sha256_digest, sha256_block2);
#else
		spi_image_load(CONFIG_EC_WRITABLE_STORAGE_OFF +
			       CONFIG_RW_STORAGE_OFF);
#endif
		break;
	case SYSTEM_IMAGE_RO:
		trace0(0, LFW, 0, "LFW EC_RO Load");
		uart_puts("lfw-RO load\n");

#ifdef LFW_DEBUG_USE_CHIP_SHA256
		spi_image_load(CONFIG_EC_PROTECTED_STORAGE_OFF +
			       CONFIG_RO_STORAGE_OFF,
			       sha256_digest, sha256_block2);
#else
		spi_image_load(CONFIG_EC_PROTECTED_STORAGE_OFF +
			       CONFIG_RO_STORAGE_OFF);
#endif
		/* fall through */
	default:
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX) =
							SYSTEM_IMAGE_RO;

		init_addr = CONFIG_RO_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE;
	}

#ifdef LFW_DEBUG_USE_CHIP_SHA256
	rom_aes_sha_reset();
	rom_aes_sha_power(0);
#endif

	trace11(0, LFW, 0, "Get EC reset handler from 0x%08x", (init_addr + 4));
	trace11(0, LFW, 0, "Jump to EC @ 0x%08x",
		*((uint32_t *)(init_addr + 4)));
	jump_to_image(*(uintptr_t *)(init_addr + 4));

	/* should never get here */
	while (1)
		;
}
