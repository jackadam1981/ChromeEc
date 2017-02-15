/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MEC1322 SoC little FW
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

#define USE_SHA256_CHIP

#ifdef USE_SHA256_CHIP
#include "sha256_chip.h"
#endif

void lfw_main(void);

__attribute__ ((section(".intvector")))
const struct int_vector_t hdr_int_vect = {
			(void *)0x11FA00, /* init sp, unused,
						set by MEC ROM loader*/
			&lfw_main,	  /* reset vector */
			&fault_handler,   /* NMI handler */
			&fault_handler,   /* HardFault handler */
			&fault_handler,   /* MPU fault handler */
			&fault_handler    /* Bus fault handler */
};

/* SPI devices - from glados/board.c*/
const struct spi_device_t spi_devices[] = {
	{ CONFIG_SPI_FLASH_PORT, 0, GPIO_SHD_CS0},
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);


#ifdef USE_SHA256_CHIP

#define TEST_SHA256_CHIP

#ifdef TEST_SHA256_CHIP
#define SHA256_TEST_PATTERN1_LEN 56
const uint8_t __attribute__((aligned(4))) test_pattern1[SHA256_TEST_PATTERN1_LEN+1] =
	"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
const uint32_t test_pattern1_sha256[SHA256_DIGEST_WORDLEN] = {
	0x248D6A61, 0xD20638B8,	0xE5C02693, 0x0C3E6039,
	0xA33CE459, 0x64FF2167, 0xF6ECEDD4, 0x19DB06C1
};
#endif
#endif


void timer_init()
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

	return spi_transaction(SPI_FLASH_DEVICE, cmd, 4, buf_usr, bytes);
}

/* TODO MCHP MEC1701
 * offset = 0x2080 for EC_RO
 *        = 0x40000 for EC_RW
 *
 * buf = 0x1000 + 0xE0000 = 0xE1000 OK, LFW is now 4KB(0x1000)
 * CONFIG_RO_SIZE = 188 * 1024
 *
 */
#ifdef USE_SHA256_CHIP

#define SPI_CHUNK_M64 (SPI_CHUNK_SIZE & 0x3F)
#define SPI_AL4 ((CONFIG_RW_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE) & 0x3F)

int spi_image_load(uint32_t offset, SHA12_CTX *pshactx)
{
	uint8_t *buf = (uint8_t *) (CONFIG_RW_MEM_OFF +
				    CONFIG_PROGRAM_MEMORY_BASE); /* = 0x1000 + 0x000E0000 = 0xE1000 */
	uint32_t i;

	BUILD_ASSERT(CONFIG_RO_SIZE == CONFIG_RW_SIZE);
	BUILD_ASSERT(SPI_CHUNK_M64 == 0);
	BUILD_ASSERT(SPI_AL4 == 0);

	memset((void *)buf, 0xFF, (CONFIG_RO_SIZE - 4)); /* Why fill all but last 4-bytes? */

	aes_sha_power(1);
	sha12_init(pshactx, MEC17XX_ROM_SHA_MODE_256);

	for (i = 0; i < CONFIG_RO_SIZE; i += SPI_CHUNK_SIZE) {
		spi_flash_readloc(&buf[i], offset + i, SPI_CHUNK_SIZE);
		sha12_update(pshactx, (const uint32_t *)&buf[i], SPI_CHUNK_SIZE);
	}

	sha12_finalize(pshactx);
	aes_sha_power(0);

	memset((void *)&pshactx->block.b[0], 0, 64);
	spi_flash_readloc(&pshactx->block.b[0], offset + CONFIG_RO_SIZE, 32);

	for (i = 0; i < 32; i++) {
		if (pshactx->block.b[0] != pshactx->digest.b[0]) {
			return -1;
		}
	}

	return 0;
}

#else

