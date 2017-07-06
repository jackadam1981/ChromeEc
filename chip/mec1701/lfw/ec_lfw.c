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

#define USE_SHA256_CHIP

#ifdef USE_SHA256_CHIP
#include "rom_api_chip.h"
#include "sha256_chip.h"
#endif

#define LFW_SPI_BYTE_TRANSFER_TIMEOUT_US (1 * MSEC)
#define LFW_SPI_BYTE_TRANSFER_POLL_INTERVAL_US 100

void __attribute__((naked)) _lfw_reset(void);
void lfw_main(void);

extern uint32_t lfw_stack_top[];


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


/* SPI devices - from glados/board.c */
const struct spi_device_t spi_devices[] = {
	{ CONFIG_SPI_FLASH_PORT, 4, GPIO_SHD_CS0 },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);


/*
 * At POR or EC reset MEC17xx Boot-ROM loads LFW + EC_RO and jumps
 * into LFW entry point located at offset 0x04 of LFW.
 * Entry point is programmed into SPI Header by Python SPI image
 * builder at chip/mec1701/util/pack_ec.py
 *
 * We update Cortex-M4 vector table and stack pointer before
 * entering LFW main.
 *
 * EC_RO/RW calling LFW should enter through this routine if you
 * want the vector table updated. The stack should be set to
 * LFW linker file parameter lfw_stack_top because we do not
 * know if the callers stack is OK.
 *
 * TODO verify lfw_stack_top will not overwrite panic data!
 * from include/panic.h
 * Panic data goes at the end of RAM.  This is safe because we don't context
 * switch away from the panic handler before rebooting, and stacks and data
 * start at the beginning of RAM.
 *
 */


#ifdef USE_SHA256_CHIP

/* 10 ms */
#define TMOUT_SHA256_HW_UPDATE		(10000ul)
/* 1 ms */
#define TMOUT_SHA256_HW_FINAL		(1000ul)

#define TEST_SHA256_CHIP

#ifdef TEST_SHA256_CHIP
#define SHA256_TEST_PATTERN1_LEN 56
const uint8_t __aligned(4)
test_pattern1[SHA256_TEST_PATTERN1_LEN+1] =
	"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

/* Documented result is
 * 248D6A61 D20638B8 E5C02693 0C3E6039 A33CE459 64FF2167 F6ECEDD4 19DB06C1
 * This is a byte stream laid out in memory low(left) to high(right)
 */
const uint8_t __aligned(4)
test_pattern1_sha256[SHA256_DIGEST_BYTELEN] = {
	0x24, 0x8D, 0x6A, 0x61, 0xD2, 0x06, 0x38, 0xB8,
	0xE5, 0xC0, 0x26, 0x93, 0x0C, 0x3E, 0x60, 0x39,
	0xA3, 0x3C, 0xE4, 0x59, 0x64, 0xFF, 0x21, 0x67,
	0xF6, 0xEC, 0xED, 0xD4, 0x19, 0xDB, 0x06, 0xC1
};

#define SHA256_TEST_PATTERN2_LEN 96
const uint8_t __aligned(4)
test_pattern2[SHA256_TEST_PATTERN2_LEN] = {
	0xBF, 0xA6, 0xC8, 0xF0, 0xFD, 0x5C, 0xE5, 0x4A, 0x5F, 0x67,
	0x67, 0x35, 0x66, 0x39, 0x1E, 0x44, 0xA8, 0x92, 0x92, 0x5A,
	0xEB, 0xAD, 0xAF, 0x6A, 0x71, 0x04, 0x58, 0x1C, 0x2E, 0xDA,
	0xEE, 0x25, 0x92, 0x6D, 0xB8, 0x56, 0x13, 0x5B, 0xB4, 0x4E,
	0x6B, 0x3E, 0x7E, 0x87, 0x02, 0x5F, 0xCA, 0x88, 0x50, 0x0A,
	0xBB, 0xFA, 0x8B, 0x7A, 0xFC, 0x95, 0xEC, 0x2D, 0xB6, 0xB8,
	0xD9, 0x16, 0x72, 0x75, 0xEB, 0x67, 0x41, 0x31, 0x98, 0x4A,
	0x97, 0xFB, 0x5F, 0xD1, 0xBE, 0xB0, 0x70, 0xE7, 0x67, 0xC9,
	0xEA, 0xB1, 0x3C, 0x0C, 0xB4, 0xB2, 0x26, 0x49, 0xC7, 0x26,
	0xA7, 0xD7, 0x19, 0xF2, 0xC8, 0x8B
};

