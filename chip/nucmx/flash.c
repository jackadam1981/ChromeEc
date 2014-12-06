/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for Chrome EC */

#include "flash.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"
#include "console.h"

/* Console output macros */
#if !DEBUG_FLH
#undef ccputs
#define ccputs(...)
#undef ccprintf
#define ccprintf(...)
#endif

int all_protected; /* Has all-flash protection been requested? */
int addr_prot_start;
int addr_prot_lens;
/*
 *  Flash commands for the W25Q16CV SPI flash
 */
#define CMD_READ_ID				0x9F
#define CMD_WRITE_EN			0x06
#define CMD_WRITE_STATUS		0x50
#define CMD_READ_STATUS_REG		0x05
#define CMD_READ_STATUS_REG2	0x35
#define CMD_WRITE_STATUS_REG	0x01
#define CMD_FLASH_PROGRAM		0x02
#define CMD_SECTOR_ERASE		0x20
#define CMD_PROGRAM_UINT_SIZE	0x08
#define CMD_PAGE_SIZE			0x00
#define CMD_READ_ID_TYPE		0x47
/*
 * Status registers for the W25Q16CV SPI flash
 */
#define SPI_FLASH_SR2_SUS		(1 << 7)
#define SPI_FLASH_SR2_CMP		(1 << 6)
#define SPI_FLASH_SR2_LB3		(1 << 5)
#define SPI_FLASH_SR2_LB2		(1 << 4)
#define SPI_FLASH_SR2_LB1		(1 << 3)
#define SPI_FLASH_SR2_QE		(1 << 1)
#define SPI_FLASH_SR2_SRP1		(1 << 0)
#define SPI_FLASH_SR1_SRP0		(1 << 7)
#define SPI_FLASH_SR1_SEC		(1 << 6)
#define SPI_FLASH_SR1_TB		(1 << 5)
#define SPI_FLASH_SR1_BP2		(1 << 4)
#define SPI_FLASH_SR1_BP1		(1 << 3)
#define SPI_FLASH_SR1_BP0		(1 << 2)
#define SPI_FLASH_SR1_WEL		(1 << 1)
#define SPI_FLASH_SR1_BUSY		(1 << 0)


/* 0: F_CS0 1: F_CS1_1(GPIO86) 2:F_CS1_2(GPIOA6) */
#define FIU_CHIP_SELECT 		0
/* Create UMA control mask */
#define MASK(bit)    			(0x1 << (bit))
#define A_SIZE              	0x03	/* 0: No ADR field 1: 3-bytes ADR field */
#define C_SIZE              	0x04	/* 0: 1-Byte CMD field 1:No CMD field */
#define RD_WR               	0x05	/* 0: Read 1: Write */
#define DEV_NUM               	0x06	/* 0: PVT is used 1: SHD is used */
#define EXEC_DONE           	0x07
#define D_SIZE_1				0x01
#define D_SIZE_2				0x02
#define D_SIZE_3				0x03
#define D_SIZE_4				0x04
#define FLASH_SEL				MASK(DEV_NUM)

