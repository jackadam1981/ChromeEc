/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#define DT_DRV_COMPAT ite_it8xxx2_cros_flash

#include <flash.h>
#include <kernel.h>
#include <logging/log.h>

#include "console.h"
#include "drivers/cros_flash.h"
#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#include <soc/ite_it8xxx2/reg_def_cros.h>

LOG_MODULE_REGISTER(shim_flash, LOG_LEVEL_ERR);

#define CROS_FLASH_DEV DT_LABEL(DT_NODELABEL(fiu0))
static const struct device *cros_flash_dev;

struct k_sem sem;
static int all_protected;
static int addr_prot_start;
static int addr_prot_length;
static int stuck_locked;
static int inconsistent_locked;

#define CONFIG_FLASH_WRITE_IDEAL_SIZE 256

/* Memory mapping */
#define CHIP_ILM_BASE                 0x80000000
#define CONFIG_PROGRAM_MEMORY_BASE    CHIP_ILM_BASE
/* Program is run directly from storage */
#define CONFIG_MAPPED_STORAGE_BASE    CONFIG_PROGRAM_MEMORY_BASE

#define CONFIG_RAM_BASE               0x80100000
/* base+0000h~base+0FFF */
#define CHIP_RAMCODE_BASE             (CONFIG_RAM_BASE + 0)
/*
 * This is the block size of the ILM on the it83xx chip.
 * The ILM for static code cache, CPU fetch instruction from
 * ILM(ILM -> CPU)instead of flash(flash -> IMMU -> CPU) if enabled.
 */
#define IT83XX_ILM_BLOCK_SIZE         0x00001000


#define CONFIG_FLASH_SIZE_BYTES       0x00100000
#define CONFIG_FLASH_BANK_SIZE        0x00001000  /* protect bank size */
#define CONFIG_FLASH_ERASE_SIZE       0x00001000  /* erase bank size */
/* erase size of sector is 1KB or 4KB */
#define FLASH_SECTOR_ERASE_SIZE       CONFIG_FLASH_ERASE_SIZE


#define FLASH_IT8XXX2_REG_BASE	\
	((struct flash_it8xxx2_regs *)(DT_INST_REG_ADDR(0)))


extern const char __flash_dma_start;
#define FLASH_DMA_START ((uint32_t)&__flash_dma_start)
#define FLASH_DMA_CODE __attribute__((section(".flash_direct_map")))

/* page program command  */
#define FLASH_CMD_PAGE_WRITE   0x2
/* ector erase command (erase size is 4KB) */
#define FLASH_CMD_SECTOR_ERASE 0x20
/* command for flash write */
#define FLASH_CMD_WRITE        FLASH_CMD_PAGE_WRITE

/* Write status register */
#define FLASH_CMD_WRSR         0x01
/* Write disable */
#define FLASH_CMD_WRDI         0x04
/* Write enable */
#define FLASH_CMD_WREN         0x06
/* Read status register */
#define FLASH_CMD_RS           0x05

#define FWP_REG(bank) (bank / 8)
#define FWP_MASK(bank) (1 << (bank % 8))

enum flash_wp_interface {
	FLASH_WP_HOST = 0x01,
	FLASH_WP_DBGR = 0x02,
	FLASH_WP_EC = 0x04,
};

enum flash_wp_status {
	FLASH_WP_STATUS_PROTECT_RO = EC_FLASH_PROTECT_RO_NOW,
	FLASH_WP_STATUS_PROTECT_ALL = EC_FLASH_PROTECT_ALL_NOW,
};

enum flash_status_mask {
	FLASH_SR_NO_BUSY = 0,
	/* Internal write operation is in progress */
	FLASH_SR_BUSY = 0x01,
	/* Device is memory Write enabled */
	FLASH_SR_WEL = 0x02,

	FLASH_SR_ALL = (FLASH_SR_BUSY | FLASH_SR_WEL),
};

static int flash_check_prot_range(unsigned int offset, unsigned int bytes)
{
	/* Invalid value */
	if (offset + bytes > CONFIG_FLASH_SIZE_BYTES)
		return EC_ERROR_INVAL;

	/* Check if ranges overlap */
	if (MAX(addr_prot_start, offset) <
	    MIN(addr_prot_start + addr_prot_length, offset + bytes))
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;
}