/*
 * openssl dgst -sha256 -out hex.txt rand96.bin
 * 2c99b17b91cbbc4f3df6b502d6a2fd618cde5003ca97d1696962d7771e301550
 */
const uint8_t __aligned(4)
test_pattern2_sha256[SHA256_DIGEST_BYTELEN] = {
	0x2c, 0x99, 0xb1, 0x7b, 0x91, 0xcb, 0xbc, 0x4f,
	0x3d, 0xf6, 0xb5, 0x02, 0xd6, 0xa2, 0xfd, 0x61,
	0x8c, 0xde, 0x50, 0x03, 0xca, 0x97, 0xd1, 0x69,
	0x69, 0x62, 0xd7, 0x77, 0x1e, 0x30, 0x15, 0x50
};

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

/*
 * One clock/byte max or (48MHz) 1.33 us/64 byte chunk + AHB transfer time.
 * Huge guard band of 32 us. (32 1MHz clocks)
 */
static int test_rom_sha256(uint32_t *block, uint32_t *digest,
			   const uint8_t *test_msg,
			   const uint8_t *expected_digest,
			   uint32_t msg_byte_len)
{
	uint64_t tmout;
	uint32_t i;
	uint8_t *p8;
	int rc;

	rom_sha_init(MEC17XX_ROM_SHA_MODE_256, digest);
	if ((msg_byte_len >> 6) != 0) {
		tmout = (msg_byte_len >> 6) << 5;
		rom_sha_update((const uint32_t *)test_msg,
			(msg_byte_len >> 6), 0x05);
		rc = hash_done(tmout);
		if (rc != EC_SUCCESS)
			return rc;

	}
	i = msg_byte_len & ~(0x3Ful);
	rom_sha_final(block, msg_byte_len,
		      &test_msg[i], 0x05);
	tmout = 4 * 32;
	rc = hash_done(tmout);
	if (rc != EC_SUCCESS)
		return rc;
	p8 = (uint8_t *)digest;
	for (i = 0; i < SHA256_DIGEST_BYTELEN; i++) {
		if (p8[i] != expected_digest[i])
			return EC_ERROR_CRC;
	}

	return EC_SUCCESS;
}

#endif

#endif


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
			TRACE11(1, LFW, 0,
				"LFW SPI timeout QMSPI.Status=0x%08x", u);
			return EC_ERROR_TIMEOUT;
		}
		u2 = u3;
	}

	return EC_SUCCESS;
}