#define MASK_CMD_ONLY			(MASK(EXEC_DONE) | FLASH_SEL)
#define MASK_CMD_ADR			(MASK(EXEC_DONE) | FLASH_SEL | MASK(A_SIZE))
#define MASK_RD_1BYTE			(MASK(EXEC_DONE) | FLASH_SEL | MASK(C_SIZE) | D_SIZE_1)
#define MASK_RD_2BYTE			(MASK(EXEC_DONE) | FLASH_SEL | MASK(C_SIZE) | D_SIZE_2)
#define MASK_RD_3BYTE			(MASK(EXEC_DONE) | FLASH_SEL | MASK(C_SIZE) | D_SIZE_3)
#define MASK_RD_4BYTE			(MASK(EXEC_DONE) | FLASH_SEL | MASK(C_SIZE) | D_SIZE_4)
#define MASK_CMD_RD_1BYTE		(MASK(EXEC_DONE) | FLASH_SEL | D_SIZE_1)
#define MASK_CMD_RD_2BYTE		(MASK(EXEC_DONE) | FLASH_SEL | D_SIZE_2)
#define MASK_CMD_RD_3BYTE		(MASK(EXEC_DONE) | FLASH_SEL | D_SIZE_3)
#define MASK_CMD_RD_4BYTE		(MASK(EXEC_DONE) | FLASH_SEL | D_SIZE_4)
#define MASK_CMD_WR_ONLY 		(MASK(EXEC_DONE) | FLASH_SEL | MASK(RD_WR))
#define MASK_CMD_WR_1BYTE 		(MASK(EXEC_DONE) | FLASH_SEL | MASK(RD_WR)	| MASK(C_SIZE) | D_SIZE_1)
#define MASK_CMD_WR_2BYTE 		(MASK(EXEC_DONE) | FLASH_SEL | MASK(RD_WR)	| MASK(C_SIZE) | D_SIZE_2)
#define MASK_CMD_WR_ADR			(MASK(EXEC_DONE) | FLASH_SEL | MASK(RD_WR)	| MASK(A_SIZE))

/*****************************************************************************/
/* flash internal functions */
void flash_pinmux(int enable)
{
	if(enable){
		CLEAR_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_NO_F_SPI);
	}
	else{
		SET_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_NO_F_SPI);
	}

	/* CS0/1 pinmux */
	if(enable){
#if (FIU_CHIP_SELECT == 1)
		SET_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_F_SPI_CS1_1);
#elif (FIU_CHIP_SELECT == 2)
		SET_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_F_SPI_CS1_2);
#endif
	}
	else{
		CLEAR_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_F_SPI_CS1_1);
		CLEAR_BIT(NUCMX_DEVALT(0),NUCMX_DEVALT0_F_SPI_CS1_2);
	}
}

void flash_tristate(int enable)
{
	if(enable){
		/* Enable FIU pins to tri-state */
		SET_BIT(NUCMX_DEVCNT, NUCMX_DEVCNT_F_SPI_TRIS);
	}
	else{
		/* Disable FIU pins to tri-state */
		CLEAR_BIT(NUCMX_DEVCNT, NUCMX_DEVCNT_F_SPI_TRIS);
	}
}

void flash_execute_cmd(uint8_t code, uint8_t cts)
{
	/* set UMA_CODE */
    NUCMX_UMA_CODE = code;
    /* execute UMA flash transaction */
    NUCMX_UMA_CTS  = cts;
    while (IS_BIT_SET(NUCMX_UMA_CTS, NUCMX_UMA_CTS_EXEC_DONE));
}

void flash_cs_level(int level)
{
	/* level is high */
	if(level){
		/* Set chip select to high */
		SET_BIT(NUCMX_UMA_ECTS, NUCMX_UMA_ECTS_SW_CS1);
	}
	/* level is low */
	else{
		/* Set chip select to low */
		CLEAR_BIT(NUCMX_UMA_ECTS, NUCMX_UMA_ECTS_SW_CS1);
	}
}

void flash_wait_ready(void)
{
    uint8_t mask = SPI_FLASH_SR1_BUSY;

    /* Chip Select down. */
    flash_cs_level(0);
    /* Command for Read status register */
    flash_execute_cmd(CMD_READ_STATUS_REG, MASK_CMD_ONLY);
    do
    {
    	/* Read status register */
    	NUCMX_UMA_CTS  = MASK_RD_1BYTE;
    	while (IS_BIT_SET(NUCMX_UMA_CTS, NUCMX_UMA_CTS_EXEC_DONE));
    } while (NUCMX_UMA_DB0 & mask); /* Wait for Busy clear */
    /* Chip Select high. */
	flash_cs_level(1);
}

int flash_write_enable(void)
{
	uint8_t mask = SPI_FLASH_SR1_WEL;
	/* Write enable command */
    flash_execute_cmd(CMD_WRITE_EN, MASK_CMD_ONLY);
    /* Wait for flash is not busy */
    flash_wait_ready();

    if(NUCMX_UMA_DB0 & mask)
    	return 1;
    else
    	return 0;
}

