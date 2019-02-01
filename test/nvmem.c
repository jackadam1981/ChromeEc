/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test Cr-50 Non-Voltatile memory module
 */

#include "common.h"
#include "console.h"
#include "crc.h"
#include "flash.h"
#include "new_nvmem.h"
#include "nvmem.h"
#include "printf.h"
#include "shared_mem.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

#define WRITE_SEGMENT_LEN 200
#define WRITE_READ_SEGMENTS 4

const uint8_t legacy_nvmem_image[] = {
	#include "legacy_nvmem_dump.h"
};

struct size_check {
	uint8_t check[(sizeof(legacy_nvmem_image) == NVMEM_PARTITION_SIZE) ?
		      1 : -1];
};

static uint8_t write_buffer[NVMEM_PARTITION_SIZE];
static int flash_write_fail;

struct nvmem_test_result {
	int var_count;
	int reserved_obj_count;
	int evictable_obj_count;
	int deleted_obj_count;
	int unexpected_count;
	size_t valid_data_size;
	size_t erased_data_size;
};

static struct nvmem_test_result test_result;

int app_cipher(const void *salt_p, void *out_p, const void *in_p, size_t size)
{

	const uint8_t *in = in_p;
	uint8_t *out = out_p;
	const uint8_t *salt = salt_p;
	size_t i;

	for (i = 0; i < size; i++)
		out[i] = in[i] ^ salt[i % CIPHER_SALT_SIZE];

	return 1;
}

void app_compute_hash(uint8_t *p_buf, size_t num_bytes,
		      uint8_t *p_hash, size_t hash_bytes)
{
	uint32_t crc;
	uint32_t *p_data;
	int n;
	size_t tail_size;

	crc32_init();
	/* Assuming here that buffer is 4 byte aligned and that num_bytes is
	 * divisible by 4
	 */
	p_data = (uint32_t *)p_buf;
	for (n = 0; n < num_bytes/4; n++)
		crc32_hash32(*p_data++);

	tail_size = num_bytes % 4;
	if (tail_size) {
		uint32_t tail;

		tail = 0;
		memcpy(&tail, p_data, tail_size);
		crc32_hash32(tail);
	}

	/*
	 * Crc32 of 0xffffffff is 0xffffffff. Let's spike the results to avoid
	 * this unfortunate Crc32 property.
	 */
	crc = crc32_result() ^ 0x55555555;

	for (n = 0; n < hash_bytes; n += sizeof(crc)) {
		size_t copy_bytes = MIN(sizeof(crc), hash_bytes - n);

		memcpy(p_hash + n, &crc, copy_bytes);
	}
}

/* Used to allow/prevent Flash erase/write operations */
int flash_pre_op(void)
{
	return flash_write_fail ? EC_ERROR_UNKNOWN : EC_SUCCESS;
}

static void dump_nvmem_state(const char *title, const struct nvmem_test_result *tr)
{
	ccprintf("\n%s:\n", title);
	ccprintf("deleted_obj_count: %d\n", tr->deleted_obj_count);
	ccprintf("var_count: %d\n", tr->var_count);
	ccprintf("reserved_obj_count: %d\n", tr->reserved_obj_count);
	ccprintf("evictable_obj_count: %d\n", tr->evictable_obj_count);
	ccprintf("unexpected_count: %d\n", tr->unexpected_count);
	ccprintf("valid_data_size: %d\n", tr->valid_data_size);
	ccprintf("erased_data_size: %d\n\n", tr->erased_data_size);
}

static int prepare_nvmem_contents(void)
{
	struct nvmem_tag *tag;

	memcpy(write_buffer, legacy_nvmem_image, sizeof(write_buffer));
	tag = (struct nvmem_tag *)write_buffer;

	app_compute_hash(tag->padding, NVMEM_PARTITION_SIZE - NVMEM_SHA_SIZE,
			 tag->sha, sizeof(tag->sha));
	app_cipher(tag->sha, tag + 1, tag + 1,
		   NVMEM_PARTITION_SIZE - sizeof(struct nvmem_tag));

	return flash_physical_write(CONFIG_FLASH_NVMEM_BASE_A -
				    CONFIG_PROGRAM_MEMORY_BASE,
				    sizeof(write_buffer), write_buffer);
}