/*
 * TODO MCHP MEC1701
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

	TRACE11(2, LFW, 0, "spi_load_image offset = 0x%08x", offset);

	for (m = 0; m < 2; m++) {
		TRACE1(3, LFW, 0, "SPI Freq = %d MHz", (24 >> m));
		/* Why fill all but last 4-bytes? */
		memset((void *)buf, 0xFF, (CONFIG_RO_SIZE - 4));
		for (n = 0; n < 2; n++) {
			TRACE11(4, LFW, 0, "SPI Read Cmd = 0x%08x",
				qmspi_rd_cmd_tbl[n]);
			rom_qmspi_init(qmspi_freq_tbl[m],
				QMSPI_SPI_MODE0, QMSPI_IFCTRL_DFLT);
			for (i = 0; i < CONFIG_RO_SIZE; i += SPI_CHUNK_SIZE) {
				rc = spi_flash_readloc(&buf[i >> 2], offset + i,
					SPI_CHUNK_SIZE, qmspi_rd_cmd_tbl[n]);
				if (rc != EC_SUCCESS) {
					TRACE12(5, LFW, 0,
						"chunk %d failed: rc=%d",
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
					TRACE0(6, LFW, 0,
						"SHA256 update timeout");
					continue;
				}

				i = ((CONFIG_RO_SIZE - 32) & ~(0x3Ful)) >> 2;
				rom_sha_final(block2, (CONFIG_RO_SIZE - 32),
					(const uint8_t *)&buf[i], 0x05);
				rc = hash_done(TMOUT_SHA256_HW_FINAL);
				if (rc != EC_SUCCESS) {
					TRACE0(7, LFW, 0,
						"SHA256 finalize timeout");
					continue;
				}

				i = 0;
				while (i < 32/4) {
					if (digest[i] != buf[
						(CONFIG_RO_SIZE - 32)/4 + i]) {
						TRACE0(8, LFW, 0,
						"LFW SHA256 mismatch");
						break;
					}
					i++;
				}
				if (i == 8) {
					TRACE0(9, LFW, 0, "LFW SHA256 OK");
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

#if 0 /* unused in new spi read code */
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
#if 0 /* TODO MCHP DEBUG */
	MEC17XX_WDG_LOAD = 1;
	MEC17XX_WDG_CTL |= 1;
#endif
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

	TRACE12(10, LFW, 0,
		"VBAT_STS = 0x%08x  PCR_PWR_RST_STS = 0x%08x",
		wdt_sts, rst_sts);

	/*
	 * TODO MCHP MEC1322/MEC1701 Boot-ROM has loaded an image comprised
	 * of LFW + EC_RO. The following if statement has the unintended?
	 * side effect of reading EC_RO again! Unlike the Boot-ROM load,
	 * the EC code performs no integrity checks of data read from SPI.
	 * Therefore:
	 *  WDT reset or VTR POR do NOT re-load EC_RO and use EC_RO
	 *  Boot-ROM has loaded.
	 *  VBAT POR only re-load using EC SPI code.
	 */
#if 1 /* leave enabled to test EC SPI read */
	if (rst_sts || wdt_sts)
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX)
					= SYSTEM_IMAGE_RO;
#else
	if (rst_sts || wdt_sts)
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX)
					= SYSTEM_IMAGE_UNKNOWN;
#endif
}

enum system_image_copy_t system_get_image_copy(void)
{
	return MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX);
}


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
 * #define CONFIG_RO_MEM_OFF	(CONFIG_LOADER_MEM_OFF +
 *	CONFIG_LOADER_SIZE) = 0x1000
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
 * #define CONFIG_LOADER_STORAGE_OFF	(CONFIG_BOOT_HEADER_STORAGE_OFF +
 *	CONFIG_BOOT_HEADER_STORAGE_SIZE) = 0x80
 * #define CONFIG_RO_STORAGE_OFF	(CONFIG_LOADER_STORAGE_OFF +
 *	CONFIG_LOADER_SIZE) = 0x80 + 0x1000 = 0x1080
 *
 * #define CONFIG_EC_WRITABLE_STORAGE_OFF	0x40000
 * #define CONFIG_RW_STORAGE_OFF		0
 *
 * init_addr = 0x1000 + 0xE0000 = 0xE1000 OK
 *
 * SPI Addr = 0x40000 + 0 = 0x40000
 */