int spi_image_load(uint32_t offset)
{
	uint8_t *buf = (uint8_t *) (CONFIG_RW_MEM_OFF +
				    CONFIG_PROGRAM_MEMORY_BASE);
	uint32_t i;

	BUILD_ASSERT(CONFIG_RO_SIZE == CONFIG_RW_SIZE);

	memset((void *)buf, 0xFF, (CONFIG_RO_SIZE - 4)); /* Why fill all but last 4-bytes? */

	for (i = 0; i < CONFIG_RO_SIZE; i += SPI_CHUNK_SIZE) {
		spi_flash_readloc(&buf[i], offset + i, SPI_CHUNK_SIZE);
	}

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

int timestamp_expired(timestamp_t deadline, const timestamp_t *now)
{
	timestamp_t now_val;

	if (!now) {
		now_val = get_time();
		now = &now_val;
	}

	return ((uint32_t)(now->le.lo - deadline.le.lo) >= 0);
}


timestamp_t get_time(void)
{
	timestamp_t ts;

	ts.le.hi = 0;
	ts.le.lo = __hw_clock_source_read();
	return ts;
}

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
	MEC17XX_WDG_LOAD = 1;
	MEC17XX_WDG_CTL |= 1;
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

void system_init(void)
{

	uint32_t wdt_sts = MEC17XX_VBAT_STS & MEC17XX_VBAT_STS_WDT;
	uint32_t rst_sts = MEC17XX_PCR_PWR_RST_STS &
				MEC17XX_PWR_RST_STS_VTR;

	TRACE12(1, LFW, 0, "VBAT_STS = 0x%08x  PCR_PWR_RST_STS = 0x%08x",wdt_sts,rst_sts);

	if (rst_sts || wdt_sts)
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX)
					= SYSTEM_IMAGE_RO;
}

enum system_image_copy_t system_get_image_copy(void)
{
	return MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX);
}