static int iterate_over_flash(void)
{
	struct page_tracker pt = {};
	uint8_t buf[CONFIG_FLASH_BANK_SIZE];
	enum ec_error_list rv;
	struct nn_container *ch;

	memset(&test_result, 0, sizeof(test_result));
	ch = (struct nn_container *)buf;

	while ((rv = get_next_object(&pt, ch, 1))
	       == EC_SUCCESS)
		switch (ch->container_type) {
		case NN_OBJ_OLD_COPY:
			test_result.deleted_obj_count++;
			test_result.erased_data_size += ch->size + sizeof(*ch);
			break;

		case NN_OBJ_TUPLE:
			test_result.var_count++;
			test_result.valid_data_size += ch->size + sizeof(*ch);
			break;

		case NN_OBJ_TPM_RESERVED:
			test_result.reserved_obj_count++;
			test_result.valid_data_size += ch->size + sizeof(*ch);
			break;

		case NN_OBJ_TPM_OBJECT:
			test_result.evictable_obj_count++;
			test_result.valid_data_size += ch->size + sizeof(*ch);
			break;

		default:
			test_result.unexpected_count++;
			break;
		}

	if (rv !=  EC_ERROR_MEMORY_ALLOCATION) {
		ccprintf("\n%s:%d - unexpected return value %d\n",
			 __func__, __LINE__, rv);
		return rv;
	}
	return EC_SUCCESS;
}

static void *page_to_flash_addr(int page_num)
{
	uint32_t base_offset = CONFIG_FLASH_NEW_NVMEM_BASE_A;

	if (page_num > NEW_NVMEM_TOTAL_PAGES)
		return NULL;

	if (page_num >= (NEW_NVMEM_TOTAL_PAGES/2)) {
		page_num -= (NEW_NVMEM_TOTAL_PAGES/2);
		base_offset = CONFIG_FLASH_NEW_NVMEM_BASE_B;
	}

	return (void *)((uintptr_t)base_offset +
			page_num * CONFIG_FLASH_BANK_SIZE);
}

static int post_inint_from_scratch(uint8_t flash_value)
{
	int i;
	void *flash_p;

	memset(write_buffer, flash_value, sizeof(write_buffer));

	/* Overwrite nvmem flash space with junk value. */
	flash_physical_write(CONFIG_FLASH_NEW_NVMEM_BASE_A -\
			     CONFIG_PROGRAM_MEMORY_BASE,
			     NEW_FLASH_HALF_NVMEM_SIZE,
			     (const char *)write_buffer);
	flash_physical_write(CONFIG_FLASH_NEW_NVMEM_BASE_B -\
			     CONFIG_PROGRAM_MEMORY_BASE,
			     NEW_FLASH_HALF_NVMEM_SIZE,
			     (const char *)write_buffer);

	TEST_ASSERT(nvmem_init() == EC_SUCCESS);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);
	TEST_ASSERT(test_result.var_count == 0);
	TEST_ASSERT(test_result.reserved_obj_count == 36);
	TEST_ASSERT(test_result.evictable_obj_count == 0);
	TEST_ASSERT(test_result.deleted_obj_count == 0);
	TEST_ASSERT(test_result.unexpected_count == 0);
	TEST_ASSERT(test_result.valid_data_size == 1258);
	TEST_ASSERT(total_var_space == 0);

	for (i = 0; i < (NEW_NVMEM_TOTAL_PAGES - 1); i++) {
		flash_p = page_to_flash_addr(i);

		TEST_ASSERT(!!flash_p);
		TEST_ASSERT(is_uninitialized(flash_p, CONFIG_FLASH_BANK_SIZE));
	}

	flash_p = page_to_flash_addr(i);
	TEST_ASSERT(!is_uninitialized(flash_p, CONFIG_FLASH_BANK_SIZE));

	return EC_SUCCESS;
}

/*
 * The purpose of this test is to check NvMem intialization when NvMem is
 * completely erased (i.e. following SpiFlash write of program). In this case,
 * nvmem_init() is expected to create initial flash storage containing
 * reserved objects only.
 */
static int test_fully_erased_nvmem(void)
{

	return post_inint_from_scratch(0xff);
}

/*
 * The purpose of this test is to check nvmem_init() in the case when no valid
 * pages exist but flash space is garbled as opposed to be fully erased. In
 * this case, the initialization is expected to create one new valid page and
 * erase the rest of the pages.
 */
