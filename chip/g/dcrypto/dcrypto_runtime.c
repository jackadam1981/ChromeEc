/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "internal.h"

#include "task.h"
#include "registers.h"

#define DMEM_NUM_WORDS 1024
#define IMEM_NUM_WORDS 1024

static struct mutex dcrypto_mutex;
static volatile task_id_t my_task_id;
static int dcrypto_is_initialized;

void dcrypto_init_and_lock(void)
{
	int i;
	volatile uint32_t *ptr;

	mutex_lock(&dcrypto_mutex);
	my_task_id = task_get_current();

	if (dcrypto_is_initialized)
		return;

	/* Enable PMU. */
	REG_WRITE_MLV(GR_PMU_PERICLKSET0, GC_PMU_PERICLKSET0_DCRYPTO0_CLK_MASK,
		GC_PMU_PERICLKSET0_DCRYPTO0_CLK_LSB, 1);

	/* Reset. */
	REG_WRITE_MLV(GR_PMU_RST0, GC_PMU_RST0_DCRYPTO0_MASK,
		GC_PMU_RST0_DCRYPTO0_LSB, 0);

	/* Turn off random nops (which are enabled by default). */
	GWRITE_FIELD(CRYPTO, RAND_STALL_CTL, STALL_EN, 0);
	/* Configure random nop percentage at 6%. */
	GWRITE_FIELD(CRYPTO, RAND_STALL_CTL, FREQ, 3);
	/* Now turn on random nops. */
	GWRITE_FIELD(CRYPTO, RAND_STALL_CTL, STALL_EN, 1);

	/* Initialize DMEM. */
	ptr = GREG32_ADDR(CRYPTO, DMEM_DUMMY);
	for (i = 0; i < DMEM_NUM_WORDS; ++i)
		*ptr++ = 0xdddddddd;

	/* Initialize IMEM. */
	ptr = GREG32_ADDR(CRYPTO, IMEM_DUMMY);
	for (i = 0; i < IMEM_NUM_WORDS; ++i)
		*ptr++ = 0xdddddddd;

	GREG32(CRYPTO, INT_STATE) = -1;   /* Reset all the status bits. */
	GREG32(CRYPTO, INT_ENABLE) = -1;  /* Enable all status bits. */

	task_enable_irq(GC_IRQNUM_CRYPTO0_HOST_CMD_DONE_INT);

	/* Reset. */
	GREG32(CRYPTO, CONTROL) = 1;
	GREG32(CRYPTO, CONTROL) = 0;

	dcrypto_is_initialized = 1;
}

void dcrypto_unlock(void)
{
	mutex_unlock(&dcrypto_mutex);
}

#ifndef DCRYPTO_CALL_TIMEOUT_US
#define DCRYPTO_CALL_TIMEOUT_US  (700 * 1000)
#endif
/*
 * When running on Cr50 this event belongs in the TPM task event space. Make
 * sure there is no collision with events defined in ./common/tpm_regsters.c.
 */
#define TASK_EVENT_DCRYPTO_DONE  TASK_EVENT_CUSTOM_BIT(0)

uint32_t dcrypto_call(uint32_t adr)
{
	uint32_t event;

	do {
		/* Reset all the status bits. */
		GREG32(CRYPTO, INT_STATE) = -1;
	} while (GREG32(CRYPTO, INT_STATE) & 3);

	GREG32(CRYPTO, HOST_CMD) = 0x08000000 + adr; /* Call imem:adr. */

	event = task_wait_event_mask(TASK_EVENT_DCRYPTO_DONE,
				     DCRYPTO_CALL_TIMEOUT_US);
	/* TODO(ngm): switch return value to an enum. */
	switch (event) {
	case TASK_EVENT_DCRYPTO_DONE:
		return 0;
	default:
		return 1;
	}
}

void __keep dcrypto_done_interrupt(void)
{
	GREG32(CRYPTO, INT_STATE) = GC_CRYPTO_INT_STATE_HOST_CMD_DONE_MASK;
	task_clear_pending_irq(GC_IRQNUM_CRYPTO0_HOST_CMD_DONE_INT);
	task_set_event(my_task_id, TASK_EVENT_DCRYPTO_DONE, 0);
}
DECLARE_IRQ(GC_IRQNUM_CRYPTO0_HOST_CMD_DONE_INT, dcrypto_done_interrupt, 1);

void dcrypto_imem_load(size_t offset, const uint32_t *opcodes,
			size_t n_opcodes)
{
	size_t i;
	volatile uint32_t *ptr = GREG32_ADDR(CRYPTO, IMEM_DUMMY);

	ptr += offset;
	/* Check first word and copy all only if different. */
	if (ptr[0] != opcodes[0]) {
		for (i = 0; i < n_opcodes; ++i)
			ptr[i] = opcodes[i];
	}
}

uint32_t dcrypto_dmem_load(size_t offset, const void *words, size_t n_words)
{
	size_t i;
	volatile uint32_t *ptr = GREG32_ADDR(CRYPTO, DMEM_DUMMY);
	const uint32_t *src = (const uint32_t *) words;
	struct access_helper *word_accessor = (struct access_helper *) src;
	uint32_t diff = 0;

	ptr += offset * 8;  /* Offset is in 256 bit addresses. */
	for (i = 0; i < n_words; ++i) {
		/*
		 * The implementation of memcpy makes unaligned writes if src
		 * is unaligned. DMEM on the other hand requires writes to be
		 * aligned, so do a word-by-word copy manually here.
		 */
		uint32_t v = word_accessor[i].udata;

		diff |= (ptr[i] ^ v);
		ptr[i] = v;
	}
	return diff;
}

#ifdef CRYPTO_TEST_ECDSA