/* TODO(b/174873770): Add calls to Zephyr code here */
#ifdef CONFIG_EXTERNAL_STORAGE
void flash_lock_mapped_storage(int lock)
{
	if (lock)
		mutex_lock(&flash_lock);
	else
		mutex_unlock(&flash_lock);
}
#endif

void FLASH_DMA_CODE dma_reset_immu(void)
{
	/* Immu tag sram reset */
	IT83XX_GCTRL_MCCR |= 0x10;

	IT83XX_GCTRL_MCCR &= ~0x10;
}

void FLASH_DMA_CODE dma_flash_follow_mode(void)
{
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;
	/*
	 * ECINDAR3-0 are EC-indirect memory address registers.
	 *
	 * Enter follow mode by writing 0xf to low nibble of ECINDAR3 register,
	 * and set high nibble as 0x4 to select internal flash.
	 */
	flash_regs->ECINDAR3 = (EC_INDIRECT_READ_INTERNAL_FLASH | 0x0F);

	/* Set FSCE# as high level by writing 0 to address xfff_fe00h */
	flash_regs->ECINDAR2 = 0xFF;
	flash_regs->ECINDAR1 = 0xFE;
	flash_regs->ECINDAR0 = 0x00;

	/* EC-indirect memory data register */
	flash_regs->ECINDDR = 0x00;
}

void FLASH_DMA_CODE dma_flash_follow_mode_exit(void)
{
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;

	/* Exit follow mode, and keep the setting of selecting internal flash */
	flash_regs->ECINDAR3 = EC_INDIRECT_READ_INTERNAL_FLASH;
	flash_regs->ECINDAR2 = 0x00;
}

void FLASH_DMA_CODE dma_flash_fsce_high(void)
{
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;

	/* FSCE# high level */
	flash_regs->ECINDAR1 = 0xFE;
	flash_regs->ECINDDR = 0x00;
}

void FLASH_DMA_CODE dma_flash_write_dat(uint8_t wdata)
{
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;

	/* Write data to FMOSI */
	flash_regs->ECINDDR = wdata;
}

void FLASH_DMA_CODE dma_flash_transaction(int wlen, uint8_t *wbuf, int rlen,
					  uint8_t *rbuf, int cmd_end)
{
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;
	int i;

	/*  FSCE# with low level */
	flash_regs->ECINDAR1 = 0xFD;
	/* Write data to FMOSI */
	for (i = 0; i < wlen; i++) {
		flash_regs->ECINDDR = wbuf[i];
	}
	/* Read data from FMISO */
	for (i = 0; i < rlen; i++) {
		rbuf[i] = flash_regs->ECINDDR;
	}
	/* FSCE# high level if transaction done */
	if (cmd_end) {
		dma_flash_fsce_high();
	}
}

void FLASH_DMA_CODE dma_flash_cmd_read_status(enum flash_status_mask mask,
					      enum flash_status_mask target)
{
	uint8_t status[1];
	uint8_t cmd_rs[] = {FLASH_CMD_RS};

	/*
	 * We prefer no timeout here. We can always get the status
	 * we want, or wait for watchdog triggered to check
	 * e-flash's status instead of breaking loop.
	 * This will avoid fetching unknown instruction from e-flash
	 * and causing exception.
	 */
	while (1) {
		/* read status */
		dma_flash_transaction(sizeof(cmd_rs), cmd_rs, 1, status, 1);
		/* only bit[1:0] valid */
		if ((status[0] & mask) == target) {
			break;
		}
	}

}

void FLASH_DMA_CODE dma_flash_cmd_write_enable(void)
{
	uint8_t cmd_we[] = {FLASH_CMD_WREN};

	/* enter EC-indirect follow mode */
	dma_flash_follow_mode();
	/* send write enable command */
	dma_flash_transaction(sizeof(cmd_we), cmd_we, 0, NULL, 1);
	/* read status and make sure busy bit cleared and write enabled. */
	dma_flash_cmd_read_status(FLASH_SR_ALL, FLASH_SR_WEL);
	/* exit EC-indirect follow mode */
	dma_flash_follow_mode_exit();
}