static int test_corrupt_nvmem(void)
{
	return post_inint_from_scratch(0x55);
}

static int prepare_new_flash(void)
{
	TEST_ASSERT(test_fully_erased_nvmem() == EC_SUCCESS);
ccprintf("%s:%d\n", __func__, __LINE__);

	/* Now copy sensible information into the nvmem cache. */
	memcpy(nvmem_cache_base(NVMEM_TPM),
	       legacy_nvmem_image + sizeof(struct nvmem_tag),
	       nvmem_user_sizes[NVMEM_TPM]);

ccprintf("%s:%d\n", __func__, __LINE__);
	TEST_ASSERT(new_nvmem_save() == EC_SUCCESS);
ccprintf("%s:%d\n", __func__, __LINE__);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);

	dump_nvmem_state("after first save", &test_result);
	TEST_ASSERT(test_result.deleted_obj_count == 22);
	TEST_ASSERT(test_result.var_count == 0);
	TEST_ASSERT(test_result.reserved_obj_count == 38);
	TEST_ASSERT(test_result.evictable_obj_count == 9);
	TEST_ASSERT(test_result.unexpected_count == 0);
	TEST_ASSERT(test_result.valid_data_size == 5353);
	TEST_ASSERT(test_result.erased_data_size == 798);

	return EC_SUCCESS;
}

static int test_nvmem_save(void)
{
	struct nvmem_test_result old_result;
	const char *key = "var1";
	const char *value = "value of var 1";
	size_t total_var_size;

	TEST_ASSERT(prepare_new_flash() == EC_SUCCESS);

	/*
	 * Verify that saving without changing the cache does not affect flash
	 * contents.
	 */
	old_result = test_result;
	TEST_ASSERT(new_nvmem_save() == EC_SUCCESS);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);
	TEST_ASSERT(memcmp(&test_result, &old_result, sizeof(test_result)) == 0);

	/*
	 * Total size test variable storage takes in flash (container header
	 * size not included).
	 */
	total_var_size = strlen(key) + strlen(value) +
		sizeof(struct tuple) + sizeof(struct nn_container);

	/* Verify that we can add a variable to nvmem. */
	TEST_ASSERT(setvar(key, strlen(key), value, strlen(value)) == EC_SUCCESS);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);

	/* Remove changes caused by the new var addition. */
	test_result.var_count -= 1;
	test_result.valid_data_size -= total_var_size;
	TEST_ASSERT(memcmp(&test_result, &old_result, sizeof(test_result)) == 0);

	/* Verify that we can delete a variable from nvmem. */
	TEST_ASSERT(setvar(key, strlen(key), NULL, 0) == EC_SUCCESS);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);
	dump_nvmem_state("test_result", &test_result);
	dump_nvmem_state("old result", &old_result);
	test_result.deleted_obj_count -= 1;
	test_result.erased_data_size -= total_var_size;
	TEST_ASSERT(memcmp(&test_result, &old_result, sizeof(test_result)) == 0);

	return EC_SUCCESS;
}

static size_t get_free_nvmem_room(void)
{
	size_t free_room;
	size_t free_pages;
	/* Compaction kicks in when 3 pages or less are left. */
	const size_t max_pages = NEW_NVMEM_TOTAL_PAGES - 3;

	if (master_pt.list_index >= max_pages)
		return 0;

	free_pages = max_pages - master_pt.list_index;
	free_room = (free_pages - 1) * (CONFIG_FLASH_BANK_SIZE -
					sizeof(struct nn_page_header)) +
		CONFIG_FLASH_BANK_SIZE - master_pt.data_offset;

	return free_room;
}