void lfw_main(void)
{

	uintptr_t init_addr;
#ifdef USE_SHA256_CHIP
	SHA12_CTX shactx;
#ifdef TEST_SHA256_CHIP
	uint32_t i;
#endif
#endif
	/* install vector table */
	*((uintptr_t *) 0xe000ed08) = (uintptr_t) &hdr_int_vect;

	/* TODO MCHP DEBUG */
#if 1
	MEC17XX_EC_JTAG_EN = 0x03; /* SWD */
	MEC17XX_EC_JTAG_EN; /* read back to insure completion before break point */
#if 0
	__asm__ __volatile__ (
		"\t bkpt 17 \n"
		"\t nop \n"
		"\t nop \n"
		"\t nop \n"
	);
#endif
#endif
	/* Use 48 MHz processor clock to power through boot */
	MEC17XX_PCR_PROC_CLK_CTL = 1;

#if 1
	if (MEC17XX_PCR_PWR_RST_STS & MEC17XX_PWR_RST_STS_VTR) {
		__asm__ __volatile__ (
		"\t bkpt 17 \n"
		"\t nop \n"
		"\t nop \n"
		"\t nop \n"
	);
	}
#endif

#ifdef CONFIG_WATCHDOG
	/* Reload watchdog which may be running in case of sysjump */
	MEC17XX_WDG_KICK = 1;
#ifdef CONFIG_WATCHDOG_HELP
	/* Stop aux timer */
	MEC17XX_TMR16_CTL(0) &= ~1;
#endif
#endif
	/* TODO MCHP DEBUG */
	tfdp_power(1);
	tfdp_enable(1, 1);
	TRACE0(2, LFW, 0, "LFW first trace");

#ifdef USE_SHA256_CHIP
#ifdef TEST_SHA256_CHIP
	aes_sha_power(1);

	sha12_init(&shactx, MEC17XX_ROM_SHA_MODE_256);
	sha12_update(&shactx, (const uint32_t *)test_pattern1,
			SHA256_TEST_PATTERN1_LEN);
	sha12_finalize(&shactx);
	aes_sha_power(0);
	i = 0;
	while (i < SHA256_DIGEST_WORDLEN) {
		if (shactx.digest.w[i] != test_pattern1_sha256[i]) {
			break;
		}
		i++;
	}
	if (SHA256_DIGEST_WORDLEN != i) {
		TRACE0(3, LFW, 0, "!!! SHA256_CHIP Test pattern failure !!!");
	}
#endif
#endif

	timer_init();
	clock_init();
	cpu_init();
	dma_init();
	uart_init();
	system_init();
	spi_enable(CONFIG_SPI_FLASH_PORT, 1);

#if 0 /* TODO MCHP DEBUG */
	uart_puts("littlefw ");
	uart_puts(current_image_data.version);
	uart_puts("\n");
#endif

	switch (system_get_image_copy()) {
	case SYSTEM_IMAGE_RW:
		TRACE0(4, LFW, 0, "LFW EC_RW Load");
		/* uart_puts("lfw-RW load\n"); */
		/* TODO MCHP DEBUG
		 *
		 * chip/mec1701/config_chip.h
		 * #define CONFIG_PROGRAM_MEMORY_BASE	0x000E0000
		 *
		 *
		 * chip/mec1701/config_flash_layout.h
		 * #define CONFIG_LOADER_MEM_OFF		0
		 * #define CONFIG_LOADER_SIZE			0x1000
		 *
		 * #define CONFIG_RO_MEM_OFF	(CONFIG_LOADER_MEM_OFF + CONFIG_LOADER_SIZE) = 0x1000
		 *
		 * #define CONFIG_RW_MEM_OFF		CONFIG_RO_MEM_OFF = 0x1000
		 *
		 * #define CONFIG_RO_SIZE			(188 * 1024)
		 * #define CONFIG_RW_MEM_OFF			CONFIG_RO_MEM_OFF = 0x1000
		 * #define CONFIG_RW_SIZE			CONFIG_RO_SIZE = (188 * 1024)
		 *
		 * #define CONFIG_EC_PROTECTED_STORAGE_OFF	0x1000
		 *
		 * #define CONFIG_BOOT_HEADER_STORAGE_OFF	0
		 * #define CONFIG_BOOT_HEADER_STORAGE_SIZE	0x80
		 * #define CONFIG_LOADER_STORAGE_OFF	(CONFIG_BOOT_HEADER_STORAGE_OFF + CONFIG_BOOT_HEADER_STORAGE_SIZE) = 0x80
		 * #define CONFIG_RO_STORAGE_OFF	(CONFIG_LOADER_STORAGE_OFF + CONFIG_LOADER_SIZE) = 0x80 + 0x1000 = 0x1080
		 *
		 * #define CONFIG_EC_WRITABLE_STORAGE_OFF	0x40000
		 * #define CONFIG_RW_STORAGE_OFF		0
		 *
		 * init_addr = 0x1000 + 0xE0000 = 0xE1000 OK
		 *
		 * SPI Addr = 0x40000 + 0 = 0x40000
		 */
		init_addr = CONFIG_RW_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE;
#ifdef USE_SHA256_CHIP
		spi_image_load(CONFIG_EC_WRITABLE_STORAGE_OFF +
			       CONFIG_RW_STORAGE_OFF, &shactx);
#else
		spi_image_load(CONFIG_EC_WRITABLE_STORAGE_OFF +
			       CONFIG_RW_STORAGE_OFF);
#endif
		break;
	case SYSTEM_IMAGE_RO:
		TRACE0(5, LFW, 0, "LFW EC_RO Load");
		/* uart_puts("lfw-RO load\n"); */

		/* SPI Addr = 0x1000 + 0x1080 = 0x2080 */
#ifdef USE_SHA256_CHIP
		spi_image_load(CONFIG_EC_PROTECTED_STORAGE_OFF +
			       CONFIG_RO_STORAGE_OFF, &shactx);
#else
		spi_image_load(CONFIG_EC_PROTECTED_STORAGE_OFF +
			       CONFIG_RO_STORAGE_OFF);
#endif
		/* fall through */
	default:
		TRACE0(6, LFW, 0, "Set Init Address to EC_RO");
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX) =
							SYSTEM_IMAGE_RO;

		/* init_addr = 0x1000 + 0xE0000 = 0xE1000 OK */
		init_addr = CONFIG_RO_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE;
	}

	TRACE11(7, LFW, 0, "Jump to EC @ 0x%08x",(init_addr + 4));
	jump_to_image(*(uintptr_t *)(init_addr + 4));

	/* should never get here */
	while (1)
		;
}