void FLASH_DMA_CODE dma_flash_cmd_write_disable(void)
{
	uint8_t cmd_wd[] = {FLASH_CMD_WRDI};

	/* enter EC-indirect follow mode */
	dma_flash_follow_mode();
	/* send write disable command */
	dma_flash_transaction(sizeof(cmd_wd), cmd_wd, 0, NULL, 1);
	/* make sure busy bit cleared. */
	dma_flash_cmd_read_status(FLASH_SR_ALL, FLASH_SR_NO_BUSY);
	/* exit EC-indirect follow mode */
	dma_flash_follow_mode_exit();
}

int FLASH_DMA_CODE dma_flash_verify(int addr, int size, const char *data)
{
	int i;
	uint8_t *wbuf = (uint8_t *)data;
	uint8_t *flash = (uint8_t *)addr;

	/* verify for erase */
	if (data == NULL) {
		for (i = 0; i < size; i++) {
			if (flash[i] != 0xFF)
				return EINVAL;
		}
	/* verify for write */
	} else {
		for (i = 0; i < size; i++) {
			if (flash[i] != wbuf[i])
				return EINVAL;
		}
	}

	return 0;
}

void FLASH_DMA_CODE dma_flash_cmd_write(int addr, int wlen, uint8_t *wbuf)
{
	int i;
	uint8_t flash_write[] = {FLASH_CMD_WRITE, ((addr >> 16) & 0xFF),
		((addr >> 8) & 0xFF), (addr & 0xFF)};

	/* enter EC-indirect follow mode */
	dma_flash_follow_mode();
	/* send flash write command (aai word or page program) */
	dma_flash_transaction(sizeof(flash_write), flash_write, 0, NULL, 0);

	for (i = 0; i < wlen; i++) {
		/* send data byte */
		dma_flash_write_dat(wbuf[i]);

		/*
		 * we want to restart the write sequence every IDEAL_SIZE
		 * chunk worth of data.
		 */
		if (!(++addr % CONFIG_FLASH_WRITE_IDEAL_SIZE)) {
			uint8_t w_en[] = {FLASH_CMD_WREN};

			dma_flash_fsce_high();
			/* make sure busy bit cleared. */
			dma_flash_cmd_read_status(FLASH_SR_BUSY, FLASH_SR_NO_BUSY);
			/* send write enable command */
			dma_flash_transaction(sizeof(w_en), w_en, 0, NULL, 1);
			/* make sure busy bit cleared and write enabled. */
			dma_flash_cmd_read_status(FLASH_SR_ALL, FLASH_SR_WEL);
			/* re-send write command */
			flash_write[1] = (addr >> 16) & 0xff;
			flash_write[2] = (addr >> 8) & 0xff;
			flash_write[3] = addr & 0xff;
			dma_flash_transaction(sizeof(flash_write), flash_write,
				0, NULL, 0);
		}
	}
	dma_flash_fsce_high();
	/* make sure busy bit cleared. */
	dma_flash_cmd_read_status(FLASH_SR_BUSY, FLASH_SR_NO_BUSY);
	/* exit EC-indirect follow mode */
	dma_flash_follow_mode_exit();
}

void FLASH_DMA_CODE dma_flash_write(int addr, int wlen,
						const char *wbuf)
{
	dma_flash_cmd_write_enable();
	dma_flash_cmd_write(addr, wlen, (uint8_t *)wbuf);
	dma_flash_cmd_write_disable();
}

void FLASH_DMA_CODE dma_flash_cmd_erase(int addr, int cmd)
{
	uint8_t cmd_erase[] = {cmd, ((addr >> 16) & 0xFF),
		((addr >> 8) & 0xFF), (addr & 0xFF)};

	/* enter EC-indirect follow mode */
	dma_flash_follow_mode();
	/* send erase command */
	dma_flash_transaction(sizeof(cmd_erase), cmd_erase, 0, NULL, 1);
	/* make sure busy bit cleared. */
	dma_flash_cmd_read_status(FLASH_SR_BUSY, FLASH_SR_NO_BUSY);
	/* exit EC-indirect follow mode */
	dma_flash_follow_mode_exit();
}

