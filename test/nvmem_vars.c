/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test of the key=val variable implementation (set, get, delete, etc).
 */

#include <ctype.h>
#include <string.h>

#include "common.h"
#include "compile_time_macros.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "printf.h"
#include "shared_mem.h"
#include "test_util.h"

uint32_t nvmem_user_sizes[] = {
	CONFIG_FLASH_NVMEM_VARS_USER_SIZE,
};
BUILD_ASSERT(ARRAY_SIZE(nvmem_user_sizes) == NVMEM_NUM_USERS);

/****************************************************************************/
/* Mock the flash storage */

static uint8_t ram_buffer[CONFIG_FLASH_NVMEM_VARS_USER_SIZE];
static uint8_t flash_buffer[CONFIG_FLASH_NVMEM_VARS_USER_SIZE];

int nvmem_read(uint32_t startOffset, uint32_t size,
	       void *data, enum nvmem_users user)
{
	/* Our mocks make some assumptions */
	if (startOffset != 0 ||
	    size > CONFIG_FLASH_NVMEM_VARS_USER_SIZE ||
	    user != CONFIG_FLASH_NVMEM_VARS_USER_NUM)
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
	    size > CONFIG_FLASH_NVMEM_VARS_USER_SIZE ||
	    user != CONFIG_FLASH_NVMEM_VARS_USER_NUM)
		return EC_ERROR_UNIMPLEMENTED;

	if (!data)
		return EC_ERROR_INVAL;

	memcpy(ram_buffer, data, size);

	return EC_SUCCESS;
}

int nvmem_commit(void)
{
	memcpy(flash_buffer, ram_buffer, CONFIG_FLASH_NVMEM_VARS_USER_SIZE);
	return EC_SUCCESS;
}

/****************************************************************************/
/* Supporting routines */

static void zero_flash(void)
{
	/* Invalidate the RAM cache */
	writevars();

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
	const char *gs = getvar(key);

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

/* White-box test for getvar_kv() */
int get_local_copy(void);
void release_local_copy(void);
int getvar_kv(const char *key, uint32_t *kidx, uint32_t *vidx);

static int check_getvar_kv(void)
{
	const char preload[] =
		"ho=yo\0"
		"yo=ho\0";
	int k, v;

	/* Flash is all zeros, no match found */
	zero_flash();
	TEST_ASSERT(get_local_copy() == EC_SUCCESS); /* load rbuf */

	k = v = 1000;
	TEST_ASSERT(getvar_kv("nomatch", &k, &v) == 0);
	TEST_ASSERT(k == 0);
	TEST_ASSERT(v == k);
	release_local_copy();

	/* Flash has values in it, no match found */
	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));
	TEST_ASSERT(get_local_copy() == EC_SUCCESS); /* load rbuf */

	k = v = 1000;
	TEST_ASSERT(getvar_kv("nomatch", &k, &v) == 0);
	TEST_ASSERT(k == sizeof(preload) - 1);	/* [k] is final '\0' */
	TEST_ASSERT(v == k);

	/* Match found at first variable */
	k = v = 1000;
	TEST_ASSERT(getvar_kv("ho", &k, &v));
	TEST_ASSERT(k == 0);			/* ho=yo starts at [0] */
	TEST_ASSERT(v == 3);

	/* Match found at second variable */
	k = v = 1000;
	TEST_ASSERT(getvar_kv("yo", &k, &v));
	TEST_ASSERT(k == 6);			/* yo=ho starts at [6] */
	TEST_ASSERT(v == 9);

	release_local_copy();

	return EC_SUCCESS;
}

static int simple_search(void)
{
	const char preload[] =
		"ho=yo\0"
		"yo=ho=yo\0"
		"mo=yo=no=yo\0";
	const char *gs;

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));

	gs = getvar("no");
	TEST_ASSERT(!gs);

	gs = getvar("ho");
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, "yo") == 0);

	gs = getvar("yo");
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, "ho=yo") == 0);

	gs = getvar("mo");
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

	TEST_ASSERT(setvar("ho", "yo") == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_one, sizeof(after_one)));

	TEST_ASSERT(setvar("yo", "ho=yo") == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_two, sizeof(after_two)));

	TEST_ASSERT(setvar("mo", "yo=no=yo") == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_three, sizeof(after_three)));

	return EC_SUCCESS;
}