static int test_nvmem_compaction(void)
{
	char value[100]; /* Definitely more than enough. */
	const char *key = "var 1";
	int i;
	size_t key_len;
	size_t val_len;
	size_t free_room;
	size_t var_space;
	int max_vars;

	// struct nvmem_test_result old_result;

	key_len = strlen(key);
	val_len = snprintf(value, sizeof(value), "variable value is %04d", 0);

	TEST_ASSERT(prepare_new_flash() == EC_SUCCESS);

	/* Let's see how much free room there is. */

	free_room = get_free_nvmem_room();
	TEST_ASSERT(free_room != 0);
	/*
	 * See how many vars should be able to fit there.
	 *
	 * First calculate rounded up space a var will take.
	 */
	var_space = (val_len +
		     key_len +
		     sizeof(struct tuple) +
		     sizeof(struct nn_container) + 3) & ~3;

	max_vars = free_room/var_space;

	for (i = 0; i <= max_vars; i++) {
		snprintf(value, sizeof(value), "variable value is %04d", i);
		TEST_ASSERT(setvar(key, key_len, value, val_len) == EC_SUCCESS);
	}

	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);
	TEST_ASSERT(test_result.erased_data_size >
		    ((max_vars - 1) * var_space));

	/* This will take it over the compaction limit. */
	val_len = snprintf(value, sizeof(value), "variable value is %03d", i);
	TEST_ASSERT(setvar(key, key_len, value, val_len) == EC_SUCCESS);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);
	TEST_ASSERT(test_result.erased_data_size < var_space);

	return EC_SUCCESS;
}

static int test_configured_nvmem(void)
{
	/*
	 * The purpose of this test is to check how nvmem_init() initializes
	 * from previously saved flash contents.
	 */
	TEST_ASSERT(prepare_nvmem_contents() == EC_SUCCESS);

	/*
	 * This is initialization from legacy flash contents which replaces
	 * legacy flash image with the new format flash image
	 */
	TEST_ASSERT(nvmem_init() == EC_SUCCESS);

	/* Call NvMem initialization */
	return nvmem_init();
}

static  uint8_t find_lb(const void *data)
{
	return (const uint8_t *)memchr(data, '#', 256) - (const uint8_t *)data;
}

/*
 * Helper function, depending on the argument value either writes variables
 * into nvmem and verifies their presence, or deletes them and verifies that
 * they indeed disappear.
 */
static int var_read_write_delete_helper(int do_write)
{
	size_t i;
	uint16_t saved_total_var_space;
	uint32_t coverage_map;
	const struct {
		uint8_t *key;
		uint8_t *value;
	} kv_pairs [] = {
		/* Use # as the delimiter to allow \0 in keys/values. */
		{ "\0key\00#", "value of key2#" },
		{ "key1#", "value of key1#" },
		{ "key2#", "value of key2#" },
		{ "key3#", "value of\0 key3#" },
		{ "ke\04#", "value\0 of\0 key4#" },
	};

	coverage_map = 0;
	saved_total_var_space = total_var_space;

	/*
	 * Read all vars, one at a time, verifying that they shows up in
	 * getvar results when appropriate but not before.
	 */
	for (i = 0; i <= ARRAY_SIZE(kv_pairs); i++) {
		size_t j;
		uint8_t key_len;
		uint8_t val_len;
		const void *value;

		for (j = 0; j < ARRAY_SIZE(kv_pairs); j++) {
			struct tuple *t;


			coverage_map |= 1;

			key_len = find_lb(kv_pairs[j].key);
			t = getvar(kv_pairs[j].key, key_len);

			if ((j >= i) ^ !do_write) {
				TEST_ASSERT(t == NULL);
				continue;
			}

			coverage_map |= 2;

			// TEST_ASSERT(saved_total_var_space == total_var_space);

			/* Confirm that what we found is the right variable. */
			val_len = find_lb(kv_pairs[j].value);

			TEST_ASSERT(t->key_len == key_len);
			TEST_ASSERT(t->val_len == val_len);
			TEST_ASSERT(!memcmp(kv_pairs[j].key, t->data_, key_len));
			TEST_ASSERT(!memcmp(kv_pairs[j].value, t->data_ + key_len, val_len));
			freevar(t);
		}

		if (i == ARRAY_SIZE(kv_pairs)) {
			coverage_map |= 4;
			/* All four variables have been processed. */
			break;
		}

		val_len = find_lb(kv_pairs[i].value);
		key_len = find_lb(kv_pairs[i].key);
		value = kv_pairs[i].value;
		if (!do_write) {

			coverage_map |= 8;

			saved_total_var_space -= val_len + key_len;
			/*
			 * Make sure all val_len == 0 and val == NULL
			 * combinations are exercised.
			 */
			switch (i) {
			case 0:
				val_len = 0;
				coverage_map |= 0x10;
				break;

			case 1:
				coverage_map |= 0x20;
				value = NULL;
				break;
			default:
				coverage_map |= 0x40;
				val_len = 0;
				value = NULL;
				break;
			}
		} else {
			coverage_map |= 0x80;
			saved_total_var_space += val_len + key_len;
		}
		key_len = find_lb(kv_pairs[i].key);
		TEST_ASSERT(setvar(kv_pairs[i].key, key_len,
				   value, val_len) == EC_SUCCESS);

		TEST_ASSERT(saved_total_var_space == total_var_space);
	}

	if (do_write)
		TEST_ASSERT(coverage_map == 0x87);
	else
		TEST_ASSERT(coverage_map == 0x7f);

	return EC_SUCCESS;
}

