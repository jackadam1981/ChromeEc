/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Flash memory module for STM32L4 family */

#include "common.h"
#include "console.h"
#include "clock.h"
#include "flash.h"
#include "hooks.h"
#include "registers.h"
#include "panic.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

/*
 * Approximate number of CPU cycles per iteration of the loop when polling
 * the flash status
 */
#define CYCLE_PER_FLASH_LOOP 10

/* Flash page programming timeout.  This is 2x the datasheet max. */
#define FLASH_TIMEOUT_US 48000

static inline int calculate_flash_timeout(void)
{
	return (FLASH_TIMEOUT_US *
		(clock_get_freq() / SECOND) / CYCLE_PER_FLASH_LOOP);
}

static int unlock(int locks)
{
	/*
	 * We may have already locked the flash module and get a bus fault
	 * in the attempt to unlock. Need to disable bus fault handler now.
	 */
	ignore_bus_fault(1);

	/* unlock CR if needed */
	if (STM32_FLASH_CR & FLASH_CR_LOCK) {
		STM32_FLASH_KEYR = FLASH_KEYR_KEY1;
		STM32_FLASH_KEYR = FLASH_KEYR_KEY2;
	}
	/* unlock option memory if required */
	if ((locks & FLASH_CR_OPTLOCK) &&
	    !(STM32_FLASH_CR & FLASH_CR_OPTLOCK)) {
		STM32_FLASH_OPTKEYR = FLASH_OPTKEYR_KEY1;
		STM32_FLASH_OPTKEYR = FLASH_OPTKEYR_KEY2;
	}

	/* Re-enable bus fault handler */
	ignore_bus_fault(0);

	return ((STM32_FLASH_CR ^ FLASH_CR_OPTLOCK) &
		(locks | FLASH_CR_OPTLOCK)) ? EC_ERROR_UNKNOWN : EC_SUCCESS;
}

static void lock(void)
{
	STM32_FLASH_CR = FLASH_CR_LOCK;
}

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_write(int offset, int size, const char *data)
{
	uint32_t *address = (void *)(CONFIG_PROGRAM_MEMORY_BASE + offset);
	int res = EC_SUCCESS;
	int timeout = calculate_flash_timeout();
	int i;
	int unaligned = (uint32_t)data & (CONFIG_FLASH_WRITE_SIZE - 1);
	uint32_t *data32 = (void *)data;

	if (unlock(FLASH_CR_LOCK) != EC_SUCCESS) {
		res = EC_ERROR_UNKNOWN;
		goto exit_wr;
	}

	/* Clear previous error status */
	STM32_FLASH_SR = FLASH_SR_ERR_MASK;

	/* set PG bit */
	STM32_FLASH_CR |= FLASH_CR_PG;

	for (; size > 0; size -= CONFIG_FLASH_WRITE_SIZE) {
		/*
		 * Reload the watchdog timer to avoid watchdog reset when doing
		 * long writing.
		 */
		watchdog_reload();

		/* wait to be ready  */
		for (i = 0; (STM32_FLASH_SR & FLASH_SR_BUSY) && (i < timeout);
		     i++)
			;

		/* write the 2 words */
		if (unaligned) {
			*address++ = (uint32_t)data[0] | (data[1] << 8)
				   | (data[2] << 16) | (data[3] << 24);
			*address++ = (uint32_t)data[4] | (data[5] << 8)
				   | (data[6] << 16) | (data[7] << 24);
			data += CONFIG_FLASH_WRITE_SIZE;
		} else {
			*address++ = *data32++;
			*address++ = *data32++;
		}

		/* Wait for writes to complete */
		for (i = 0; (STM32_FLASH_SR & FLASH_SR_BUSY) && (i < timeout);
		     i++)
			;

		if (STM32_FLASH_SR & FLASH_SR_BUSY) {
			res = EC_ERROR_TIMEOUT;
			goto exit_wr;
		}

		/*
		 * Check for error conditions - erase failed, voltage error,
		 * protection error.
		 */
		if (STM32_FLASH_SR & FLASH_SR_ERR_MASK) {
			res = EC_ERROR_UNKNOWN;
			goto exit_wr;
		}
	}

exit_wr:
	/* Disable PG bit */
	STM32_FLASH_CR &= ~FLASH_CR_PG;

	lock();

	return res;
}

int flash_physical_erase(int offset, int size)
{
	int res = EC_SUCCESS;
	int pg;
	int last = (offset + size) / CONFIG_FLASH_ERASE_SIZE;

	if (unlock(FLASH_CR_LOCK) != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	/* Clear previous error status */
	STM32_FLASH_SR = FLASH_SR_ERR_MASK;

	last = (offset + size) / CONFIG_FLASH_ERASE_SIZE;
	for (pg = offset / CONFIG_FLASH_ERASE_SIZE; pg < last; pg++) {
		timestamp_t deadline;

		/* Do nothing if already erased */
		if (flash_is_erased(offset, CONFIG_FLASH_ERASE_SIZE))
			continue;

		/* select page to erase and PER bit */
		STM32_FLASH_CR = (STM32_FLASH_CR & ~FLASH_CR_PNB_MASK)
				| FLASH_CR_PER | FLASH_CR_PNB(pg);

		/* set STRT bit : start erase */
		STM32_FLASH_CR |= FLASH_CR_STRT;

		/*
		 * Reload the watchdog timer to avoid watchdog reset during a
		 * long erase operation.
		 */
		watchdog_reload();

		deadline.val = get_time().val + FLASH_TIMEOUT_US;
		/* Wait for erase to complete */
		while ((STM32_FLASH_SR & FLASH_SR_BUSY) &&
		       (get_time().val < deadline.val)) {
			usleep(300);
		}
		if (STM32_FLASH_SR & FLASH_SR_BUSY) {
			res = EC_ERROR_TIMEOUT;
			goto exit_er;
		}

		/*
		 * Check for error conditions - erase failed, voltage error,
		 * protection error
		 */
		if (STM32_FLASH_SR & FLASH_SR_ERR_MASK) {
			res = EC_ERROR_UNKNOWN;
			goto exit_er;
		}
	}

exit_er:
	/* reset PER bit */
	STM32_FLASH_CR &= ~(FLASH_CR_PER | FLASH_CR_PNB_MASK);

	lock();

	return res;
}

int flash_physical_protect_at_boot(uint32_t new_flags)
{
	return EC_SUCCESS;
}

/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
	return EC_SUCCESS;
}

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_get_protect(int block)
{
	return 0;
}

uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;

	return flags;
}

int flash_physical_protect_now(int all)
{
	return EC_ERROR_INVAL;
}

uint32_t flash_physical_get_valid_flags(void)
{
	return 0;
}

uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	uint32_t ret = 0;

	return ret;
}
