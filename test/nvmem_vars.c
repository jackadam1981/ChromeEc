/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ctype.h>
#include <string.h>

#include "common.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "printf.h"
#include "shared_mem.h"
#include "test_util.h"

#define TEST_BUF_SIZE 300

uint32_t nvmem_user_sizes[NVMEM_NUM_USERS] = { TEST_BUF_SIZE };

/****************************************************************************/
/* Mock the flash storage */

static uint8_t flash_buffer[TEST_BUF_SIZE];

int nvmem_read(uint32_t startOffset, uint32_t size,
	       void *data, enum nvmem_users user)
{
	/* Our mocks make some assumptions */
	if (startOffset != 0 ||
	    size > TEST_BUF_SIZE ||
	    user != NVMEM_USER_0)
		return EC_ERROR_UNIMPLEMENTED;

	if (!data)
		return EC_ERROR_INVAL;

	memcpy(data, flash_buffer, size);

	return EC_SUCCESS;
}

int nvmem_write(uint32_t startOffset, uint32_t size,
		void *data, enum nvmem_users user)
{
	/* Our mocks make some assumptions */
	if (startOffset != 0 ||
	    size > TEST_BUF_SIZE ||
	    user != NVMEM_USER_0)
		return EC_ERROR_UNIMPLEMENTED;

	if (!data)
		return EC_ERROR_INVAL;

	memcpy(flash_buffer, data, size);

	return EC_SUCCESS;
}

/****************************************************************************/
/* Supporting routines */

static void zero_flash(void)
{
	/* Invalidate the RAM cache */
	writeenv();

	/* Zero flash */
	memset(flash_buffer, 0, sizeof(flash_buffer));
}

static void dump(const char *name, const uint8_t *ptr, uint32_t len)
{
	int i;

	ccprintf(" %s(%d): \"", name, len);
	for (i = 0; i < len; i++)
		if (isprint(ptr[i]))
			ccprintf("%c", ptr[i]);
		else
			ccprintf("\\x%02x", ptr[i]);
	ccprintf("\"\n");
}

static int flash_matches(const uint8_t *correct, uint32_t correct_len)
{
	if (memcmp(flash_buffer, correct, correct_len) == 0)
		return 1;

	ccprintf("\n");
	dump("expected val", correct, correct_len);
	dump("flash_buffer", flash_buffer, correct_len);
	return 0;
}


static int str_matches_lengths;
static int str_matches(const char *key, const uint8_t *expected_val)
{
	const char *gs = getenv(key);

	if (!expected_val && !gs)
		return 1;

	if (expected_val && !gs) {
		ccprintf("expected \"%s\", got NULL\n", expected_val);
		return 0;
	}

	if (!expected_val && gs) {
		ccprintf("expected NULL, got \"%s\"\n", gs);
		return 0;
	}

	if (strcmp(expected_val, gs)) {
		ccprintf("expected \"%s\", got \"%s\"\n", expected_val, gs);
		return 0;
	}

	str_matches_lengths += strlen(key) + 1;
	str_matches_lengths += strlen(expected_val) + 1;
	return 1;
}


/****************************************************************************/
/* Tests */


static int simple_search(void)
{
	const char preload[] =
		"ho=yo\0"
		"yo=ho=yo\0"
		"mo=yo=no=yo\0";
	const char *gs;

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));

	gs = getenv("no");
	TEST_ASSERT(!gs);

	gs = getenv("ho");
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, "yo") == 0);

	gs = getenv("yo");
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, "ho=yo") == 0);

	gs = getenv("mo");
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, "yo=no=yo") == 0);

	return EC_SUCCESS;
}

static int simple_write(void)
{
	const char after_one[] = "ho=yo\0";
	const char after_two[] = "ho=yo\0yo=ho=yo\0";
	const char after_three[] = "ho=yo\0yo=ho=yo\0mo=yo=no=yo\0";

	zero_flash();

	TEST_ASSERT(setenv("ho", "yo") == EC_SUCCESS);
	TEST_ASSERT(writeenv() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_one, sizeof(after_one)));

	TEST_ASSERT(setenv("yo", "ho=yo") == EC_SUCCESS);
	TEST_ASSERT(writeenv() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_two, sizeof(after_two)));

	TEST_ASSERT(setenv("mo", "yo=no=yo") == EC_SUCCESS);
	TEST_ASSERT(writeenv() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_three, sizeof(after_three)));

	return EC_SUCCESS;
}