static int test_var_read_write_delete(void)
{
	TEST_ASSERT(post_inint_from_scratch(0xff) == EC_SUCCESS);

	ccprintf("\n%s: starting write cycle\n", __func__);
	TEST_ASSERT(var_read_write_delete_helper(1) == EC_SUCCESS);

	ccprintf("%s: starting delete cycle\n", __func__);
	TEST_ASSERT(var_read_write_delete_helper(0) == EC_SUCCESS);

	return EC_SUCCESS;
}
/* Verify that nvmem_erase_user_data only erases the given user's data. */
static int test_nvmem_erase_tpm_data(void)
{
	TEST_ASSERT(prepare_nvmem_contents() == EC_SUCCESS);
	TEST_ASSERT(nvmem_init() == EC_SUCCESS);
	browse_flash_contents(1);
	TEST_ASSERT(nvmem_erase_tpm_data() == EC_SUCCESS);
	browse_flash_contents(1);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);
	TEST_ASSERT(test_result.deleted_obj_count == 0);
	TEST_ASSERT(test_result.var_count == 3);
	TEST_ASSERT(test_result.reserved_obj_count == 0);
	TEST_ASSERT(test_result.evictable_obj_count == 0);
	TEST_ASSERT(test_result.unexpected_count == 0);
	TEST_ASSERT(test_result.valid_data_size == 101);
	TEST_ASSERT(test_result.erased_data_size == 0);

	return EC_SUCCESS;
}

int nvmem_first_task(void *unused)
{
#if 0
	uint32_t offset = 0;
	uint32_t num_bytes = WRITE_SEGMENT_LEN;
	int user = NVMEM_USER_0;

	task_wait_event(0);
	/* Generate source data */
	generate_random_data(0, num_bytes);
	nvmem_write(0, num_bytes, &write_buffer[offset], user);
	/* Read from cache memory */
	nvmem_read(0, num_bytes, read_buffer, user);
	/* Verify that write to nvmem was successful */
	TEST_ASSERT_ARRAY_EQ(write_buffer, read_buffer, num_bytes);
	/* Wait here with mutex held by this task */
	task_wait_event(0);
	/* Write to flash which releases nvmem mutex */
	nvmem_commit();
	nvmem_read(0, num_bytes, read_buffer, user);
	/* Verify that write to flash was successful */
	TEST_ASSERT_ARRAY_EQ(write_buffer, read_buffer, num_bytes);
#endif
	return EC_SUCCESS;
}

int nvmem_second_task(void *unused)
{
#if 0
	uint32_t offset = WRITE_SEGMENT_LEN;
	uint32_t num_bytes = WRITE_SEGMENT_LEN;
	int user = NVMEM_USER_0;

	task_wait_event(0);

	/* Gen test data and don't overwite test data generated by 1st task */
	generate_random_data(offset, num_bytes);
	/* Write test data at offset 0 nvmem user buffer */
	nvmem_write(0, num_bytes, &write_buffer[offset], user);
	/* Write to flash */
	nvmem_commit();
	/* Read from nvmem */
	nvmem_read(0, num_bytes, read_buffer, user);
	/* Verify that write to nvmem was successful */
	TEST_ASSERT_ARRAY_EQ(&write_buffer[offset], read_buffer, num_bytes);
	/* Clear flag to indicate lock test is complete */
	lock_test_started = 0;
#endif
	return EC_SUCCESS;
}

static void run_test_setup(void)
{
	/* Allow Flash erase/writes */
	flash_write_fail = 0;
	test_reset();
}

void nvmem_wipe_cache(void)
{
}

int DCRYPTO_ladder_is_enabled(void)
{
	return 1;
}