void FLASH_DMA_CODE dma_flash_erase(int addr, int cmd)
{
	dma_flash_cmd_write_enable();
	dma_flash_cmd_erase(addr, cmd);
	dma_flash_cmd_write_disable();
}

static enum flash_wp_status flash_check_wp(void)
{
	enum flash_wp_status wp_status;
	int all_bank_count, bank;

	all_bank_count = CONFIG_FLASH_SIZE_BYTES / CONFIG_FLASH_BANK_SIZE;

	for (bank = 0; bank < all_bank_count; bank++) {
		if (!(IT83XX_GCTRL_EWPR0PFEC(FWP_REG(bank)) & FWP_MASK(bank)))
			break;
	}

	if (bank == WP_BANK_COUNT)
		wp_status = FLASH_WP_STATUS_PROTECT_RO;
	else if (bank == (WP_BANK_COUNT + PSTATE_BANK_COUNT))
		wp_status = FLASH_WP_STATUS_PROTECT_RO;
	else if (bank == all_bank_count)
		wp_status = FLASH_WP_STATUS_PROTECT_ALL;
	else
		wp_status = 0;

	return wp_status;
}

/**
 * Protect flash banks until reboot.
 *
 * @param start_bank    Start bank to protect
 * @param bank_count    Number of banks to protect
 */
static void flash_protect_banks(int start_bank,
				int bank_count,
				enum flash_wp_interface wp_if)
{
	int bank;

	for (bank = start_bank; bank < start_bank + bank_count; bank++) {
		if (wp_if & FLASH_WP_EC)
			IT83XX_GCTRL_EWPR0PFEC(FWP_REG(bank)) |= FWP_MASK(bank);
		if (wp_if & FLASH_WP_HOST)
			IT83XX_GCTRL_EWPR0PFH(FWP_REG(bank)) |= FWP_MASK(bank);
		if (wp_if & FLASH_WP_DBGR)
			IT83XX_GCTRL_EWPR0PFD(FWP_REG(bank)) |= FWP_MASK(bank);
	}
}

int flash_physical_write(int offset, int size, const char *data)
{
	int rv = -EINVAL;
	unsigned int key;

	/* Fail if offset, size, and data aren't at least word-aligned */
	if ((offset | size | (uint32_t)(uintptr_t)data) &
	    (CONFIG_FLASH_WRITE_SIZE - 1))
		return EC_ERROR_INVAL;

	/* check protection */
	if (all_protected)
		return EC_ERROR_ACCESS_DENIED;

	/* check protection */
	if (flash_check_prot_range(offset, size))
		return EC_ERROR_ACCESS_DENIED;

	/* Lock physical flash operations */
	flash_lock_mapped_storage(1);

	/*
	 * CPU can't fetch instruction from flash while use
	 * EC-indirect follow mode to access flash, interrupts need to be
	 * disabled.
	 */
	key = irq_lock();

	dma_flash_write(offset, size, data);
	dma_reset_immu();
	/*
	 * Internal flash of N8 or RISC-V core is ILM(Instruction Local Memory)
	 * mapped, but RISC-V's ILM base address is 0x80000000.
	 *
	 * Ensure that we will get the ILM address of a flash offset.
	 */
	offset |= CONFIG_MAPPED_STORAGE_BASE;
	rv = dma_flash_verify(offset, size, data);

	irq_unlock(key);

	/* Unlock physical flash operations */
	flash_lock_mapped_storage(0);

	return rv;
}

