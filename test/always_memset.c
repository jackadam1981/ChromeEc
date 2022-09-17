/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * We enable optimization level 3 in the test/build.mk.
 * Running these tests without optimization is pointless, since the primary
 * purpose of the always_memset function is to evade compiler optimizations.
 * If optimization is disabled, the test_optimization_working test will fail.
 */

#include <string.h>

#include "common.h"
#include "test_util.h"
#include "cryptoc/util.h"

#define EXTRA_STACK_SIZE 1024
#define UNIQUE_STRING "Hello World!"

/**
 * @brief Check basic memset behavior of always_memset.
 */
static int test_basic_functionality(void)
{
	char buf[256];

	for (size_t i = 0; i < sizeof(buf); i++) {
		buf[i] = (char)i;
	}

	always_memset(buf, 1, sizeof(buf));

	TEST_ASSERT_MEMSET(buf, 1, sizeof(UNIQUE_STRING));

	return EC_SUCCESS;
}

/**
 * @brief Builtin memset stand-in.
 *
 * The compiler doesn't see our EC memset as a function that can be optimized
 * out "with no side effect", so we present one here.
 */
static inline __attribute__((always_inline)) void
fake_builtin_memset(char *dest, char c, size_t len)
{
	for (size_t i = 0; i < len; i++)
		dest[i] = c;
}

/**
 * This function creates a contrived scenario where the compiler would choose
 * to optimize out the last memset due to no side effect.
 * This methodology/setup has been manually tested in a normal GNU/Linux
 * environment with the real builtin memset.
 */
static void exercise_memset(char **p)
{
	/*
	 * Add extra stack space so that |buf| doesn't get trampled while the
	 * caller is processing the |p| we set (ex. printing and testing p).
	 *
	 * Without volatile, space will be optimized out.
	 */
	volatile char space[EXTRA_STACK_SIZE] = { 0 };

	char buf[] = UNIQUE_STRING;
	*p = buf;

	/* Use |space| to avoid unused-variable warning. */
	memset((void *)space, 's', sizeof(space));

	/*
	 * Force access to |buf| to ensure that it is allocated and seen as
	 * used. Without casting to volatile, the access would be optimized out.
	 * We don't want to make |buf| itself volatile, since
	 * we want the compiler to optimize out the final memset.
	 */
	for (int i = 0; i < sizeof(buf); i++)
		(void)((volatile char *)buf)[i];

	/* We expect the following memset to be omitted during optimization. */
	fake_builtin_memset(buf, 0, sizeof(buf));
}

/**
 * Ensure that optimization is removing a trailing memset that it deems to have
 * no side-effect.
 */
static int test_optimization_working(void)
{
	char *p;

	exercise_memset(&p);

	/*
	 * We expect that the compiler would have optimized out the final
	 * memset, thus we should still be able to see the UNIQUE_STRING in
	 * memory.
	 */
	TEST_ASSERT_ARRAY_EQ(p, UNIQUE_STRING, sizeof(UNIQUE_STRING));

	return EC_SUCCESS;
}

/**
 * This function creates a contrived scenario where the compiler would choose
 * to optimize out the last memset due to no side effect.
 * This methodology/setup has been manually tested in a normal GNU/Linux
 * environment with the real builtin memset.
 */
static void exercise_always_memset(char **p)
{
	/*
	 * Add extra stack space so that |buf| doesn't get trampled while the
	 * caller is processing the |p| we set (ex. printing and testing p).
	 *
	 * Without volatile, space will be optimized out.
	 */
	volatile char space[EXTRA_STACK_SIZE] = { 0 };

	char buf[] = UNIQUE_STRING;
	*p = buf;

	/* Use |space| to avoid unused-variable warning. */
	memset((void *)space, 's', sizeof(space));

	/*
	 * Force access to |buf| to ensure that it is allocated and seen as
	 * used. Without casting to volatile, the access would be optimized out.
	 * We don't want to make |buf| itself volatile, since
	 * we want the compiler to optimize out the final memset.
	 */
	for (int i = 0; i < sizeof(buf); i++)
		(void)((volatile char *)buf)[i];

	/* We expect the following memset to NOT be omitted during optimization.
	 */
	always_memset(buf, 0, sizeof(buf));
}

/**
 * Ensure that always_memset works when used in a scenario where a normal
 * memset would be removed.
 */
static int test_always_memset(void)
{
	char *p;

	exercise_always_memset(&p);

	TEST_ASSERT_MEMSET(p, 0, sizeof(UNIQUE_STRING));

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(test_basic_functionality);
	RUN_TEST(test_optimization_working);
	RUN_TEST(test_always_memset);

	test_print_result();
}