#include "dcrypto.h"
#include "trng.h"
#include "console.h"
#include "shared_mem.h"
#include "system.h"
#include "watchdog.h"

#define ECDSA_TEST_ITERATIONS 1000

#define ECDSA_TEST_SLEEP_DELAY_IN_US 1000000

static const p256_int r_golden = {
	.a = {0x7f03f667, 0xdf31a511, 0x714e7982, 0x8d6b6c80, 0xe27181f3,
	      0x3f9af787, 0x6109760d, 0xacddd5db},
};
static const p256_int s_golden = {
	.a = {0x7de56f48, 0x71fb55e9, 0x2a74aa46, 0xc1a046ff, 0xe24e0f1d,
	      0x938458b9, 0xbcb0802f, 0x960c4295},
};

static int call_on_bigger_stack(uint32_t stack,
				int (*func)(p256_int *, p256_int *),
				p256_int *r, p256_int *s)
{
	int result = 0;

	/* Move to new stack and call the function */
	__asm__ volatile("mov r4, sp\n"
			 "mov sp, %[new_stack]\n"
			 "mov r0, %[r]\n"
			 "mov r1, %[s]\n"
			 "blx %[func]\n"
			 "mov sp, r4\n"
			 "mov %[result], r0\n"
			 : [result] "=r"(result) /* output */
			 : [new_stack] "r"(stack), [r] "r"(r), [s] "r"(s),
			   [func] "r"(func) /* input */
			 : "r0", "r1", "r2", "r3", "r4",
			   "lr" /* clobbered registers */
	);

	return result;
}

static int ecdsa_sign_go(p256_int *r, p256_int *s)
{
	struct drbg_ctx drbg;
	p256_int d;
	int ret = 0;
	p256_int message = *s;

	/* drbg init with same entropy */
	hmac_drbg_init(&drbg, r->a, sizeof(r->a), NULL, 0, NULL, 0);

	/* pick a key */
	dcrypto_p256_pick(&drbg, &d);

	/* drbg_reseed with entropy and message */
	hmac_drbg_reseed(&drbg, r->a, sizeof(r->a), s->a, sizeof(s->a), NULL,
			 0);

	ret = dcrypto_p256_ecdsa_sign(&drbg, &d, &message, r, s);
	drbg_exit(&drbg);

	return ret;
}

static int ecdsa_verisign_go(p256_int *r, p256_int *s)
{
	struct drbg_ctx drbg;
	p256_int entropy = *r;
	p256_int message = *s;
	int ret = 0;

	/* drbg init with same entropy */
	hmac_drbg_init(&drbg, r->a, sizeof(r->a), NULL, 0, NULL, 0);

	ret = dcrypto_p256_ecdsa_verisign(&drbg, &entropy, &message, r, s, NULL,
					  NULL);
	drbg_exit(&drbg);

	return ret;
}

static int cmd_crypto_test_ecdsa(int argc, char *argv[])
{
	p256_int entropy, message, r_sign, s_sign, r_verisign, s_verisign;
	LITE_SHA256_CTX hsh;
	int result = 0;
	char *new_stack;
	const uint32_t new_stack_size = 2 * 1024;

	/* start with some known value for a message */
	const uint8_t ten = 0x0A;

	for (uint8_t i = 0; i < 8; i++)
		entropy.a[i] = i;

	DCRYPTO_SHA256_init(&hsh, 0);
	HASH_update(&hsh, &ten, sizeof(ten));
	p256_from_bin(HASH_final(&hsh), &message);

	r_sign = entropy;
	s_sign = message;
	r_verisign = entropy;
	s_verisign = message;

	result = shared_mem_acquire(new_stack_size, &new_stack);

	if (result != EC_SUCCESS)
		return result;

	memset(new_stack, 0, new_stack_size);

	for (uint32_t i = 0; i < ECDSA_TEST_ITERATIONS; i++) {
		result = call_on_bigger_stack((uint32_t)new_stack +
						      new_stack_size,
					      ecdsa_sign_go, &r_sign, &s_sign);

		if (!result) {
			ccprintf("ECDSA SIGN TEST fail: %d\n", result);
			return EC_ERROR_INVAL;
		}

		result = call_on_bigger_stack(
			(uint32_t)new_stack + new_stack_size, ecdsa_verisign_go,
			&r_verisign, &s_verisign);

		if (result != EC_SUCCESS) {
			ccprintf("ECDSA VERISIGN TEST fail: %d\n", result);
			return EC_ERROR_INVAL;
		}

		watchdog_reload();
		delay_sleep_by(ECDSA_TEST_SLEEP_DELAY_IN_US);
	}

	shared_mem_release(new_stack);

	/* compare to the golden r and s values */
	for (uint8_t i = 0; i < 8; i++) {
		if (r_sign.a[i] != r_golden.a[i] ||
		    r_verisign.a[i] != r_golden.a[i]) {
			ccprintf("ECDSA TEST r does not match at %d: "
				 "r_sign=%08x, r_verisign=%08x, r_golden= "
				 "%08x\n",
				 i, r_sign.a[i], r_verisign.a[i],
				 r_golden.a[i]);
			return EC_ERROR_INVAL;
		}
		if (s_sign.a[i] != s_golden.a[i] ||
		    s_verisign.a[i] != s_golden.a[i]) {
			ccprintf("ECDSA TEST s does not match at %d: "
				 "s_sign=%08x, s_verisign=%08x, s_golden= "
				 "%08x\n",
				 i, s_sign.a[i], s_verisign.a[i],
				 s_golden.a[i]);
			return EC_ERROR_INVAL;
		}
	}

	ccprintf("ECDSA TEST success!!!\n");

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(crt_ecdsa, cmd_crypto_test_ecdsa, "",
			     "crypto ecdsa test");

#endif