int flash_physical_erase(int offset, int size)
{
	int rv = -EINVAL;
	unsigned int key;

	/* check protection */
	if (all_protected)
		return EC_ERROR_ACCESS_DENIED;

	/* check protection */
	if (flash_check_prot_range(offset, size))
		return EC_ERROR_ACCESS_DENIED;

	/* Lock physical flash operations */
	flash_lock_mapped_storage(1);

	/*
	 * CPU can't fetch instruction from flash while use
	 * EC-indirect follow mode to access flash, interrupts need to be
	 * disabled.
	 */
	key = irq_lock();

	/* Always use sector erase command (1K or 4K bytes) */
	for (; size > 0; size -= FLASH_SECTOR_ERASE_SIZE) {
		dma_flash_erase(offset, FLASH_CMD_SECTOR_ERASE);
		offset += FLASH_SECTOR_ERASE_SIZE;
	}
	dma_reset_immu();
	/* get the ILM address of a flash offset. */
	offset |= CONFIG_MAPPED_STORAGE_BASE;
	rv = dma_flash_verify(offset, size, NULL);

	irq_unlock(key);

	/* Unlock physical flash operations */
	flash_lock_mapped_storage(0);

	return rv;
}

int flash_physical_read(int offset, int size, char *data)
{
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;
	int rv = 0, i;

	/* Lock physical flash operations */
	flash_lock_mapped_storage(1);

	for (i = 0; i < size; i++) {
		flash_regs->ECINDAR3 = EC_INDIRECT_READ_INTERNAL_FLASH;
		flash_regs->ECINDAR2 = (offset >> 16) & 0xFF;
		flash_regs->ECINDAR1 = (offset >> 8) & 0xFF;
		flash_regs->ECINDAR0 = (offset & 0xFF);

		/*
		 * Read/Write to this register will access one byte on the
		 * flash with the 32-bit flash address defined in ECINDAR3-0
		 */
		data[i] = flash_regs->ECINDDR;

		offset++;
	}

	/* Unlock physical flash operations */
	flash_lock_mapped_storage(0);

	return rv;
}

int flash_physical_get_protect(int bank)
{
	return IT83XX_GCTRL_EWPR0PFEC(FWP_REG(bank)) & FWP_MASK(bank);
}

uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;

	flags |= flash_check_wp();

	if (all_protected)
		flags |= EC_FLASH_PROTECT_ALL_NOW;

	/* Check if blocks were stuck locked at pre-init */
	if (stuck_locked)
		flags |= EC_FLASH_PROTECT_ERROR_STUCK;

	/* Check if flash protection is in inconsistent state at pre-init */
	if (inconsistent_locked)
		flags |= EC_FLASH_PROTECT_ERROR_INCONSISTENT;

	return flags;
}

int flash_physical_protect_at_boot(uint32_t new_flags)
{
	return 0;
}

int flash_physical_protect_now(int all)
{
	if (all) {
		/* Protect the entire flash */
		flash_protect_banks(0,
			CONFIG_FLASH_SIZE_BYTES / CONFIG_FLASH_BANK_SIZE,
			FLASH_WP_EC);
		all_protected = 1;
	} else {
		/* Protect the read-only section and persistent state */
		flash_protect_banks(WP_BANK_OFFSET,
			WP_BANK_COUNT, FLASH_WP_EC);
	}

	/*
	 * bit[0], eflash protect lock register which can only be write 1 and
	 * only be cleared by power-on reset.
	 */
	IT83XX_GCTRL_EPLR |= 0x01;

	return EC_SUCCESS;
}

static void flash_code_static_dma(void)
{
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;
	unsigned int key;

	/* Make sure no interrupt while enable static DMA */
	key = irq_lock();

	/* invalid static DMA first */
	IT83XX_GCTRL_RVILMCR0 &= ~ILMCR_ILM1_ENABLE;

	flash_regs->SCAR0H = BIT(3);

	memcpy((void *)CHIP_RAMCODE_BASE, (const void *)FLASH_DMA_START,
		IT83XX_ILM_BLOCK_SIZE);

	/* RISCV ILM 1 Enable */
	IT83XX_GCTRL_RVILMCR0 |= ILMCR_ILM0_ENABLE;

	/*
	 * Enable ILM
	 * Set the logic memory address(flash code of RO/RW) in eflash
	 * by programming the register SCARx bit19-bit0.
	 */
	flash_regs->SCAR0L = FLASH_DMA_START & 0xFF;
	flash_regs->SCAR0M = (FLASH_DMA_START >> 8) & 0xFF;
	flash_regs->SCAR0H = (FLASH_DMA_START >> 16) & 0x7;

	if (FLASH_DMA_START & BIT(19))
		flash_regs->SCAR0H |= BIT(7);
	else
		flash_regs->SCAR0H &= ~BIT(7);

	/*
	 * Validate Direct-map SRAM function by programming
	 * register SCARx bit20=0
	 */
	flash_regs->SCAR0H &= ~BIT(4);

	irq_unlock(key);
}