void flash_set_address(uint32_t dest_addr)
{
    uint8_t * addr = (uint8_t *)&dest_addr;
    // Write address
    NUCMX_UMA_AB2 = addr[2];
    NUCMX_UMA_AB1 = addr[1];
    NUCMX_UMA_AB0 = addr[0];
}

uint8_t flash_get_status1(void)
{
	/* Disable tri-state */
	flash_tristate(0);
	/* Read status register1 */
	flash_execute_cmd(CMD_READ_STATUS_REG, MASK_CMD_RD_1BYTE);
	/* Enable tri-state */
	flash_tristate(1);
	return NUCMX_UMA_DB0;
}

uint8_t flash_get_status2(void)
{
	/* Disable tri-state */
	flash_tristate(0);
	/* Read status register2 */
	flash_execute_cmd(CMD_READ_STATUS_REG2, MASK_CMD_RD_1BYTE);
	/* Enable tri-state */
	flash_tristate(1);
	return NUCMX_UMA_DB0;
}

/*****************************************************************************/
/* flash protection functions */
/* Use a copy function of spi_flash.c in flash driver */
/**
 * Computes block write protection range from registers
 * Returns start == len == 0 for no protection
 *
 * @param sr1 Status register 1
 * @param sr2 Status register 2
 * @param start Output pointer for protection start offset
 * @param len Output pointer for protection length
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
static int reg_to_protect(uint8_t sr1, uint8_t sr2, unsigned int *start,
	unsigned int *len)
{
	int blocks;
	int size;
	uint8_t cmp;
	uint8_t sec;
	uint8_t tb;
	uint8_t bp;

	/* Determine flags */
	cmp = (sr2 & SPI_FLASH_SR2_CMP) ? 1 : 0;
	sec = (sr1 & SPI_FLASH_SR1_SEC) ? 1 : 0;
	tb = (sr1 & SPI_FLASH_SR1_TB) ? 1 : 0;
	bp = (sr1 & (SPI_FLASH_SR1_BP2 | SPI_FLASH_SR1_BP1 | SPI_FLASH_SR1_BP0))
		 >> 2;

	/* Bad pointers or invalid data */
	if (!start || !len || sr1 == -1 || sr2 == -1)
		return EC_ERROR_INVAL;

	/* Not defined by datasheet */
	if (sec && bp == 6)
		return EC_ERROR_INVAL;

	/* Determine granularity (4kb sector or 64kb block) */
	/* Computation using 2 * 1024 is correct */
	size = sec ? (2 * 1024) : (64 * 1024);

	/* Determine number of blocks */
	/* Equivalent to pow(2, bp) with pow(2, 0) = 0 */
	blocks = bp ? (1 << bp) : 0;
	/* Datasheet specifies don't care for BP == 4, BP == 5 */
	if (sec && bp == 5)
		blocks = (1 << 4);

	/* Determine number of bytes */
	*len = size * blocks;

	/* Determine bottom/top of memory to protect */
	*start = tb ? 0 :
			(CONFIG_FLASH_SIZE - *len) % CONFIG_FLASH_SIZE;

	/* Reverse computations if complement set */
	if (cmp) {
		*start = (*start + *len) % CONFIG_FLASH_SIZE;
		*len = CONFIG_FLASH_SIZE - *len;
	}

	return EC_SUCCESS;
}

