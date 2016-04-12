/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The anti-rollback, persistent flash counter. This counter requires two pages
 * of flash, one HIGH page and one LOW page.
 *
 * The LOW page is implemented in a strike style, with each "strike" zero-ing
 * out 4 bits at a time, meaning each word can be struck a total of 8 times.
 *
 * Once the LOW page is completely struck, the HIGH page is incremented by 2.
 * The even increment is for the value, the odd increment is a guard signal that
 * the LOW page must be erased.  So as an example:
 *
 * If HIGH is 2, the LOW page would increment to 3, erase itself, and then
 * increment to 4.  If this process is interrupted for some reason (power loss
 * or user intervention) and the HIGH left at 3, on next resume, the HI page
 * will recognize something was left pending and erase again.
 *
 */

#include "common.h"
#include "console.h"
#include "flash.h"
#include "flash_config.h"
#include "registers.h"
#include "timer.h"
#include "hooks.h"


#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)
#define INCORRECT_FLASH_CNT 0xdeadd0d0

#define PAGE_WORDS (CONFIG_FLASH_BANK_SIZE/sizeof(uint32_t))

uint32_t FLASH_CNT_HI[PAGE_WORDS] __attribute__((section(".flash_cntr.hi")));
uint32_t FLASH_CNT_LO[PAGE_WORDS] __attribute__((section(".flash_cntr.lo")));


static uint32_t cnt_stop;

static void _write(uint32_t *p, size_t i, uint32_t v)
{
	char data[4];
	int offset = (uintptr_t)p + (i*4) - CONFIG_PROGRAM_MEMORY_BASE;

	/* this write is always 4 bytes */
	data[0] = v & 0xFF;
	data[1] = (v >> 8) & 0xFF;
	data[2] = (v >> 16) & 0xFF;
	data[3] = (v >> 24) & 0xFF;

	/* p[i] = v */
	ccprintf("\nWriting Offset 0x%08x with value 0x%08x\n", offset, v);
	flash_write(offset, 4, data);
}

static void _erase(void *p)
{
	int offset = (uintptr_t)p - CONFIG_PROGRAM_MEMORY_BASE;

	ccprintf("\nErasing Offset 0x%x", offset);
	flash_erase(offset, CONFIG_FLASH_BANK_SIZE);
}

static uint32_t _decode(size_t i,  uint32_t *p)
{
	uint32_t v = p[i];

	/* Return value for clean states */
	switch (v) {
	case 0xffffffff: return 0;
	case 0x3cffffff: return 1;
	case 0x00ffffff: return 2;
	case 0x003cffff: return 3;
	case 0x0000ffff: return 4;
	case 0x00003cff: return 5;
	case 0x000000ff: return 6;
	case 0x0000003c: return 7;
	case 0x00000000: return 8;
	}

	/*
	 * Not a clean state; figure which transition got interrupted,
	 * and affirm that transition target and return its target value.
	 */
	if ((v & 0x3cffffff) == 0x3cffffff) {
		_write(p, i, 0x3cffffff);  /* affirm */
		return 1;
	}
	if ((v & 0xc3ffffff) == 0x00ffffff) {
		_write(p, i, 0x00ffffff);  /* affirm */
		return 2;
	}
	if ((v & 0xff3cffff) == 0x003cffff) {
		_write(p, i, 0x003cffff);  /* affirm */
		return 3;
	}
	if ((v & 0xffc3ffff) == 0x0000ffff) {
		_write(p, i, 0x0000ffff);  /* affirm */
		return 4;
	}
	if ((v & 0xffff3cff) == 0x00003cff) {
		_write(p, i, 0x003cff);  /* affirm */
		return 5;
	}
	if ((v & 0xffffc3ff) == 0x000000ff) {
		_write(p, i, 0x000000ff);  /* affirm */
		return 6;
	}
	if ((v & 0xffffff3c) == 0x0000003c) {
		_write(p, i, 0x0000003c);  /* affirm */
		return 7;
	}
	if ((v & 0xffffffc3) == 0x00000000) {
		_write(p, i, 0x0000000000);  /* affirm */
		return 8;
	}

	return INCORRECT_FLASH_CNT;  /* unknown state */
}

static uint32_t _encode(size_t v)
{
	if (v > 7)
		return 0;

	switch (v & 7) {
	case 0: return 0xffffffff;
	case 1: return 0x3cffffff;
	case 2: return 0x00ffffff;
	case 3: return 0x003cffff;
	case 4: return 0x0000ffff;
	case 5: return 0x00003cff;
	case 6: return 0x000000ff;
	case 7: return 0x0000003c;
	}
	return 0;
}