static int flash_dev_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	int32_t reset_flags, prot_flags, unwanted_prot_flags;
	struct flash_it8xxx2_regs *const flash_regs = FLASH_IT8XXX2_REG_BASE;

	cros_flash_dev = device_get_binding(CROS_FLASH_DEV);
	if (!cros_flash_dev) {
		LOG_ERR("Fail to find %s", CROS_FLASH_DEV);
		return -ENODEV;
	}

	cros_flash_init(cros_flash_dev);

	/* By default, select internal flash for indirect fast read. */
	flash_regs->ECINDAR3 = EC_INDIRECT_READ_INTERNAL_FLASH;

	/*
	 * If the embedded flash's size of this part number is larger
	 * than 256K-byte, enable the page program cycle constructed
	 * by EC-Indirect Follow Mode.
	 */
	flash_regs->FLHCTRL6R |= IT8XXX2_SMFI_MASK_ECINDPP;

	/* Initialize mutex for flash controller */
	k_sem_init(&sem, 1, 1);

	flash_code_static_dma();

	reset_flags = system_get_reset_flags();
	prot_flags = flash_get_protect();
	unwanted_prot_flags = EC_FLASH_PROTECT_ALL_NOW |
		EC_FLASH_PROTECT_ERROR_INCONSISTENT;

	/*
	 * If we have already jumped between images, an earlier image could
	 * have applied write protection.  Nothing additional needs to be done.
	 */
	if (reset_flags & EC_RESET_FLAG_SYSJUMP)
		return EC_SUCCESS;

	if (prot_flags & EC_FLASH_PROTECT_GPIO_ASSERTED) {
		/* Protect the entire flash of host interface */
		flash_protect_banks(0,
			CONFIG_FLASH_SIZE_BYTES / CONFIG_FLASH_BANK_SIZE,
			FLASH_WP_HOST);
		/* Protect the entire flash of DBGR interface */
		flash_protect_banks(0,
			CONFIG_FLASH_SIZE_BYTES / CONFIG_FLASH_BANK_SIZE,
			FLASH_WP_DBGR);
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
	} else {
		/* Don't want RO flash protected */
		unwanted_prot_flags |= EC_FLASH_PROTECT_RO_NOW;
	}

	/* If there are no unwanted flags, done */
	if (!(prot_flags & unwanted_prot_flags))
		return EC_SUCCESS;

	/*
	 * If the last reboot was a power-on reset, it should have cleared
	 * write-protect.  If it didn't, then the flash write protect registers
	 * have been permanently committed and we can't fix that.
	 */
	if (reset_flags & EC_RESET_FLAG_POWER_ON) {
		stuck_locked = 1;
		return EC_ERROR_ACCESS_DENIED;
	} else {
		/*
		 * Set inconsistent flag, because there is no software
		 * reset can clear write-protect.
		 */
		inconsistent_locked = 1;
		return EC_ERROR_ACCESS_DENIED;
	}

	return EC_ERROR_UNKNOWN;
}

uint32_t flash_physical_get_valid_flags(void)
{
	return EC_FLASH_PROTECT_RO_AT_BOOT | EC_FLASH_PROTECT_RO_NOW |
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

/*
 * The priority flash_dev_init should be lower than GPIO initialization because
 * it calls gpio_get_level function.
 */
#if CONFIG_PLATFORM_EC_FLASH_INIT_PRIORITY <= \
	CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY
#error "Flash must be initialized after GPIOs"
#endif
SYS_INIT(flash_dev_init, POST_KERNEL, CONFIG_PLATFORM_EC_FLASH_INIT_PRIORITY);
