/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "common.h"
#include "test_util.h"
#include "cryptoc/util.h"

/**
 * This function successfully makes gcc optimize out the last memset
 * line, when using the normal gcc with builtins.
 */
void exercise_memset(char **p) {
  /* Without volatile, space will be optimized out. */
  volatile char space[1024] = { 0 };

  char buf[] = "Hello World!";
  *p = (char *)buf; // might need to avoid dangling-pointer warning

  /* Use space to avoid unused-variable warning. */
  memset((void *)space, 's', sizeof(space));

  /* Use buf */
  ccprintf("buf is \"%s\"\n", buf);

  /* We except the following memset to be omitted during optimization. */
  memset(buf, 0, sizeof(buf));
}

void exercise_always_memset(char **p) {
  /* Without volatile, space will be optimized out. */
  volatile char space[1024] = { 0 };

  char buf[] = "Hello World!";
  *p = (char *)buf; // might need to avoid dangling-pointer warning

  /* Use space to avoid unused-variable warning. */
  memset((void *)space, 's', sizeof(space));

  /* Use buf */
  ccprintf("buf is \"%s\"\n", buf);

  /* We except the following memset to not be omitted during optimization. */
  always_memset(buf, 0, sizeof(buf));
}

static int test_optimization_enabled(void)
{
	char *p;
	exercise_memset(&p);

	TEST_ASSERT_ARRAY_EQ(p, "Hello World!", sizeof("Hello World!"));

	return EC_SUCCESS;
}


static int test_always_memset(void)
{
	char *p;
	exercise_always_memset(&p);

  TEST_ASSERT_MEMSET(p, '\0', sizeof("Hello World!"));

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(test_optimization_enabled);
	RUN_TEST(test_always_memset);

	test_print_result();
}