void lfw_main(void)
{

	uintptr_t init_addr;
#ifdef USE_SHA256_CHIP
#ifdef TEST_SHA256_CHIP
	int rc;
#endif
	/* struct sha12_ctx shactx; */
	uint32_t sha256_block2[64/4 * 2];
	uint32_t sha256_digest[32/4];
#endif

	/* install vector table */
	*((uintptr_t *) 0xe000ed08) = (uintptr_t) &hdr_int_vect;

	/* TODO MCHP DEBUG */
#if 1
	MEC17XX_EC_JTAG_EN = 0x03; /* SWD */
	/* read back to insure completion before break point */
	MEC17XX_EC_JTAG_EN;
#if 0
	__asm__ __volatile__ ("bkpt 17;nop;nop;nop");
#endif
#endif
	/* Use 48 MHz processor clock to power through boot */
	MEC17XX_PCR_PROC_CLK_CTL = 1;

#if 0
	if (MEC17XX_PCR_PWR_RST_STS & MEC17XX_PWR_RST_STS_VTR)
		__asm__ __volatile__ ("bkpt 17;nop;nop;nop");
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
	TRACE0(11, LFW, 0, "LFW first trace");

	timer_init();
	clock_init();
	cpu_init();
	dma_init();
	uart_init();
	system_init();

#ifdef USE_SHA256_CHIP
	rom_aes_sha_power(1);
	rom_aes_sha_reset();

#ifdef TEST_SHA256_CHIP

	rc = test_rom_sha256(sha256_block2, sha256_digest,
			test_pattern1, test_pattern1_sha256,
			SHA256_TEST_PATTERN1_LEN);

	if (rc == EC_SUCCESS)
		TRACE0(12, LFW, 0, "SHA256_CHIP Test pattern 1 PASS");
	else
		TRACE0(13, LFW, 0, "SHA256_CHIP Test pattern 1 FAIL");

	rc = test_rom_sha256(sha256_block2, sha256_digest,
			test_pattern2, test_pattern2_sha256,
			SHA256_TEST_PATTERN2_LEN);

	if (rc == EC_SUCCESS)
		TRACE0(14, LFW, 0, "SHA256_CHIP Test pattern 2 PASS");
	else
		TRACE0(15, LFW, 0, "SHA256_CHIP Test pattern 2 FAIL");

#endif
#endif


	/* spi_enable(CONFIG_SPI_FLASH_PORT, 1); */
	gpio_config_module(MODULE_SPI_FLASH, 1);
	rom_qmspi_init(QMSPI_FREQ_24M, QMSPI_SPI_MODE0, QMSPI_IFCTRL_DFLT);

#if 0 /* TODO MCHP DEBUG */
	uart_puts("littlefw ");
	uart_puts(current_image_data.version);
	uart_puts("\n");
#endif

	switch (system_get_image_copy()) {
	case SYSTEM_IMAGE_RW:
		TRACE0(16, LFW, 0, "LFW EC_RW Load");
		/* TODO MCHP
		 * uart_puts("lfw-RW load\n");
		 */

		init_addr = CONFIG_RW_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE;
#ifdef USE_SHA256_CHIP
		spi_image_load(CONFIG_EC_WRITABLE_STORAGE_OFF +
			       CONFIG_RW_STORAGE_OFF,
			       sha256_digest, sha256_block2);
#else
		spi_image_load(CONFIG_EC_WRITABLE_STORAGE_OFF +
			       CONFIG_RW_STORAGE_OFF);
#endif
		break;
	case SYSTEM_IMAGE_RO:
		TRACE0(17, LFW, 0, "LFW EC_RO Load");
		/* uart_puts("lfw-RO load\n"); */

		/* SPI Addr = 0x1000 + 0x1080 = 0x2080 */
#ifdef USE_SHA256_CHIP
		spi_image_load(CONFIG_EC_PROTECTED_STORAGE_OFF +
			       CONFIG_RO_STORAGE_OFF,
			       sha256_digest, sha256_block2);
#else
		spi_image_load(CONFIG_EC_PROTECTED_STORAGE_OFF +
			       CONFIG_RO_STORAGE_OFF);
#endif
		/* fall through */
	default:
		TRACE0(18, LFW, 0, "Set Init Address to EC_RO");
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX) =
							SYSTEM_IMAGE_RO;

		/* init_addr = 0x1000 + 0xE0000 = 0xE1000 OK */
		init_addr = CONFIG_RO_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE;
	}

#ifdef USE_SHA256_CHIP
	rom_aes_sha_reset();
	rom_aes_sha_power(0);
#endif

	TRACE11(19, LFW, 0, "Get EC reset handler from 0x%08x",
		(init_addr + 4));
	TRACE11(20, LFW, 0, "Jump to EC @ 0x%08x",
		*((uint32_t *)(init_addr + 4)));
	jump_to_image(*(uintptr_t *)(init_addr + 4));

	/* should never get here */
	while (1)
		;
}