static int simple_delete(void)
{
	const char preload[] =
		"ho=yo\0"
		"yo=ho=yo\0"
		"mo=yo=no=yo\0";
	const char after_ho[] =
		"yo=ho=yo\0"
		"mo=yo=no=yo\0";
	const char after_yo[] =
		"ho=yo\0"
		"mo=yo=no=yo\0";
	const char after_mo[] =
		"ho=yo\0"
		"yo=ho=yo\0";
	const char mo_then_ho[] =
		"yo=ho=yo\0";
	const char no_mo_yo[] =
		"\0";

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));
	TEST_ASSERT(setvar("ho", "") == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_ho, sizeof(after_ho)));

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));
	TEST_ASSERT(setvar("ho", 0) == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_ho, sizeof(after_ho)));

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));
	TEST_ASSERT(setvar("yo", "") == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_yo, sizeof(after_yo)));

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));
	TEST_ASSERT(setvar("yo", 0) == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_yo, sizeof(after_yo)));

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));
	TEST_ASSERT(setvar("mo", "") == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_mo, sizeof(after_mo)));

	zero_flash();
	memcpy(flash_buffer, preload, sizeof(preload));
	TEST_ASSERT(setvar("mo", 0) == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(after_mo, sizeof(after_mo)));

	/* eat the rest */

	TEST_ASSERT(setvar("ho", 0) == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(mo_then_ho, sizeof(mo_then_ho)));

	TEST_ASSERT(setvar("yo", 0) == EC_SUCCESS);
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_matches(no_mo_yo, sizeof(no_mo_yo)));

	return EC_SUCCESS;
}

static int complex_write(void)
{
	zero_flash();

	/* Do a bunch of writes and erases */
	setvar("ho", "aa");
	setvar("zo", "nn");
	setvar("yo", "CCCCCCCC");
	setvar("zooo", "yyyyyyy");
	setvar("yo", "AA");
	setvar("ho", 0);
	setvar("yi", "BBB");
	setvar("yi", "AA");
	setvar("hixx", 0);
	setvar("yo", "BBB");
	setvar("zo", "");
	setvar("hi", "bbb");
	setvar("ho", "cccccc");
	setvar("yo", "");
	setvar("zo", "ggggg");

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
	TEST_ASSERT(writevars() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[str_matches_lengths] == '\0');
	TEST_ASSERT(flash_buffer[str_matches_lengths - 1] == '\0');

	return EC_SUCCESS;
}

static int weird_keys(void)
{
	char keyA[256];
	char keyB[256];
	const char *valA = "this is A";
	const char *valB = "THIS IS b";
	int c, i;
	const char *gs;

	zero_flash();

	/* Keys can use any char except '=' and '\0' */

	/* keyA is increasing order */
	i = 0;
	for (c = 0; c < 256; c++)
		if (c != 0 && c != '=')
			keyA[i++] = c;
	keyA[i++] = '\0';

	/* keyB is decreasing order */
	i = 0;
	for (c = 255; c >= 0; c--)
		if (c != 0 && c != '=')
			keyB[i++] = c;
	keyB[i++] = '\0';

	TEST_ASSERT(setvar(keyA, valA) == EC_SUCCESS);
	TEST_ASSERT(setvar(keyB, valB) == EC_SUCCESS);

	gs = getvar(keyA);
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, valA) == 0);

	gs = getvar(keyB);
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, valB) == 0);

	return EC_SUCCESS;
}

static int weird_values(void)
{
	const char *keyA = "this is A";
	const char *keyB = "THIS IS b";
	const char *keyC = "c";
	char valA[256];
	char valB[256];
	const char *valC = "=c=C=c=";
	int c, i;
	const char *gs;

	zero_flash();

	/* Values can use any char except '\0' */

	/* valA is increasing order */
	i = 0;
	for (c = 0; c < 256; c++)
		if (c != 0)
			valA[i++] = c;
	valA[i++] = '\0';

	/* valB is decreasing order */
	i = 0;
	for (c = 255; c >= 0; c--)
		if (c != 0)
			valB[i++] = c;
	valB[i++] = '\0';

	TEST_ASSERT(setvar(keyA, valA) == EC_SUCCESS);
	TEST_ASSERT(setvar(keyB, valB) == EC_SUCCESS);
	TEST_ASSERT(setvar(keyC, valC) == EC_SUCCESS);

	gs = getvar(keyA);
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, valA) == 0);

	gs = getvar(keyB);
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, valB) == 0);

	gs = getvar(keyC);
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, valC) == 0);

	return EC_SUCCESS;
}