static void _inc(size_t i, uint32_t *p)
{
	uint32_t v = _decode(i, p);

	if (v == 8) {
		/*
		 * re-affirm (w/ single pulse?)
		 * in case previous strike got interrupted and is flaky but we
		 * read it as 0 this run. Making sure next run will see it as 0
		 * for sure. Note this is extra hit past the 8 / word.. should
		 * be ok, even 8 writes per word is far below the word line
		 * requirement, an extra should be negligible
		 */
		_write(p, i, 0);
		_write(p, i + 1, _encode(1));
	} else {
		/* This also re-affirms other 0 bits in this word. */
		_write(p, i,  _encode(v + 1));
	}
}

static uint32_t flash_cntr_incr(void)
{
	uint32_t cnt = 0;
	uint32_t hi, lo;
	uint32_t result;

	/*
	 * First determine the current count
	 * Do so by first iterating through the HIGH/LOW pages
	 */
	for (hi = 0; hi < ARRAY_SIZE(FLASH_CNT_HI); ++hi) {
		result = _decode(hi, FLASH_CNT_HI);

		/*
		 * if the WORD does not decode correctly, write the entire WORD
		 * to 0 and move on
		 */
		if (result == INCORRECT_FLASH_CNT) {
			/* jump the entire word ahead */
			_write(FLASH_CNT_HI, hi, 0);
			/* count adds 4 because each HIGH word counts 4 times */
			return (cnt + 4) * (8 * ARRAY_SIZE(FLASH_CNT_LO) + 1);
		}

		/*
		 * if the decoded result is ODD, that means an erase operation
		 * was interrupt and we need to finish it off again
		 */
		if (result & 1) {
			_erase(FLASH_CNT_LO);
			_write(FLASH_CNT_HI, hi, _encode(result + 1));
			return (cnt + (result + 1) / 2) *
				(8 * ARRAY_SIZE(FLASH_CNT_LO) + 1);
		}
		cnt += result / 2;

		/*
		 * if result equals 8, that means the current HIGH word is
		 * entirely 0, so we have not yet reached the end, continue
		 * counting, otherwise, breakout of the for loop
		 */
		if (result != 8)
			break;
	}

	/* each count is worth the entire strike of the LOW array */
	cnt *= (8 * ARRAY_SIZE(FLASH_CNT_LO) + 1);

	for (lo = 0; lo < ARRAY_SIZE(FLASH_CNT_LO); ++lo) {
		result = _decode(lo, FLASH_CNT_LO);
		if (result == INCORRECT_FLASH_CNT) {
			/* Try fix-up broken LO write; assume worst */
			_write(FLASH_CNT_LO, lo, 0);  /* jump ahead */

			/* each LOW word counts 8 times (instead of 4 like HIGH)
			 */
			return cnt + 8;  /* done */
		}
		cnt += result;
		if (result != 8)
			break;
	}

	if (hi == ARRAY_SIZE(FLASH_CNT_HI) && lo == ARRAY_SIZE(FLASH_CNT_LO)) {
		/* We are exhausted, can count no more */
		return -1;
	}

	/* After current count is determined, increment as required */
	if (lo == ARRAY_SIZE(FLASH_CNT_LO)) {
		ccprintf("\nFull page of LOW count has been used, %d\n", lo);
		/* All LOW page is striken, time to advance HIGH page */
		_write(FLASH_CNT_LO, ARRAY_SIZE(FLASH_CNT_LO) - 1, 0);

		/* mark erase busy, odd increment */
		_inc(hi, FLASH_CNT_HI);

		_erase(FLASH_CNT_LO);

		/* mark erase done, even increment */
		_inc(hi, FLASH_CNT_HI);
	} else {
		_inc(lo, FLASH_CNT_LO);
	}

	/* return the final count */
	return cnt + 1;
}

static void flash_cnt_auto_inc(void)
{
	uint32_t count;

	count = 0;
	if (!cnt_stop) {
		count = flash_cntr_incr();
		ccprintf("Count is %d\n", count);
		if (count > 5000000) {
			ccprintf("Max count is %d\n", count);
			cnt_stop = 1;
		}
	}
}
DECLARE_HOOK(HOOK_TICK, flash_cnt_auto_inc, HOOK_PRIO_DEFAULT);