static int test_migration(void)
{
	/*
	 * This purpose of this test is to verify migration of the 'legacy'
	 * TPM NVMEM format to the new scheme where each element is stored in
	 * flash in its own container.
	 */
	TEST_ASSERT(prepare_nvmem_contents() == EC_SUCCESS);
	TEST_ASSERT(nvmem_init() == EC_SUCCESS);
	TEST_ASSERT(iterate_over_flash() == EC_SUCCESS);
	TEST_ASSERT(test_result.var_count == 3);
	TEST_ASSERT(test_result.reserved_obj_count == 38);
	TEST_ASSERT(test_result.evictable_obj_count == 9);
	TEST_ASSERT(test_result.deleted_obj_count == 0);
	TEST_ASSERT(test_result.unexpected_count == 0);
	TEST_ASSERT(test_result.valid_data_size == 5454);
	TEST_ASSERT(total_var_space == 77);

	return EC_SUCCESS;
}

/*
 * The purpose of this test is to verify variable storage limits, both per
 * object and total.
 */
static int test_var_boundaries(void)
{
	const size_t max_size = 255; /* Key and value must fit in a byte. */
	const uint8_t *key;
	const uint8_t *val;
	size_t key_len;
	size_t val_len;
	uint16_t saved_total_var_space;
	uint32_t coverage_map;
	uint8_t var_key[10];

	TEST_ASSERT(prepare_new_flash() == EC_SUCCESS);
	saved_total_var_space = total_var_space;
	coverage_map = 0;

	/*
	 * Let's use the legacy NVMEM image as a source of fairly random but
	 * reproducible data.
	 */
	key = legacy_nvmem_image;
	val = legacy_nvmem_image;

	/*
	 * Test limit of max variable body space, use keys and values of
	 * different sizes, below and above the limit.
	 */
	for (key_len = 1; key_len < max_size; key_len += 20) {

		coverage_map |= 1;

		val_len = MIN(max_size, MAX_VAR_BODY_SPACE - key_len);
		TEST_ASSERT(setvar(key, key_len, val, val_len) == EC_SUCCESS);
		TEST_ASSERT(total_var_space ==
			    saved_total_var_space + key_len + val_len);

		/* Now drop the variable from the storage. */
		TEST_ASSERT(setvar(key, key_len, NULL, 0) == EC_SUCCESS);
		TEST_ASSERT(total_var_space == saved_total_var_space);

		/* And if key length allows it, try to write too much. */
		if (val_len == max_size)
			continue;

		coverage_map |= 2;
		/*
		 * Yes, let's try writing one byte too many and see that the
		 * attempt is rejected.
		 */
		val_len++;
		TEST_ASSERT(setvar(key, key_len, val, val_len) ==
			    EC_ERROR_INVAL);
		TEST_ASSERT(total_var_space == saved_total_var_space);
	}

	/*
	 * Test limit of max total variable space, use keys and values of
	 * different sizes, below and above the limit.
	 */
	key_len = sizeof(var_key);
	val_len = 20; /* Anything below 256 would work. */
	memset(var_key, 'x', key_len);

	while (1) {
		int rv;

		/*
		 * Change the key so that a new variable is added to the
		 * storage.
		 */
		rv = setvar(var_key, key_len, val, val_len);

		if (rv == EC_ERROR_OVERFLOW)
			break;

		coverage_map |= 4;
		TEST_ASSERT(rv == EC_SUCCESS);
		var_key[0]++;
		saved_total_var_space += key_len + val_len;
	}

	TEST_ASSERT(saved_total_var_space == total_var_space);
	TEST_ASSERT(saved_total_var_space <= MAX_VAR_TOTAL_SPACE);
	TEST_ASSERT((saved_total_var_space + key_len + val_len) >
		    MAX_VAR_TOTAL_SPACE);

	TEST_ASSERT(coverage_map == 7);
	return EC_SUCCESS;
}

void run_test(void)
{
	run_test_setup();

	RUN_TEST(test_migration);
	RUN_TEST(test_corrupt_nvmem);
	RUN_TEST(test_fully_erased_nvmem);
	RUN_TEST(test_configured_nvmem);
	RUN_TEST(test_nvmem_save);
	RUN_TEST(test_var_read_write_delete);
	RUN_TEST(test_nvmem_compaction);
	RUN_TEST(test_var_boundaries);
	RUN_TEST(test_nvmem_erase_tpm_data);

	/* more tests to come
	RUN_TEST(test_lock);
	RUN_TEST(test_malloc_blocking);
	*/

	test_print_result();
}
