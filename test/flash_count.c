/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test persistent flash counter.
 */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "console.h"
#include "flash.h"
#include "hooks.h"
#include "host_command.h"
#include "test_util.h"
#include "util.h"

#define PAGE_WORDS (CONFIG_FLASH_BANK_SIZE/sizeof(uint32_t))

/* Each word can count 8 */
#define LO_CNT_PER_WORD 8
#define LO_MAX (PAGE_WORDS*LO_CNT_PER_WORD)


jmp_buf failhandler;
uint32_t FLASH_CNT_HI[PAGE_WORDS];
uint32_t FLASH_CNT_LO[PAGE_WORDS];
uint32_t failafter;

/*****************************************************************************/
/* Replace actual functions */

void _write(uint32_t *p, size_t i, uint32_t v)
{
	uint32_t delta;
	uint32_t rnd;
	uint32_t final;

	if (failafter) {
		if (!--failafter) {
			/* write random subset of the bits down */
			delta = p[i] ^ v;
			rnd = rand() & delta;
			final = v | rnd;
			printf("\nWrite Interrupt\n");
			printf("Current:  0x%08x\n", p[i]);
			printf("Incoming: 0x%08x\n", v);
			printf("Wrote:    0x%08x\n", final);
			p[i] = final;
			longjmp(failhandler, 1);

		}
	}
	/* aka flash_write(page, index, value) */
	p[i] = v;
}

void _erase(void *p)
{
	uint32_t i;

	if (failafter) {
		if (!--failafter) {
			/* write random garbage to entire page */
			uint8_t *b = (uint8_t *)p;

			for (i = 0; i < PAGE_WORDS; ++i)
				b[i] = rand();

			printf("\nErase Interrupt\n");
			longjmp(failhandler, 2);
		}
	}
	/* aka flash_erase(page).. */
	memset(p, 255, CONFIG_FLASH_BANK_SIZE);
}

/*****************************************************************************/
/* Tests */

static uint32_t _decode(uint32_t v)
{

	/* decode hex and decimal values */
	switch (v) {
	case 0: return 0xffffffff;
	case 1: return 0x3cffffff;
	case 2: return 0x00ffffff;
	case 3: return 0x003cffff;
	case 4: return 0x0000ffff;
	case 5: return 0x00003cff;
	case 6: return 0x000000ff;
	case 7: return 0x0000003c;
	}
	return 0xDEADBEEF;
}

/* Using masked compare, because as long as things can be correct in the future
 * it is still correct
 */
static uint32_t _masked_comp(uint32_t exp, uint32_t act)
{
	return (act & exp) == exp;
}

/* Given the iteration number, check if the content matches expected */
static uint32_t _equal(uint32_t iter)
{
	uint32_t exp_cur_wd;
	uint32_t act_cur_wd;
	uint32_t exp_nxt_wd;
	uint32_t act_nxt_wd;
	uint32_t cur_wd_ind;
	uint32_t return_val;
	uint32_t v;

	/* The remaining low count value */
	v = iter % LO_CNT_PER_WORD;

	/* The 3 here should be changed to log of writes per word */
	cur_wd_ind = (iter % LO_MAX) >> 3;

	printf("Current iter:     %d\n", iter);
	printf("Current word ind: %d\n", cur_wd_ind);

	/* Different handling when on multiple of LO_MAX */
	if ((v == 0) & (iter > 0) & !(iter % LO_MAX)) {
		cur_wd_ind = PAGE_WORDS - 1;
		exp_cur_wd = 0;
		act_cur_wd = FLASH_CNT_LO[cur_wd_ind];
		return_val = _masked_comp(exp_cur_wd, act_cur_wd);
	}
	/* Differnt handling depending on multiple of LO_CNT_PER_WORD */
	else if ((v == 0) & (iter > 0)) {
		/* We are a multiple of 8, an increment here needs to check a
		 * a couple of things
		 */
		exp_cur_wd = 0;
		act_cur_wd = FLASH_CNT_LO[cur_wd_ind-1];
		exp_nxt_wd = _decode(0);
		act_nxt_wd = FLASH_CNT_LO[cur_wd_ind];
		return_val = _masked_comp(exp_cur_wd, act_cur_wd) &&
			_masked_comp(exp_nxt_wd, act_nxt_wd);
		printf("Expected next: 0x%08x\n", exp_nxt_wd);
		printf("Actual next:   0x%08x\n", act_nxt_wd);
	} else {
		exp_cur_wd = _decode(v);
		act_cur_wd = FLASH_CNT_LO[cur_wd_ind];
		return_val = _masked_comp(exp_cur_wd, act_cur_wd);
	}
	printf("Expected current: 0x%08x\n", exp_cur_wd);
	printf("Actual current:   0x%08x\n\n", act_cur_wd);

	/* return 1 if contents are equal */
	return return_val;
}

static int test_flash_cntr(void)
{
	uint32_t limit;
	uint32_t i, j;
	uint32_t fail;
	uint32_t results;
	uint32_t forward_steps;

	ccprintf("Start Testing\n");

	/* Maximum value the counter is capable of counting
	 * limit = ((sizeof(FLASH_CNT_HI) + 1) * (2 * sizeof(FLASH_CNT_LO) + 1))
	 * -1
	 */

	/* For now, just test the lower interruptions, will add rest later */
	failafter = 0;
	limit = LO_MAX;

	/* init counters to all 1's */
	memset(FLASH_CNT_HI, 255, sizeof(FLASH_CNT_HI));
	memset(FLASH_CNT_LO, 255, sizeof(FLASH_CNT_LO));

	/* Keep on counting, whenever counting fails for any reason, skip the
	 * assert checking, indicate the interrupt point, and continue counting
	 */
	for (i = 0; i < limit; i++) {

		/* If interrupted, start from here again */
		fail = setjmp(failhandler);

		/* Tracking counter re-evaluated on every interruption and
		 * every successful iteration
		 */
		forward_steps = 0;

		/* if the last run was interrupted, the number of steps to fast
		 * forward depends on how many times things are incorrect
		 */
		if (fail) {
			printf("\nFailure Handling\n");
			if (!_equal(i)) {
				for (j = 1; j < (LO_MAX - 1); j++) {
					if (_equal(i + j))
						break;
				}
				forward_steps = j;
			}
			printf("Forward %d!\n", forward_steps);

		} else
			printf("\nIteration %d\n", i);


		if (!failafter)
			/* just fail after some random number of iterations */
			failafter = rand() % 5;

		results = flash_cntr_incr();
		printf("Increment result is %d\n", results);
		printf("Expected result is %d\n", i + forward_steps + 1);

		if (i + forward_steps + 1 != results) {
			printf("\n\nTest failed!!!\n");
			for (i = 0; i < PAGE_WORDS; i++) {
				printf("Contents[%03d]: 0x%08x\n", i,
				       FLASH_CNT_LO[i]);
			}
		}

		TEST_ASSERT(results == (i + forward_steps + 1));
		i += forward_steps;
	}

	return EC_SUCCESS;
}


void run_test(void)
{
	RUN_TEST(test_flash_cntr);

	test_print_result();
}