/**
 * Computes block write protection registers from range
 *
 * @param start Desired protection start offset
 * @param len Desired protection length
 * @param sr1 Output pointer for status register 1
 * @param sr2 Output pointer for status register 2
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
static int protect_to_reg(unsigned int start, unsigned int len,
	uint8_t *sr1, uint8_t *sr2)
{
	char cmp = 0;
	char sec = 0;
	char tb = 0;
	char bp = 0;
	int blocks;
	int size;

	/* Bad pointers */
	if (!sr1 || !sr2 || *sr1 == -1 || *sr2 == -1)
		return EC_ERROR_INVAL;

	/* Invalid data */
	if ((start && !len) || start + len > CONFIG_FLASH_SIZE)
		return EC_ERROR_INVAL;

	/* Set complement bit based on whether length is power of 2 */
	if ((len & (len - 1)) != 0) {
		cmp = 1;
		start = (start + len) % CONFIG_FLASH_SIZE;
		len = CONFIG_FLASH_SIZE - len;
	}

	/* Set bottom/top bit based on start address */
	/* Do not set if len == 0 or len == CONFIG_FLASH_SIZE */
	if (!start && (len % CONFIG_FLASH_SIZE))
		tb = 1;

	/* Set sector bit and determine block length based on protect length */
	if (len == 0 || len >= 128 * 1024) {
		sec = 0;
		size = 64 * 1024;
	} else if (len >= 4 * 1024 && len <= 32 * 1024) {
		sec = 1;
		size = 2 * 1024;
	} else
		return EC_ERROR_INVAL;

	/* Determine number of blocks */
	if (len % size != 0)
		return EC_ERROR_INVAL;
	blocks = len / size;

	/* Determine bp = log2(blocks) with log2(0) = 0 */
	bp = blocks ? (31 - __builtin_clz(blocks)) : 0;

	/* Clear bits */
	*sr1 &= ~(SPI_FLASH_SR1_SEC | SPI_FLASH_SR1_TB |
		SPI_FLASH_SR1_BP2 | SPI_FLASH_SR1_BP1 | SPI_FLASH_SR1_BP0);
	*sr2 &= ~SPI_FLASH_SR2_CMP;

	/* Set bits */
	*sr1 |= (sec ? SPI_FLASH_SR1_SEC : 0) | (tb ? SPI_FLASH_SR1_TB : 0)
			| (bp << 2);
	*sr2 |= (cmp ? SPI_FLASH_SR2_CMP : 0);

	return EC_SUCCESS;
}

int flash_set_status_for_prot(int reg1, int reg2)
{
	/* Disable tri-state */
	flash_tristate(0);
	/* Enable write */
	flash_write_enable();

	NUCMX_UMA_DB0 = reg1;
	NUCMX_UMA_DB1 = reg2;

	/* Write status register 1/2 */
    flash_execute_cmd(CMD_WRITE_STATUS_REG, MASK_CMD_WR_2BYTE);
    /* Enable tri-state */
	flash_tristate(1);

	reg_to_protect(reg1, reg2, &addr_prot_start, &addr_prot_lens);

	return EC_SUCCESS;
}

int flash_check_prot_range(unsigned int offset, unsigned int bytes)
{
	/* Invalid value */
	if (offset + bytes > CONFIG_FLASH_SIZE)
		return EC_ERROR_INVAL;
	/* Check if ranges overlap */
	if (MAX(addr_prot_start, offset) < MIN(addr_prot_start + addr_prot_lens, offset + bytes))
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;
}

int flash_check_prot_reg(unsigned int offset, unsigned int bytes)
{
	unsigned int start;
	unsigned int len;
	uint8_t sr1 = 0, sr2 = 0;
	int rv = EC_SUCCESS;

	sr1 = flash_get_status1();
	sr2 = flash_get_status2();

	/* Invalid value */
	if (offset + bytes > CONFIG_FLASH_SIZE)
		return EC_ERROR_INVAL;

	/* Compute current protect range */
	rv = reg_to_protect(sr1, sr2, &start, &len);
	if (rv)
		return rv;

	/* Check if ranges overlap */
	if (MAX(start, offset) < MIN(start + len, offset + bytes))
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;

}

int flash_write_prot_reg(unsigned int offset, unsigned int bytes)
{
	int rv;
	uint8_t sr1 = flash_get_status1();
	uint8_t sr2 = flash_get_status2();

	/* Invalid values */
	if (offset + bytes > CONFIG_FLASH_SIZE)
		return EC_ERROR_INVAL;

	/* Compute desired protect range */
	rv = protect_to_reg(offset, bytes, &sr1, &sr2);
	if (rv)
		return rv;

	ccprintf("Chk Addr is 0x%08X - 0x%08X \n",offset, offset+bytes);
	ccprintf("STS-W reg1 is 0x%02X, reg2 is 0x%02X\n",sr1,sr2);
	return flash_set_status_for_prot(sr1, sr2);
}

