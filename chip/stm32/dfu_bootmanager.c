/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bkpdata.h"
#include "clock.h"
#include "common.h"
#include "dfu_bootmanager.h"
#include "registers.h"
#include "task.h"

#ifdef CONFIG_DFU_BOOTMANAGER_MAX_REBOOT_COUNT
#if CONFIG_DFU_BOOTMANAGER_MAX_REBOOT_COUNT <= 0 || \
	CONFIG_DFU_BOOTMANAGER_MAX_REBOOT_COUNT > DFU_BOOTMANAGER_VALUE_DFU
#error "Max reboot count is out of range"
#endif
#endif /* CONFIG_DFU_BOOTMANAGER_MAX_REBOOT_COUNT */

/*
 * The Servo platforms do not have any free backup regions. The scratchpad
 * is only with the console command scratchpad and on some of the tests so
 * we'll use the scratchpad region.
 */
#ifndef CONFIG_CMD_SCRATCHPAD

/*
 * Write the value to the backup registers and sets the bitmasks
 * indicating the field is valid.
 *
 * @param value  New value to store.
 */
static void dfu_bootmanager_backup_write(uint8_t value)
{
	uint16_t data = DFU_BOOTMANAGER_VALID_CHECK | value;

	bkpdata_write(BKPDATA_INDEX_SCRATCHPAD, data);
}

#else /* CONFIG_CMD_SCRATCHPAD */
#error "The scratchpad is used, define a backup region for the DFU fields."
#endif /* CONFIG_CMD_SCRATCHPAD */


#ifdef SECTION_IS_RW


int dfu_bootmanager_enter_dfu(void)
{
	dfu_bootmanager_backup_write(DFU_BOOTMANAGER_VALUE_DFU);
	system_reset(0);
	return 0;
}

void dfu_bootmanager_clear_reboot_counter(void)
{
	dfu_bootmanager_backup_write(DFU_BOOTMANAGER_VALUE_CLEAR);
}

#else /* !SECTION_IS_RW */

/*
 * Reads the backup registers and performs validation. The value stored
 * within the VALUE_MASK is returned and the status code indicates
 * if the valid check passed.
 *
 * @param value[out] Value stored within the DFU_BOOTMANAGER_VALUE_MASK
 * @return           EC_SUCCESS, or non-zero if error.
 */
static int dfu_bootmanager_backup_read(uint8_t *value)
{
	uint16_t data = bkpdata_read(BKPDATA_INDEX_SCRATCHPAD);
	uint16_t valid_check = data & DFU_BOOTMANAGER_VALID_MASK;

	*value = (uint8_t)(data & DFU_BOOTMANAGER_VALUE_MASK);
	if (valid_check != DFU_BOOTMANAGER_VALID_CHECK)
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/*
 * Checks if the RW region is valid by reading the first 8 bytes of flash, it
 * should not start with an erased block.
 *
 * The DFU boot manager should not jump into the RW region if it contains
 * invalid code as the EC is be unstable. A check will be performed to validate
 * the start of the RW region to verify that it contains valid data.
 * DFU programmers should erase this section of flash first and at this point,
 * the EC will no longer be able to jump into the RW application.
 *
 * The normal DFU programming sequence programming will work, but by
 * splitting into the following sequence we can protect against additional
 * failures.
 *
 * 1. Erase the first RW flash section. This will lock the EC out of RW.
 * 2. Update the remaining flash. Erase, program, and read back flash to
 *      to verify the operation was successful. Regions of the flash which
 *      are difficult to repair if an error occurs should be programmed next.
 * 3. Program the first RW flash section and exit DFU mode if verification is
 *      successful.
 *
 * @returns True if the start of the RW region is erased.
 */
static bool rw_is_empty(void)
{
	const uint64_t *start = (const uint64_t *)(CONFIG_PROGRAM_MEMORY_BASE +
		CONFIG_RW_MEM_OFF);

	return *start == UINT64_MAX;
}

/*
 * Reads the backup registers. This will trigger a jump to DFU if either
 * the application has requested it or if the reboot counter indicates
 * the device is likely in a bad state. A counter recording the number
 * of reboots will be incremented.
 *
 * @returns True if the backup memory region indicates we should boot into DFU.
 */
static bool backup_boot_checks(void)
{
	uint8_t value;

	if (dfu_bootmanager_backup_read(&value)) {
		/* Value stored is not valid, set it to a valid value. */
		dfu_bootmanager_backup_write(DFU_BOOTMANAGER_VALUE_CLEAR);
		return false;
	}
	if (value == DFU_BOOTMANAGER_VALUE_DFU)
		return true;
#ifdef CONFIG_DFU_BOOTMANAGER_MAX_REBOOT_COUNT
	if (value >= CONFIG_DFU_BOOTMANAGER_MAX_REBOOT_COUNT)
		return true;
	/* Increment the reboot loop counter. */
	value++;
	dfu_bootmanager_backup_write(value);
#endif /* CONFIG_DFU_BOOTMANAGER_MAX_REBOOT_COUNT */
	return false;
}


/*
 * Performs the minimal set of initialization required for the boot manager.
 * The main application region or DFU boot loader have different prerequisites,
 * any configurations that are enabled either need to be benign with both
 * images or disabled prior to the jumps.
 */
static void dfu_bootmanager_init(void)
{
	/* enable clock on Power module */
#ifndef CHIP_FAMILY_STM32H7
#ifdef	CHIP_FAMILY_STM32L4
	STM32_RCC_APB1ENR1 |= STM32_RCC_PWREN;
#else
	STM32_RCC_APB1ENR |= STM32_RCC_PWREN;
#endif
#endif
#if defined(CHIP_FAMILY_STM32F4)
	/* enable backup registers */
	STM32_RCC_AHB1ENR |= STM32_RCC_AHB1ENR_BKPSRAMEN;
#elif defined(CHIP_FAMILY_STM32H7)
	/* enable backup registers */
	STM32_RCC_AHB4ENR |= BIT(28);
#elif defined(CHIP_FAMILY_STM32L4)
	/* enable RTC APB clock */
	STM32_RCC_APB1ENR1 |= STM32_RCC_APB1ENR1_RTCAPBEN;
#else
	/* enable backup registers */
	STM32_RCC_APB1ENR |= BIT(27);
#endif
	/* Delay 1 APB clock cycle after the clock is enabled */
	clock_wait_bus_cycles(BUS_APB, 1);
	/* Enable access to RCC CSR register and RTC backup registers */
	STM32_PWR_CR |= BIT(8);
}

static void jump_to_rw(void)
{
	void (*resetvec)(void);

	resetvec = (void(*)(void))(CONFIG_PROGRAM_MEMORY_BASE +
		CONFIG_RW_MEM_OFF + 4);

	resetvec();
}

static void jump_to_dfu(void)
{
	void (*resetvec)(void);

	resetvec = (void(*)(void))(STM32_DFU_BASE + 4);

	/* Clear the scratchpad. */
	dfu_bootmanager_backup_write(DFU_BOOTMANAGER_VALUE_CLEAR);
	resetvec();
}

/*
 * DFU based bootloader main
 */
int main(void)
{
	dfu_bootmanager_init();

	if (rw_is_empty() || backup_boot_checks())
		jump_to_dfu();
	else
		jump_to_rw();

	return 0;
}

/*
 * Function stubs
 */

void exception_panic(void) {}
void task_clear_pending_irq(int irq) {}
void mutex_lock(mutex_t *mtx) {}
void mutex_unlock(mutex_t *mtx) {}

bool in_interrupt_context(void)
{
	return false;
}

#endif /* SECTION_IS_RW */