static int invalid_stuff(void)
{
	int i, n;
	const char *gs;
	char key[10];

	zero_flash();

	/* Putting '\0' in a value just truncates it */
	TEST_ASSERT(setvar("foo", "bar\0hey") == EC_SUCCESS);
	gs = getvar("foo");
	TEST_ASSERT(gs);
	TEST_ASSERT(strcmp(gs, "bar") == 0);

	/* bogus keys should be rejected */
	TEST_ASSERT(getvar("foo=bar") == 0);
	TEST_ASSERT(setvar("foo=bar", "hey") == EC_ERROR_INVAL);

	zero_flash();

	/*
	 * Some magic numbers here, because we want to use up 10 bytes at a
	 * time and end up with exactly 9 free bytes left.
	 */
	TEST_ASSERT(CONFIG_FLASH_NVMEM_VARS_USER_SIZE % 10 == 0);
	n = CONFIG_FLASH_NVMEM_VARS_USER_SIZE / 10;
	TEST_ASSERT(n < 1000);

	/* Fill up the storage */
	for (i = 0; i < n - 1; i++) {
		/* 6-char key, '=', 2-char val, '\0' == 10 chars */
		snprintf(key, sizeof(key), "key%03d", i);
		TEST_ASSERT(setvar(key, "aa") == EC_SUCCESS);
	}

	/*
	 * Should be nine bytes left in rbuf (because we need one more '\0' at
	 * the end). This won't fit.
	 */
	TEST_ASSERT(setvar("key999", "aa") == EC_ERROR_OVERFLOW);
	/* But this will. */
	TEST_ASSERT(setvar("key999", "a") == EC_SUCCESS);
	/* And this, because it replaces a previous entry */
	TEST_ASSERT(setvar("key000", "bc") == EC_SUCCESS);
	/* But this still won't fit */
	TEST_ASSERT(setvar("key999", "de") == EC_ERROR_OVERFLOW);

	return EC_SUCCESS;
}

static int check_init(void)
{
	const char bad_key[] = "A=a\0=b\0";
	const char bad_val[] = "A=a\0B=\0";
	const char good[] = "A=a\0B=b\0";

	zero_flash();
	TEST_ASSERT(initvars() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memset(flash_buffer, 0xff, sizeof(flash_buffer));
	TEST_ASSERT(initvars() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	strcpy(flash_buffer, "=A");
	TEST_ASSERT(initvars() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	strcpy(flash_buffer, "A=");
	TEST_ASSERT(initvars() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memcpy(flash_buffer, bad_key, sizeof(bad_key));
	TEST_ASSERT(initvars() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memcpy(flash_buffer, bad_val, sizeof(bad_val));
	TEST_ASSERT(initvars() == EC_SUCCESS);
	TEST_ASSERT(flash_buffer[0] == '\0' && flash_buffer[1] == '\0');

	zero_flash();
	memcpy(flash_buffer, good, sizeof(good));
	TEST_ASSERT(initvars() == EC_SUCCESS);
	TEST_ASSERT(memcmp(flash_buffer, good, sizeof(good)) == 0);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(check_getvar_kv);
	RUN_TEST(simple_search);
	RUN_TEST(simple_write);
	RUN_TEST(simple_delete);
	RUN_TEST(complex_write);
	RUN_TEST(weird_keys);
	RUN_TEST(weird_values);
	RUN_TEST(invalid_stuff);
	RUN_TEST(check_init);

	/*
	 * NOTE: As mentioned in nvmem_vars.h, the current implementation
	 * assumes that only one thread at a time will ever make changes.
	 * These tests are no different.
	 */

	test_print_result();
}