void flash_burst_write(unsigned int dest_addr, unsigned int bytes, const char *data)
{
	unsigned int i;
	/* Chip Select down. */
	flash_cs_level(0);
	/* Set erase address */
	flash_set_address(dest_addr);
	/* Start write */
	flash_execute_cmd(CMD_FLASH_PROGRAM, MASK_CMD_WR_ADR);
	for(i=0; i<bytes; i++){
		flash_execute_cmd(*data, MASK_CMD_WR_ONLY);
		data++;
	}
	/* Chip Select up */
	flash_cs_level(1);
}
/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_write(int offset, int size, const char *data)
{
	int dest_addr = offset;
	const int sz_page = CONFIG_FLASH_WRITE_IDEAL_SIZE;

	/* Fail if offset, size, and data aren't at least word-aligned */
	if ((offset | size | (uint32_t)(uintptr_t)data) & (CONFIG_FLASH_WRITE_SIZE - 1))
		return EC_ERROR_INVAL;

	/* check protection */
	if (all_protected)
		return EC_ERROR_ACCESS_DENIED;

	/* Disable tri-state */
	flash_tristate(0);

	/* Write the data per CONFIG_FLASH_WRITE_IDEAL_SIZE bytes */
	for (; size >= sz_page; size -= sz_page) {

		/* check protection */
		if(flash_check_prot_range(dest_addr, sz_page))
			return EC_ERROR_ACCESS_DENIED;

		/* Enable write */
		flash_write_enable();
		/* Burst UMA transaction */
		flash_burst_write(dest_addr, sz_page, data);
		/* Wait write completed */
		flash_wait_ready();

		data += sz_page;
		dest_addr += sz_page;
	}

	/* Handle final partial page, if any */
	if(size !=0){
		/* check protection */
		if(flash_check_prot_range(dest_addr, size))
			return EC_ERROR_ACCESS_DENIED;

		/* Enable write */
		flash_write_enable();
		/* Burst UMA transaction */
		flash_burst_write(dest_addr, size, data);
		/* Wait write completed */
		flash_wait_ready();
	}

	/* Enable tri-state */
	flash_tristate(1);
	return EC_SUCCESS;
}

int flash_physical_erase(int offset, int size)
{
	/* check protection */
	if (all_protected)
		return EC_ERROR_ACCESS_DENIED;

	/* Disable tri-state */
	flash_tristate(0);

	/* Alignment has been checked in upper layer */
	for (; size > 0; size -= CONFIG_FLASH_ERASE_SIZE,
		     offset += CONFIG_FLASH_ERASE_SIZE) {

		/* Do nothing if already erased */
		if (flash_is_erased(offset, CONFIG_FLASH_ERASE_SIZE))
			continue;

		/* check protection */
		if(flash_check_prot_range(offset, CONFIG_FLASH_ERASE_SIZE))
			return EC_ERROR_ACCESS_DENIED;


		/*
		 * Reload the watchdog timer, so that erasing many flash pages
		 * doesn't cause a watchdog reset.  May not need this now that
		 * we're using msleep() below.
		 */
		watchdog_reload();

		/* Enable write */
		flash_write_enable();
		/* Set erase address */
        flash_set_address(offset);
        /* Start erase */
        flash_execute_cmd(CMD_SECTOR_ERASE,MASK_CMD_ADR);

        /* Wait erase completed */
        flash_wait_ready();
	}

	/* Enable tri-state */
	flash_tristate(1);
	return EC_SUCCESS;
}

int flash_physical_get_protect(int bank)
{
	uint32_t addr = bank * CONFIG_FLASH_BANK_SIZE;
	return flash_check_prot_reg(addr, CONFIG_FLASH_BANK_SIZE);
}

uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;

	/* Read all-protected state from our shadow copy */
	if (all_protected)
		flags |= EC_FLASH_PROTECT_ALL_NOW;

	return flags;
}

int flash_physical_protect_now(int all)
{
	if (all) {
		/* Protect the entire flash */
		all_protected = 1;
		flash_write_prot_reg(0, CONFIG_FLASH_PHYSICAL_SIZE);
	} else {
		/* Protect the read-only section and persistent state */
#ifdef CONFIG_PSTATE_AT_END
		flash_write_prot_reg(RO_BANK_OFFSET*CONFIG_FLASH_BANK_SIZE, RO_BANK_COUNT*CONFIG_FLASH_BANK_SIZE);
		flash_write_prot_reg(PSTATE_BANK   *CONFIG_FLASH_BANK_SIZE, CONFIG_FLASH_BANK_SIZE);
#else
		/* PSTATE immediately follows RO, in the first half of flash */
		flash_write_prot_reg(RO_BANK_OFFSET*CONFIG_FLASH_BANK_SIZE, (RO_BANK_COUNT+1)*CONFIG_FLASH_BANK_SIZE);
#endif
	}

	return EC_SUCCESS;
}

uint32_t flash_physical_get_valid_flags(void)
{
	return EC_FLASH_PROTECT_RO_AT_BOOT |
	       EC_FLASH_PROTECT_RO_NOW |
	       EC_FLASH_PROTECT_ALL_NOW;
}

uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	uint32_t ret = 0;

	/* If RO protection isn't enabled, its at-boot state can be changed. */
	if (!(cur_flags & EC_FLASH_PROTECT_RO_NOW))
		ret |= EC_FLASH_PROTECT_RO_AT_BOOT;

	/*
	 * If entire flash isn't protected at this boot, it can be enabled if
	 * the WP GPIO is asserted.
	 */
	if (!(cur_flags & EC_FLASH_PROTECT_ALL_NOW) &&
	    (cur_flags & EC_FLASH_PROTECT_GPIO_ASSERTED))
		ret |= EC_FLASH_PROTECT_ALL_NOW;

	return ret;
}

/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
	uint32_t reset_flags, prot_flags, unwanted_prot_flags;

	/* Enable FIU interface */
    flash_pinmux(1);

	reset_flags = system_get_reset_flags();
	prot_flags = flash_get_protect();
	unwanted_prot_flags = EC_FLASH_PROTECT_ALL_NOW |
		EC_FLASH_PROTECT_ERROR_INCONSISTENT;

	/*
	 * If we have already jumped between images, an earlier image could
	 * have applied write protection.  Nothing additional needs to be done.
	 */
	if (reset_flags & RESET_FLAG_SYSJUMP)
		return EC_SUCCESS;

	/* Handle flash write-protection */
	if (prot_flags & EC_FLASH_PROTECT_GPIO_ASSERTED) {
		/*
		 * Write protect is asserted.  If we want RO flash protected,
		 * protect it now.
		 */
		if ((prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT) &&
		    !(prot_flags & EC_FLASH_PROTECT_RO_NOW)) {
			int rv = flash_set_protect(EC_FLASH_PROTECT_RO_NOW,
						   EC_FLASH_PROTECT_RO_NOW);
			if (rv)
				return rv;

			/* Re-read flags */
			prot_flags = flash_get_protect();
		}

		/* Update all-now flag if all flash is protected */
		if (prot_flags & EC_FLASH_PROTECT_ALL_NOW)
			all_protected = 1;
	} else {
		/* Don't want RO flash protected */
		unwanted_prot_flags |= EC_FLASH_PROTECT_RO_NOW;
	}

	/* If there are no unwanted flags, done */
	if (!(prot_flags & unwanted_prot_flags))
		return EC_SUCCESS;

	/* Otherwise, clear the flash protection bits of status registers */
	flash_set_status_for_prot(0, 0);

	return EC_SUCCESS;
}