static int complex_write(void)
{
	zero_flash();

	/* Do a bunch of writes and erases */
	setenv("ho", "aa");
	setenv("zo", "nn");
	setenv("yo", "CCCCCCCC");
	setenv("zooo", "yyyyyyy");
	setenv("yo", "AA");
	setenv("ho", 0);
	setenv("yi", "BBB");
	setenv("yi", "AA");
	setenv("hixx", 0);
	setenv("yo", "BBB");
	setenv("zo", "");
	setenv("hi", "bbb");
	setenv("ho", "cccccc");
	setenv("yo", "");
	setenv("zo", "ggggg");

	/* What do we expect to find? */
	str_matches_lengths = 0;		/* count storage used */
	TEST_ASSERT(str_matches("hi", "bbb"));
	TEST_ASSERT(str_matches("hixx", 0));
	TEST_ASSERT(str_matches("ho", "cccccc"));
	TEST_ASSERT(str_matches("yi", "AA"));
	TEST_ASSERT(str_matches("yo", 0));
	TEST_ASSERT(str_matches("zo", "ggggg"));
	TEST_ASSERT(str_matches("zooo", "yyyyyyy"));

	/* That should be ALL we find, too */
	TEST_ASSERT(writeenv() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[str_matches_lengths] == '\0');
	TEST_ASSERT(flash_buffer[str_matches_lengths - 1] == '\0');

	return EC_SUCCESS;
}

static int invalid_stuff(void)
{
	int i;
	const char *gs;
	char key[10];

	zero_flash();

	/* Putting '\0' in a value just truncates it */
	TEST_ASSERT(setenv("foo", "bar\0hey") == EC_SUCCESS);
	gs = getenv("foo");
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, "bar") == 0);

	/* bogus keys should be rejected */
	TEST_ASSERT(getenv("foo=bar") == 0);
	TEST_ASSERT(setenv("foo=bar", "hey") == EC_ERROR_INVAL);

	zero_flash();

	/* Fill up the storage */
	TEST_ASSERT(TEST_BUF_SIZE == 300);	/* magic numbers */
	for (i = 0; i < 29; i++) {
		/* 6-char key, '=' 2-char val, '\0' == 10 chars */
		snprintf(key, sizeof(key), "key_%02d", i);
		TEST_ASSERT(setenv(key, "aa") == EC_SUCCESS);
	}
	/* Should be nine bytes left in rbuf. This won't fit. */
	TEST_ASSERT(setenv("key_29", "aa") == EC_ERROR_OVERFLOW);
	/* But this will */
	TEST_ASSERT(setenv("key_29", "a") == EC_SUCCESS);

	return EC_SUCCESS;
}

static int check_init(void)
{
	const char bad_key[] = "A=a\0=b\0";
	const char bad_val[] = "A=a\0B=\0";
	const char good[] = "A=a\0B=b\0";

	zero_flash();
	TEST_ASSERT(initenv() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memset(flash_buffer, 0xff, sizeof(flash_buffer));
	TEST_ASSERT(initenv() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	strcpy(flash_buffer, "=A");
	TEST_ASSERT(initenv() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	strcpy(flash_buffer, "A=");
	TEST_ASSERT(initenv() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memcpy(flash_buffer, bad_key, sizeof(bad_key));
	TEST_ASSERT(initenv() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memcpy(flash_buffer, bad_val, sizeof(bad_val));
	TEST_ASSERT(initenv() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memcpy(flash_buffer, good, sizeof(good));
	TEST_ASSERT(initenv() == EC_SUCCESS);
	TEST_ASSERT(memcmp(flash_buffer, good, sizeof(good)) == 0);

	return EC_SUCCESS;
}


void run_test(void)
{
	test_reset();

	RUN_TEST(simple_search);
	RUN_TEST(simple_write);
	RUN_TEST(complex_write);
	RUN_TEST(invalid_stuff);
	RUN_TEST(check_init);

	/*
	 * NOTE: These tests assume that only one thread at a time will ever
	 * make changes. If simultaneous access is expected, we should add
	 * threaded tests to ensure that changes are atomic and
	 * non-conflicting.
	 */
	test_print_result();
}
