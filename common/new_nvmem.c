/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <string.h>

#define NV_C
#include "Global.h"
#undef NV_C
#include "NV_fp.h"
#include "tpm_generated.h"

#include "common.h"
#include "board.h"
#include "console.h"
#include "crypto_api.h"
#include "flash.h"
#include "new_nvmem.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "shared_mem.h"
#include "timer.h"

/*
 * ==== Overview
 *
 * This file is the implementation of the new TPM NVMEM flash storage layer.
 * These are major differences compared to the legacy implementation:
 *
 * NVMEM objects are stored in separate containers, only changing objects are
 * updated when nvmem_commit() is invoked.
 *
 * The (key, value) pairs are also stored in separate containers. There is no
 * special area allocated to the (key, value) pairs in flash, they are
 * interleaved with TPM objects when stored in flash.
 *
 * The (key, value) pairs are not kept in the cache, they stored in flash
 * only. This causes a few deviations from the legacy (key, value) pair
 * interface:
 *
 *  - no need to initialize (key, value) storage separately, initvars() API is
 *    not there.
 *
 *  - when the user is retrieving a (key, value) object, he/she is given a
 *    dynamically allocated buffer, which needs to be explicitly released by
 *    calling the new API: freevar().
 *
 *  - the (key. value) pairs are stored in flash immediately, not after
 *    nvmem_commit() is called.
 *
 * Storing (key, value) pairs in the flash frees up 272 bytes of the cache
 * space previously used, but makes it more difficult to control how flash
 * memory is split between TPM objects and (key, value) pairs. A soft limit of
 * 1K is introduced, limiting the total space used by (key, value) pairs data,
 * not including tuple headers.
 *
 * ===== Organizing flash storage
 *
 * The total space used by the NVMEM in flash is reduced from 24 to 20
 * kilobytes, five 2K pages at the top of each flash bank. These pages are
 * concatenated into a single storage space, based on the page header placed
 * at the bottom of each page (struct nn_page_header). The page header
 * includes a 21 bit page number and a hash, this allows to order pages
 * properly on initialization.
 *
 * Yet another limitation of the new scheme is that no object stored in NVMEM
 * can exceed the flash page size less the page header size and container
 * header size. This allows for objects as large as 2035 bytes. Objects can
 * span flash pages. Note that reserved TPM object STATE_CLEAR_DATA exceeds 2K
 * in size, this is why one of its components (the .pcrSave field) is stored
 * separately.
 *
 * ===== Object containers
 *
 * The container header (struct nn_container) describes the contained TPM
 * object along with its size, and also includes the hash of the entire
 * container calculated when the hash field is set to zero. When an object
 * needs to be updated, it is stored in the end of the used flash space in a
 * container with the higher .generation field value, and then the older
 * container type field is erased, thus marking it as a deleted object. The
 * idea is that when initializing NVMEM cache after reset, in case two
 * instances of the same object are found in the flash because the new
 * instance was saved, but the old instance was erased because of some
 * failure, the instance with larger .generation field value wins.
 *
 * The container type field is duplicated in the container header, this allows
 * to verify the container hash after the object was erased. The need to be
 * able to erase the type field requires that the container starts at the 4
 * byte boundary, this in turn requires that each container is padded such
 * that total storage taken by the container is divisible by 4.
 *
 * To be able to tell if two containers contain two instances of the same
 * object one needs to be able to identify the object stored in the container.
 * For the three distinct types of objects it works as follows:
 *
 *    - (key, value) pair: key size and contents, stored in the contained
 *      tuple.
 *
 *    - reserved tpm object: the first byte stored in the container. PCR
 *      values from STATE_CLEAR_DATA.pcrSave field are stored as separate
 *      reserved objects with the appropriate first bytes.
 *
 *    - evictable tpm object: the first 4 bytes stored in the container, the
 *      evictable TTPM object ID.
 *
 * dont't forget that the contents are encrypted, decryption is needed each
 * time a stored object needs to be examined.
 *
 * Reserved objects of types STATE_CLEAR_DATA and STATE_RESET_DATA are very
 * big and are stored in the flash in marshaled form.
 *
 * ===== Storage compaction
 *
 * Adding changed values at the end of the flash space would inevitably cause
 * space overflow, unless something is done about it. This is where flash
 * compaction kicks in: as soon as there are just three free flash pages left
 * the stored objects are moved to the end of the space, which results in
 * earlier used pages being freed and added to the pool of available flash
 * pages.
 *
 * ===== Committing TPM changes
 *
 * When nvmem_commit() is invoked it is necessary to identify which TPM
 * objects in the cache have changed and require saving. Remember, that (key,
 * value) pairs are not held in the cache any more and are saved in the flash
 * immediately, so they do not have to be dealt with during commit.
 *
 * The commit procedure starts with iterating over th ecivtable objects space
 * in the NVMEM cache, storing in an array offsets of all evictable objects it
 * finds ther. Then it iterates over flash contents skipping over (key, value)
 * pairs.
 *
 * For each reserved object stored in flash it compares its stored value with
 * the value stored in the cache at known fixed location. If the value has
 * changed, a new instance is saved in flash.
 *
 * For each evictable object stored in flash it checks if that object is still
 * in the cache, using the previously collected array of offsets. If the
 * object is not in the cache, it must have been deleted by the TPM, the
 * function deletes it from the flash as well. If the object is in the cache,
 * its offset is removed from the array to indicate that the object has been
 * processed, and then if the object value has changed, the new instance is
 * added and the old instance erased. After this the only offsets left in the
 * array are offsets of new objects, not yet saved in the flash, they all get
 * saved.
 *
 * ===== Migration from legacy storage and reclaiming flash space
 *
 * To be able to migrate existing devices from the legacy storage format the
 * initialization code checks if a full 12K flash partition is still present,
 * and if so -copies its contents into the chache and invokes the migration
 * function. The function erases the alternative partition and creates a list
 * of 5 pages available for new format (remember, the flash footprint of the
 * new scheme is smaller, only 10K is available in each half).
 *
 * The (key, value) pairs and TPM objects are stored in the new format as
 * described, and then the legacy partition is erased and its pages are added
 * to the list of free pages. This approach would fail if the existing TPM
 * storage would exceed 10K, but this is extremely unlikely, especially since
 * the large reserved objects are stored by the new scheme in marshaled form,
 * this frees up a lot of flash space.
 *
 * Eventually it will be possible to reclaim the bottom 2K page per flash half
 * currently used by the legacy scheme, but this would be possible only after
 * migration is over. The plan is to keep a few Cr50 versions supporting the
 * migration process, and then drop the migration code and rearrange the
 * memory map and reclaim the freed pages. Chrome OS will always carry a
 * migrating capable Cr50 version along with the latest one to make sure that
 * even Chrome OS devices which had not updated their Cr50 code in a long
 * while can be migrated in two steps.
 *
 * ===== Initialization, including erased/corrupted flash
 *
 * On regular startup (no legacy partition found) the flash pages dedicated to
 * NVMEM storage are examined, pages with valid headers are included in the
 * list of available pages, sorted by the page number. Erased pages are kept
 * in a separate list, pages which are not fully erased, but do not have a
 * valid header are considered corrupted, and are erased and added to the
 * second list.
 *
 * After that the contents of the ordered flash pages is read, all discovered
 * TPM objects are verified and saved in the cache.
 *
 * If it is discovered that the flash is empty, a set of dummy reserved
 * objects is put into flash, this is necessary because the commit process
 * relies on comparing flash and cache contents when it decides if a reserved
 * object needs to be saved.
 *
 * ===== Handling failures
 *
 * This implementation is no better in handling failures than the legacy one,
 * and it is even worse, because if a failure happened during legacy commit
 * operation, at least the earlier saved partition would be available, if
 * failure happens during this implementation's save or compaction process,
 * there is a risk of ending up with a corrupted or inconsistent flash
 * contents.
 *
 * This first draft is offered for review and to facilitate testing and
 * discussion about how failures should be addressed.
 *
 * ===== Missing stuff
 *
 * . Presently not much thought has been given to locking and preventing
 *   problems caused by task preemption. The legacy scheme is still in place,
 *   but it might require improvements.
 *
 * . Transactions support described in the original design document are not
 *   yet supproted, this is something to consider, is it really necessary?
 */

/*
 * Container for storing (key, value) pairs, a.k.a. vars during read. Actual
 * vars would never be this large, but when looking for vars we need to be
 * able to iterate over and verify all objects in the flash, hence the max
 * body size.
 */
struct max_var_container {
	struct nn_container c_header;
	struct tuple t_header;
	uint8_t body[CONFIG_FLASH_BANK_SIZE -
		     sizeof(struct nn_container) - sizeof(struct tuple)];
} __packed;

/*
 * This array contains a list of flash pages indices (0..255 range) sorted by
 * the page header page number filed. Erased pages are kept at the tail of the
 * list.
 */
static uint8_t page_list[NEW_NVMEM_TOTAL_PAGES];

/*
 * Total space taken by key, value pairs in flash. It is limited to give TPM
 * objects priority.
 */
test_export_static uint16_t total_var_space;

/* The main context used when adding objects to NVMEM. */
test_export_static struct page_tracker master_pt;

test_export_static enum ec_error_list browse_flash_contents(int print);
static enum ec_error_list save_container(struct nn_container *nc);
static enum ec_error_list save_object(const struct nn_container *cont);

/*
 * This function allocates a buffer of the requested size.
 *
 * Heap space could be very limited and at times there could be not enough
 * memory in the heap to allocate. This should not be considered a failure,
 * polling should be used instead. On a properly functioning device the memory
 * would become available. If it is not - there is not much we can do, we'll
 * have to reboot adding a log entry when the log facility becomes available.
 */
static void *get_scratch_buffer(size_t size)
{
	char *buf;
	int i;

	/*
	 * Wait up to a 5 seconds in case some other operation is under
	 * way.
	 */
	for (i = 0; i < 50; i++) {
		int rv;

		rv = shared_mem_acquire(size, &buf);
		if (rv == EC_SUCCESS) {
			if (i)
				ccprintf("%s: waited %d cycles!\n",
					 __func__, i);
			return buf;
		}
		usleep(100 * MSEC);
	}

	/*
	 * Report failure on the console and in the log and then reboot, nvmem
	 * can't continue.
	 */
	ccprintf("%s: could not allocate memory!\n",  __func__);

	/* TODO: b/63760920 add entry to the log. */
	while (1)
		;
}

/* Convenience functions used to reduce amount of typecasting. */
static void app_compute_hash_wrapper(void *buf, size_t size,
				     void *hash, size_t hash_size)
{
	app_compute_hash(buf, size, hash, hash_size);
}

static STATE_CLEAR_DATA *get_scd(void)
{
	NV_RESERVED_ITEM ri;

	NvGetReserved(NV_STATE_CLEAR, &ri);

	return (STATE_CLEAR_DATA *)
		((uint8_t *)nvmem_cache_base(NVMEM_TPM) + ri.offset);

}

/* Veirify page header hash. */
static int page_header_is_valid(struct nn_page_header *ph)
{
	uint32_t ph_hash;

	app_compute_hash_wrapper(ph, offsetof(struct nn_page_header, page_hash),
				 &ph_hash, sizeof(ph_hash));

	return ph_hash == ph->page_hash;
}

/* Convert flash page number in 0..255 range into actual flash address. */
static struct nn_page_header *flash_index_to_ph(uint8_t index)
{
	return  (struct nn_page_header *)((index * CONFIG_FLASH_BANK_SIZE) +
					  CONFIG_PROGRAM_MEMORY_BASE);
}

/*
 * Return flash page pointed at by a certain page_list element if the page is
 * valid. If the index is out of range, or page is not initialized properly
 * return NULL.
 */
static struct nn_page_header *list_element_to_ph(size_t el)
{
	struct nn_page_header *ph;

	if (el >= ARRAY_SIZE(page_list))
		return NULL;

	ph = flash_index_to_ph(page_list[el]);

	if (page_header_is_valid(ph))
		return ph;

	return NULL;
}

/*
 * Read into buf or skip if buf is NULL the next num_bytes in the storage, at
 * the location determined by the passed in context.. Start from the very
 * beginning if the passed in context is empty.
 *
 * If necessary - concatenate contents from different pages bypassing page
 * headers.
 *
 * If user is reading the container header (as specified by the
 * container_fetch argument), save in the context the location of the
 * container.
 */
static size_t nvmem_read_bytes(struct page_tracker *pt, size_t num_bytes,
			       void *buf, int container_fetch)
{
	size_t togo;

	if (!pt->list_index && !pt->data_offset) {
		/* Start from the beginning. */
		pt->ph = list_element_to_ph(0);
		pt->data_offset = pt->ph->data_offset;
	}

	if (container_fetch) {
		pt->orig_data_offset = pt->data_offset;
		pt->orig_ph = pt->ph;
	}

	if ((pt->data_offset + num_bytes) < CONFIG_FLASH_BANK_SIZE) {
		/* All requested data fits in the current page. */
		if (buf)
			memcpy(buf, ((uint8_t *)pt->ph + pt->data_offset),
			       num_bytes);

		pt->data_offset += num_bytes;
		return num_bytes;
	}

	/* Data is split between pages. */
	/* To go in the current page. */
	togo = CONFIG_FLASH_BANK_SIZE - pt->data_offset;
	if (buf) {
		memcpy(buf, ((uint8_t *)pt->ph + pt->data_offset), togo);
		/* Next portion goes here. */
		buf = (uint8_t *)buf + togo;
	}

	/* To go in the next page. */
	togo = num_bytes - togo;

	/* Move to the next page. */
	pt->list_index++;
	pt->ph = list_element_to_ph(pt->list_index);

	if (!pt->ph && togo) {
		/*
		 * No more data to read. Could the end of used flash be close
		 * to the page boundary, so that there is no room to read an
		 * erased container header?
		 */
		if (!container_fetch) {
			ccprintf("%s: premature end of data (%d to go)!\n",
				 __func__, togo);
			return 0;
		}

		/*
		 * Simulate reading of the container header filled with all
		 * ones, which would be an indication of the end of storage,
		 * the caller will roll back ph, data_offset and list index as
		 * appropriate.
		 */
		memset(buf, 0xff, togo);
	} else if (pt->ph) {
		if (pt->ph->data_offset < (sizeof(*pt->ph) + togo)) {
			ccprintf("%s: size mismatch %d != %d!\n",
				 __func__,
				 pt->ph->data_offset, pt->data_offset);
			return 0; /* Something's screwed up, b/63760920. */
		}
		if (buf)
			memcpy(buf, pt->ph + 1, togo);

		pt->data_offset = sizeof(*pt->ph) + togo;
	}

	return num_bytes;
}

/*
 * Convert passed in absolute address into flash memory offset and write the
 * passed in blob into the flash.
 */
static enum ec_error_list write_to_flash(const void *flash_addr,
					 const void *obj, size_t size)
{
	return flash_physical_write((uintptr_t)flash_addr -
				    CONFIG_PROGRAM_MEMORY_BASE,
				    size, obj);
}

/*
 * When initializing flash for the first time - set the proper first page
 * header.
 */
static enum ec_error_list set_first_page_header(void)
{
	struct nn_page_header ph = {};
	enum ec_error_list rv;
	struct nn_page_header *fph; /* Address in flash. */

	ph.data_offset = sizeof(ph);
	app_compute_hash_wrapper(&ph,
				 offsetof(struct nn_page_header, page_hash),
				 &ph.page_hash, sizeof(ph.page_hash));

	fph = flash_index_to_ph(page_list[0]);
	rv = write_to_flash(fph, &ph, sizeof(ph));

	if (rv == EC_SUCCESS) {
		/* Make sure master page tracker is ready. */
		memset(&master_pt, 0, sizeof(master_pt));
		master_pt.data_offset = ph.data_offset;
		master_pt.ph = fph;
	}

	return rv;
}

/*
 * Verify that the passed in container is valid, specifically that its hash
 * matches its contents.
 */
static int container_is_valid(struct nn_container *ch)
{
	struct nn_container dummy_c;
	uint32_t hash;
	uint32_t preserved_hash;
	uint8_t preserved_type;

	preserved_hash = ch->container_hash;
	preserved_type = ch->container_type;

	ch->container_type = ch->container_type_copy;
	ch->container_hash = 0;
	app_compute_hash_wrapper(ch,
				 ch->size + sizeof(*ch),
				 &hash,
				 sizeof(hash));

	ch->container_hash = preserved_hash;
	ch->container_type = preserved_type;

	dummy_c.container_hash = hash;

	return dummy_c.container_hash == ch->container_hash;

}

static uint32_t aligned_container_size(const struct nn_container *ch)
{
	return (ch->size + sizeof(*ch) + 3) & ~3;
}

/*
 * Function which allows to iterate through all objects stored in flash. The
 * passed in context keeps track of where the previous object retrieval ended.
 *
 * Return:
 *  EC_SUCCESS                  if an object is retrieved and verified
 *  EC_ERROR_MEMORY_ALLOCATION  if 'erased' object reached (not an error).
 *  EC_ERROR_INVAL	        if verification failed or read is out of sync.
 */
enum ec_error_list get_next_object(struct page_tracker *pt,
				   struct nn_container *ch,
				   int include_deleted)
{
	uint32_t salt[4];

	salt[3] = 0;

	do {
		size_t aligned_remaining_size;

		if (nvmem_read_bytes(pt, sizeof(*ch), ch, 1) != sizeof(*ch)) {
			/* TODO: b/63760920 add entry to the log. */
			ccprintf("%s:%d\n", __func__, __LINE__);
			return EC_ERROR_INVAL;
		}

		/* Should we check for the container being all 0xff? */
		if (ch->container_type == NN_OBJ_ERASED) {
			/* Roll back container size. */
			pt->data_offset = pt->orig_data_offset;
			pt->ph = pt->orig_ph;

			/*
			 * If the container header happened to span between
			 * two pages - roll back page index saved in the
			 * context.
			 */
			if ((CONFIG_FLASH_BANK_SIZE - pt->data_offset) <
			    sizeof(struct nn_container))
				pt->list_index--;

			return EC_ERROR_MEMORY_ALLOCATION;
		}

		aligned_remaining_size = aligned_container_size(ch)
			- sizeof(*ch);

		if (aligned_remaining_size >
		    (CONFIG_FLASH_BANK_SIZE - sizeof(*ch))) {
			/* TODO: b/63760920 add entry to the log. */
			ccprintf("%s: inconsistent flash structure!\n",
				 __func__);
			return EC_ERROR_INVAL; /* Something is very wrong. */
		}

		nvmem_read_bytes(pt, aligned_remaining_size, ch + 1, 0);

		salt[0] = pt->orig_ph->page_number;
		salt[1] = pt->orig_data_offset;
		salt[2] = ch->container_hash;

		/* Decrypt in place. */
		app_cipher(salt, ch + 1, ch + 1, ch->size);

		/* And calculate hash. */
		if (!container_is_valid(ch)) {
			ccprintf("%s: container hash mismatch!\n", __func__);
			return EC_ERROR_INVAL;
		}
	} while (!include_deleted && (ch->container_type == NN_OBJ_OLD_COPY));

	return EC_SUCCESS;
}

/* Reshaffle flash contents dropping deleted objects. */
test_export_static enum ec_error_list compact_nvmem(void)
{
	const void *fence_ph;
	enum ec_error_list rv = EC_SUCCESS;
	size_t before;
	struct nn_container *ch;
	struct page_tracker pt = {};

	/* How much space was used before compaction. */
	before = master_pt.list_index * CONFIG_FLASH_BANK_SIZE +
		master_pt.data_offset;

	/* One page is enough even for the largest object. */
	ch = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);

	/*
	 * Page where we should stop compaction, all pages before this would
	 * be recycled.
	 */
	fence_ph = master_pt.ph;

	do {
		switch (get_next_object(&pt, ch, 1)) {
		case EC_SUCCESS:
			break;

		case EC_ERROR_MEMORY_ALLOCATION:
			return EC_SUCCESS;

		default: /* Error has been reported already. */
			return EC_ERROR_INVAL;
		}

		/* Re-store the object in compacted flash. */
		if (ch->container_type != NN_OBJ_OLD_COPY) {
			ch->generation++;
			if (save_container(ch) != EC_SUCCESS) {
				ccprintf("%s: Saving FAILED\n",  __func__);
				return EC_ERROR_INVAL;
			}
		}

		if (pt.list_index != 0) {
			/*
			 * We are done with a pre-compaction page, return it
			 * to the pool of empty pages and rearrange the list
			 * of pages.
			 */
			void *flash;
			uint8_t page_index = page_list[0];

			flash = flash_index_to_ph(page_index);
			flash_physical_erase((uintptr_t)flash -
				    CONFIG_PROGRAM_MEMORY_BASE,
				    CONFIG_FLASH_BANK_SIZE);
			memmove(page_list, page_list + 1,
				(ARRAY_SIZE(page_list) - 1) *
				sizeof(page_list[0]));
			page_list[ARRAY_SIZE(page_list) - 1] = page_index;
			pt.list_index--;
			master_pt.list_index--;
		}
	} while (pt.ph != fence_ph);

	shared_mem_release(ch);
	ccprintf("Compaction done, went from %d to %d bytes\n", before,
		 master_pt.list_index * CONFIG_FLASH_BANK_SIZE +
		 master_pt.data_offset);

	return rv;
}

/*
 * Save in the flash an object represented by the passed in container. Add new
 * pages to the list of used pages if necessary.
 */
static enum ec_error_list save_object(const struct nn_container *cont)
{
	const void *save_data = cont;
	size_t save_size = aligned_container_size(cont);
	size_t top_room;

	top_room = CONFIG_FLASH_BANK_SIZE - master_pt.data_offset;
	if (save_size >= top_room) {
		struct nn_page_header ph = {};

		/* Let's finish the current page. */
		write_to_flash((uint8_t *)master_pt.ph +
			       master_pt.data_offset, cont,
			       top_room);

		/* Remaining data and size to be written on the next page. */
		save_data = (const void *)((uintptr_t)save_data + top_room);
		save_size -= top_room;

		ph.data_offset = sizeof(ph) + save_size;
		ph.page_number = master_pt.ph->page_number + 1;
		app_compute_hash_wrapper(&ph,
					 offsetof(struct nn_page_header,
						  page_hash),
					 &ph.page_hash, sizeof(ph.page_hash));
		master_pt.list_index++;
		master_pt.ph = (const void *)
			(((uintptr_t)page_list[master_pt.list_index] *
			  CONFIG_FLASH_BANK_SIZE) + CONFIG_PROGRAM_MEMORY_BASE);

		write_to_flash(master_pt.ph, &ph, sizeof(ph));
		master_pt.data_offset = sizeof(ph);
	}

	if (save_size) {
		write_to_flash((uint8_t *)master_pt.ph +
			       master_pt.data_offset, save_data, save_size);
		master_pt.data_offset += save_size;
	}

	return EC_SUCCESS;
}

/*
 * Functions to check if the passed in blob is all zeros or all 0xff, in both
 * cases would be considered an uninitialized value. This is used when
 * marshaling certaing structures and PCRs.
 */
static int is_all_value(const uint8_t *p, size_t size, uint8_t value)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (p[i] != value)
			return 0;

	return 1;
}

test_export_static int is_uninitialized(const void *p, size_t size)
{
	return is_all_value(p, size, 0xff);
}

static int is_all_zero(const void *p, size_t size)
{
	return is_all_value(p, size, 0);
}

static int is_empty(const void *pcr_base, size_t pcr_size)
{
	return is_uninitialized(pcr_base, pcr_size) ||
		is_all_zero(pcr_base, pcr_size);
}

/*
 * A convenience function checking if the passed in blob is not empty, and if
 * so - save the blob in the destination memory prepended by the
 * 'reserved_index' value. This allows to store a PCR in a container.
 *
 * Return number of bytes placed in dst or zero, if the blob was empty.
 */
static size_t copy_pcr(const uint8_t *pcr_base,
		       size_t pcr_size,
		       uint8_t *dst,
		       uint8_t reserved_index)
{
	if (is_empty(pcr_base, pcr_size))
		return 0;  /* No need to save this. */

	*dst = reserved_index;
	memcpy(dst + 1, pcr_base, pcr_size);

	return pcr_size + 1;
}

/*
 * A convenience structure and array, allowing quick access to PCR banks
 * contained in the STATE_CLEAR_DATA:pcrSave field. This helps when
 * marshailing/unmarshaling PCR contents.
 */
struct pcr_descriptor {
	uint16_t pcr_array_offset;
	uint8_t pcr_size;
} __packed;

static const struct pcr_descriptor pcr_arrays[] = {
	{offsetof(PCR_SAVE, sha1), SHA1_DIGEST_SIZE},
	{offsetof(PCR_SAVE, sha256), SHA256_DIGEST_SIZE},
	{offsetof(PCR_SAVE, sha384), SHA384_DIGEST_SIZE},
	{offsetof(PCR_SAVE, sha512), SHA512_DIGEST_SIZE}
};
#define NUM_OF_PCRS (ARRAY_SIZE(pcr_arrays) * NUM_STATIC_PCR)

/*
 * Iterate over PCRs contained in the STATE_CLEAR_DATA structure in the NVMEM
 * cache and save nonempty ones in the flash.
 */
static void migrate_pcr(STATE_CLEAR_DATA *scd, size_t array_index,
			size_t pcr_index, struct nn_container *ch)
{
	const struct pcr_descriptor *pdsc;
	uint8_t *p_container_body;
	uint8_t *pcr_base;
	uint8_t reserved_index; /* Unique ID of this PCR in reserved storage. */

	p_container_body = (uint8_t *)(ch + 1);
	pdsc = pcr_arrays + array_index;
	pcr_base = (uint8_t *) &scd->pcrSave +
		pdsc->pcr_array_offset + pdsc->pcr_size * pcr_index;
	reserved_index = NV_VIRTUAL_RESERVE_LAST +
		array_index * NUM_STATIC_PCR + pcr_index;

	if (!copy_pcr(pcr_base, pdsc->pcr_size,
		      p_container_body + 1,
		      reserved_index))
		return;

	p_container_body[0] = reserved_index;
	ch->size = pdsc->pcr_size + 1;
	save_container(ch);
}

/*
 * Some NVMEM structures end up in the NVMEM cache with a wrong alignment. In
 * a passed in pointer is not aligned at a 4 byte boundary, this function will
 * save the 4 bytes above the blob in the passed in space and then move the
 * blob up so that it is properly aligned.
 */
static void *preserve_struct(void *p, size_t size, uint32_t *preserved)
{
	uint32_t misalignment = ((uintptr_t)p & 3);
	void *new_p;

	if (!misalignment)
		return p; /* Nothing to adjust. */

	memcpy(preserved, (uint8_t *)p + size, sizeof(*preserved));
	new_p = (void *)((((uintptr_t) p) + 3) & ~3);
	memmove(new_p, p, size);

	return new_p;
}

static void restore_struct(void *new_p, void *old_p,
			   size_t size, uint32_t *preserved)
{
	memmove(old_p, new_p, size);
	memcpy((uint8_t *)old_p + size, preserved, sizeof(*preserved));
}

/*
 * Note that PCRs are not marshaled here, but the rest of the structre, below
 * and above the PCR array is.
 */
static uint16_t marshal_state_clear(STATE_CLEAR_DATA *scd,
				    uint8_t *dst, int room)
{
	PCR_AUTHVALUE *new_pav;
	STATE_CLEAR_DATA *new_scd;
	size_t bottom_size;
	size_t i;
	size_t top_size;
	uint32_t preserved;
	uint8_t *base;

	bottom_size = offsetof(STATE_CLEAR_DATA, pcrSave);
	top_size = sizeof(scd->pcrAuthValues);

	if (is_empty(scd, bottom_size) &&
	    is_empty(&scd->pcrAuthValues, top_size))
		return 0;

	new_scd = preserve_struct(scd, bottom_size, &preserved);

	base = dst;

	*dst++ = (!!new_scd->shEnable) | ((!!new_scd->ehEnable) << 1) |
		((!!new_scd->phEnableNV) << 1);

	memcpy(dst, &new_scd->platformAlg, sizeof(new_scd->platformAlg));
	dst += sizeof(new_scd->platformAlg);

	room -= (dst - base);

	TPM2B_DIGEST_Marshal(&new_scd->platformPolicy, &dst, &room);

	TPM2B_AUTH_Marshal(&new_scd->platformAuth, &dst, &room);

	memcpy(dst, &new_scd->pcrSave.pcrCounter,
	       sizeof(new_scd->pcrSave.pcrCounter));
	dst += sizeof(new_scd->pcrSave.pcrCounter);
	room -= sizeof(new_scd->pcrSave.pcrCounter);

	if (new_scd != scd)
		restore_struct(new_scd, scd, bottom_size, &preserved);

	new_pav = preserve_struct(&scd->pcrAuthValues, top_size, &preserved);
	for (i = 0; i < ARRAY_SIZE(new_scd->pcrAuthValues.auth); i++)
		TPM2B_DIGEST_Marshal(new_pav->auth + i, &dst, &room);

	if (new_pav != (void *)&scd->pcrAuthValues)
		restore_struct(new_pav, &scd->pcrAuthValues,
			       top_size, &preserved);

	return dst - base;
}

static uint16_t marshal_state_reset_data(STATE_RESET_DATA *srd,
					 uint8_t *dst, int room)
{
	STATE_RESET_DATA *new_srd;
	uint32_t preserved;
	uint8_t *base;

	if (is_empty(srd, sizeof(*srd)))
		return 0;

	new_srd = preserve_struct(srd, sizeof(*srd), &preserved);

	base = dst;

	TPM2B_AUTH_Marshal(&new_srd->nullProof, &dst, &room);
	TPM2B_DIGEST_Marshal((TPM2B_DIGEST *)(&new_srd->nullSeed), &dst, &room);
	UINT32_Marshal(&new_srd->clearCount, &dst, &room);
	UINT64_Marshal(&new_srd->objectContextID, &dst, &room);

	memcpy(dst, new_srd->contextArray, sizeof(new_srd->contextArray));
	room -= sizeof(new_srd->contextArray);
	dst += sizeof(new_srd->contextArray);

	memcpy(dst, &new_srd->contextCounter, sizeof(new_srd->contextCounter));
	room -= sizeof(new_srd->contextCounter);
	dst += sizeof(new_srd->contextCounter);

	TPM2B_DIGEST_Marshal(&new_srd->commandAuditDigest, &dst, &room);
	UINT32_Marshal(&new_srd->restartCount, &dst, &room);
	UINT32_Marshal(&new_srd->pcrCounter, &dst, &room);

#ifdef TPM_ALG_ECC
	UINT64_Marshal(&new_srd->commitCounter, &dst, &room);
	TPM2B_NONCE_Marshal(&new_srd->commitNonce, &dst, &room);

	memcpy(dst, new_srd->commitArray, sizeof(new_srd->commitArray));
	room -= sizeof(new_srd->commitArray);
	dst += sizeof(new_srd->commitArray);
#endif

	if (new_srd != srd)
		restore_struct(new_srd, srd, sizeof(*srd), &preserved);

	return dst - base;
}

/*
 * Migrate all reserved objects found in the NVMEM cache after intializing
 * from legacy NVMEM storage.
 */
static enum ec_error_list migrate_tpm_reserved(struct nn_container *ch)
{
	STATE_CLEAR_DATA *scd;
	STATE_RESET_DATA *srd;
	size_t pcr_type_index;
	uint8_t *p_tpm_nvmem = nvmem_cache_base(NVMEM_TPM);
	uint8_t *p_container_body = (uint8_t *)(ch + 1);
	uint8_t index = 0;

	ch->container_type = ch->container_type_copy = NN_OBJ_TPM_RESERVED;

	while (1) {
		NV_RESERVED_ITEM ri;
		int copy_needed = 1;
		const size_t room = CONFIG_FLASH_BANK_SIZE -
			sizeof(struct nn_container) - 1;

		NvGetReserved(index, &ri);

		if (!ri.size)
			break;

		p_container_body[0] = index;

		switch (index++) {
		case NV_FIRMWARE_V1:
		case NV_FIRMWARE_V2:
			/* No need to store these in flash. */
			continue;

		case NV_STATE_CLEAR:
			scd = (STATE_CLEAR_DATA *)(p_tpm_nvmem + ri.offset);
			ri.size = marshal_state_clear(scd,
						      p_container_body + 1,
						      room);
			copy_needed = 0;
			break;

		case NV_STATE_RESET:
			srd = (STATE_RESET_DATA *)(p_tpm_nvmem + ri.offset);
			ri.size = marshal_state_reset_data(srd,
							   p_container_body + 1,
							   room);
			copy_needed = 0;
			break;
		}

		if (copy_needed) {
			/*
			 * Copy data into the stage area unless already done
			 * by marshaling function above.
			 */
			memcpy(p_container_body + 1,
			       p_tpm_nvmem + ri.offset,
			       ri.size);
		}

		ch->size = ri.size + 1;
		save_container(ch);
	}

	/*
	 * Now all components but the PCRs from STATE_CLEAR_DATA have been
	 * saved, let's deal with those PCR arrays. We want to save each PCR
	 * in a separate container, as if all PCRs are extended, the total
	 * combined size of the arrays would exceed flash page size. Also,
	 * PCRs are most likely to change one or very few at a time.
	 */
	for (pcr_type_index = 0;
	     pcr_type_index < ARRAY_SIZE(pcr_arrays);
	     pcr_type_index++) {
		size_t pcr_index;

		for (pcr_index = 0; pcr_index < NUM_STATIC_PCR; pcr_index++)
			migrate_pcr(scd, pcr_type_index, pcr_index, ch);

	}

	return EC_SUCCESS;
}

/*
 * Migrate all evictable objects found in the NVMEM cache after intializing
 * from legacy NVMEM storage.
 */
static enum ec_error_list migrate_objects(struct nn_container *ch)
{
	uint32_t next_obj_base;
	uint32_t obj_base;
	uint32_t obj_size;
	void *obj_addr;

	ch->container_type = ch->container_type_copy = NN_OBJ_TPM_OBJECT;

	obj_base = s_evictNvStart;
	obj_addr = nvmem_cache_base(NVMEM_TPM) + obj_base;
	memcpy(&next_obj_base, obj_addr, sizeof(next_obj_base));

	while (next_obj_base && (next_obj_base <= s_evictNvEnd)) {

		obj_size = next_obj_base - obj_base - sizeof(obj_size);
		memcpy(ch + 1,
		       (uint32_t *)obj_addr + 1,
		       obj_size);

		ch->size = obj_size;
		save_container(ch);

		obj_base = next_obj_base;
		obj_addr = nvmem_cache_base(NVMEM_TPM) + obj_base;

		memcpy(&next_obj_base, obj_addr, sizeof(next_obj_base));
	}

	return EC_SUCCESS;
}

static enum ec_error_list migrate_tpm_nvmem(struct nn_container *ch)
{
	/* Call this to initialize NVMEM indices. */
	NvEarlyStageFindHandle(0);

	migrate_tpm_reserved(ch);
	migrate_objects(ch);

	return EC_SUCCESS;
}

static enum ec_error_list save_var(const uint8_t *key, uint8_t key_len,
				   const uint8_t *val, uint8_t val_len,
				   struct max_var_container *vc)
{
	const int total_size = key_len + val_len +
		offsetof(struct max_var_container, body);
	enum ec_error_list rv;
	int local_alloc = !vc;

	if (local_alloc) {
		vc = get_scratch_buffer(total_size);
		vc->c_header.generation = 0;
	}

	/* Fill up tuple body. */
	vc->t_header.key_len = key_len;
	vc->t_header.val_len = val_len;
	memcpy(vc->body, key, key_len);
	memcpy(vc->body + key_len, val, val_len);

	/* Set up container header. */
	vc->c_header.container_type_copy =
		vc->c_header.container_type = NN_OBJ_TUPLE;
	vc->c_header.encrypted = 1;
	vc->c_header.size = sizeof(struct tuple) + val_len + key_len;

	rv = save_container(&vc->c_header);

	if (rv == EC_SUCCESS)
		total_var_space += key_len + val_len;

	if (local_alloc)
		shared_mem_release(vc);

	return rv;
}

/*
 * Migrate all (key, value) pairs found in the NVMEM cache after intializing
 * from legacy NVMEM storage.
 */
static enum ec_error_list migrate_vars(struct nn_container *ch)
{
	const struct tuple *var;

	/*
	 * During migration (key, value) pairs need to be manually copied from
	 * the NVMEM cache.
	 */
	set_local_copy();
	var = NULL;
	total_var_space = 0;

	while ((var = getnextvar(var)) != NULL)
		save_var(var->data_,
			 var->key_len,
			 var->data_ + var->key_len,
			 var->val_len,
			 (struct max_var_container *)ch);

	return EC_SUCCESS;
}

static int erase_partition(unsigned int act_partition, int erase_backup)
{
	enum ec_error_list rv;
	size_t flash_base;

	/*
	 * This is the first time we save using the new scheme, let's prepare
	 * the flash space. First determine which half is the backup now and
	 * erase it.
	 */
	flash_base = (act_partition ^ erase_backup) ?
		CONFIG_FLASH_NVMEM_BASE_A : CONFIG_FLASH_NVMEM_BASE_B;
	flash_base -= CONFIG_PROGRAM_MEMORY_BASE;

	rv = flash_physical_erase(flash_base, NVMEM_PARTITION_SIZE);

	if (rv != EC_SUCCESS) {
		ccprintf("%s: flash erase failed", __func__);
		return -rv;
	}

	return flash_base + CONFIG_FLASH_BANK_SIZE;
}

/*
 * This function is called once in a lifetime, when Cr50 boots up and a legacy
 * partition if found in the flash.
 */
enum ec_error_list new_nvmem_migrate(unsigned int act_partition)
{
	int flash_base;
	int i;
	int j;
	struct nn_container *ch;

	/*
	 * This is the first time we save using the new scheme, let's prepare
	 * the flash space. First determine which half is the backup now and
	 * erase it.
	 */
	flash_base = erase_partition(act_partition, 1);
	if (flash_base < 0) {
		ccprintf("%s: backup partition erase failed", __func__);
		return  -flash_base;
	}

	ch = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);

	/* Populate half of page_list with available page offsets. */
	for (i = 0; i < ARRAY_SIZE(page_list) / 2; i++)
		page_list[i] = flash_base/CONFIG_FLASH_BANK_SIZE + i;

	set_first_page_header();

	ch->encrypted = 1;
	ch->generation = 0;

	migrate_vars(ch);
	migrate_tpm_nvmem(ch);

	shared_mem_release(ch);

	if (browse_flash_contents(0) != EC_SUCCESS) {
		/* TODO: b/63760920 add entry to the log. */
		ccprintf("Migration failure!\n");
		return EC_ERROR_INVAL;
	}

	ccprintf("Migration success, used %d bytes of flash\n",
		 master_pt.list_index * CONFIG_FLASH_BANK_SIZE +
		 master_pt.data_offset);
	/*
	 * Now we can erase the active partition and add its flash to the pool.
	 */
	flash_base = erase_partition(act_partition, 0);
	if (flash_base < 0) {
		/* TODO: b/63760920 add entry to the log. */
		ccprintf("%s: main partition erase failed", __func__);
		return -flash_base;
	}

	/*
	 * Populate the second half of the page_list with pages retrieved from
	 * legacy partition.
	 */
	for (j = 0; j < ARRAY_SIZE(page_list) / 2; j++)
		page_list[i + j] = flash_base/CONFIG_FLASH_BANK_SIZE + j;

	return EC_SUCCESS;
}

/* Check if the passed in flash page is empty, if not - erase it. */
static void verify_empty_page(void *ph)
{
	uint32_t *word_p = ph;
	size_t i;

	for (i = 0; i < (CONFIG_FLASH_BANK_SIZE/sizeof(*word_p)); i++) {
		if (word_p[i] != (uint32_t)~0) {
			ccprintf("%s: corrupted page at %p!\n",
				 __func__, word_p);
			flash_physical_erase((uintptr_t)word_p -
				    CONFIG_PROGRAM_MEMORY_BASE,
				    CONFIG_FLASH_BANK_SIZE);
			break;
		}
	}
}

/*
 * At startup initialize the list of pages which contain NVMEM data and erased
 * pages. The list (in fact an array containing indices of the pages) is
 * sorted by the page number found in the page header. Pages which do not
 * contain valid page hader are checked to be erased and are placed at the
 * tail of the list.
 */
static void init_page_list(void)
{
	size_t i;
	size_t j;
	size_t page_list_index = 0;
	size_t tail_index;
	struct nn_page_header *ph;

	tail_index = ARRAY_SIZE(page_list);

	for (i = 0; i < ARRAY_SIZE(page_list); i++) {
		uint32_t page_index;

		/*
		 * This will yield indices of top pages first, first from the
		 * bottom half of the flash, and then from the top half. We
		 * know that flash is 512K in size, and pages are 2K in size,
		 * the indices will be in 123..127 and 251..255 range.
		 */
		if (i < (ARRAY_SIZE(page_list)/2)) {
			page_index = (CONFIG_FLASH_NEW_NVMEM_BASE_A -
				      CONFIG_PROGRAM_MEMORY_BASE)/
				CONFIG_FLASH_BANK_SIZE + i;
		} else {
			page_index = (CONFIG_FLASH_NEW_NVMEM_BASE_B -
				      CONFIG_PROGRAM_MEMORY_BASE)/
				CONFIG_FLASH_BANK_SIZE -
				ARRAY_SIZE(page_list)/2 + i;
		}

		ph = flash_index_to_ph(page_index);


		if (!page_header_is_valid(ph)) {
			/*
			 * this is not a valid page, let's plug it in into the
			 * tail of the list.
			 */
			page_list[--tail_index] = page_index;
			verify_empty_page(ph);
			continue;
		}

		/* This seems a valid page, let's put it in order. */
		for (j = 0; j < page_list_index; j++) {
			struct nn_page_header *prev_ph;

			prev_ph = list_element_to_ph(j);

			if (prev_ph->page_number > ph->page_number) {
				/* Need to move up. */
				memmove(page_list + j + 1, page_list + j,
					sizeof(page_list[0]) *
					(page_list_index - j));
				break;
			}
		}

		page_list[j] = page_index;
		page_list_index++;
	}

	if (!page_list_index) {
		ccprintf("Init nvmem from scratch\n");
		set_first_page_header();
	}
}

/*
 * The passed in pointer contains marshaled STATE_CLEAR structure as retrieved
 * from flash. This function unmarshals it and places in the NVMEM cache where
 * it belongs. Note that PCRs were not marshaled.
 */
static void unmarshal_state_clear(uint8_t *pad, int size, uint32_t offset)
{
	STATE_CLEAR_DATA *real_scd;
	STATE_CLEAR_DATA *scd;
	size_t i;
	uint32_t preserved;
	uint8_t booleans;

	real_scd = (STATE_CLEAR_DATA *)
		((uint8_t *)nvmem_cache_base(NVMEM_TPM) + offset);

	memset(real_scd, 0, sizeof(*real_scd));
	if (!size)
		return;

	memcpy(&preserved, real_scd + 1, sizeof(preserved));

	scd = (void *)(((uintptr_t)real_scd + 3) & ~3);

	/* Need proper unmarshal. */
	booleans = *pad++;
	scd->shEnable = !!(booleans & 1);
	scd->ehEnable = !!(booleans & (1 << 1));
	scd->phEnableNV = !!(booleans & (1 << 2));
	size--;

	memcpy(&scd->platformAlg, pad, sizeof(scd->platformAlg));
	pad += sizeof(scd->platformAlg);
	size -= sizeof(scd->platformAlg);

	TPM2B_DIGEST_Unmarshal(&scd->platformPolicy, &pad, &size);
	TPM2B_AUTH_Unmarshal(&scd->platformAuth, &pad, &size);

	memcpy(&scd->pcrSave.pcrCounter, pad, sizeof(scd->pcrSave.pcrCounter));
	pad += sizeof(scd->pcrSave.pcrCounter);
	size -= sizeof(scd->pcrSave.pcrCounter);

	for (i = 0; i < ARRAY_SIZE(scd->pcrAuthValues.auth); i++)
		TPM2B_DIGEST_Unmarshal(scd->pcrAuthValues.auth + i,
				       &pad, &size);

	memmove(real_scd, scd, sizeof(*scd));
	memcpy(real_scd + 1, &preserved, sizeof(preserved));
}

/*
 * The passed in pointer contains marshaled STATE_RESET structure as retrieved
 * from flash. This function unmarshals it and places in the NVMEM cache where
 * it belongs.
 */
static void unmarshal_state_reset(uint8_t *pad, int size, uint32_t offset)
{
	STATE_RESET_DATA *real_srd;
	STATE_RESET_DATA *srd;
	uint32_t preserved;

	real_srd = (STATE_RESET_DATA *)
		((uint8_t *)nvmem_cache_base(NVMEM_TPM) + offset);

	memset(real_srd, 0, sizeof(*real_srd));
	if (!size)
		return;

	memcpy(&preserved, real_srd + 1, sizeof(preserved));

	srd = (void *)(((uintptr_t)real_srd + 3) & ~3);

	TPM2B_AUTH_Unmarshal(&srd->nullProof, &pad, &size);
	TPM2B_DIGEST_Unmarshal((TPM2B_DIGEST *)(&srd->nullSeed), &pad, &size);
	UINT32_Unmarshal(&srd->clearCount, &pad, &size);
	UINT64_Marshal(&srd->objectContextID, &pad, &size);

	memcpy(srd->contextArray, pad, sizeof(srd->contextArray));
	size -= sizeof(srd->contextArray);
	pad += sizeof(srd->contextArray);

	memcpy(&srd->contextCounter, pad, sizeof(srd->contextCounter));
	size -= sizeof(srd->contextCounter);
	pad += sizeof(srd->contextCounter);

	TPM2B_DIGEST_Unmarshal(&srd->commandAuditDigest, &pad, &size);
	UINT32_Unmarshal(&srd->restartCount, &pad, &size);
	UINT32_Unmarshal(&srd->pcrCounter, &pad, &size);

#ifdef TPM_ALG_ECC
	UINT64_Unmarshal(&srd->commitCounter, &pad, &size);
	TPM2B_NONCE_Unmarshal(&srd->commitNonce, &pad, &size);

	memcpy(srd->commitArray, pad, sizeof(srd->commitArray));
	size -= sizeof(srd->commitArray);
#endif

	memmove(real_srd, srd, sizeof(*srd));
	memcpy(real_srd + 1, &preserved, sizeof(preserved));

}

/*
 * Based on the passed in index, find the location of the PCR in the NVMEM
 * cache and copy it there.
 */
static void restore_pcr(size_t pcr_index, uint8_t *pad, size_t size)
{
	const STATE_CLEAR_DATA *scd;
	const struct pcr_descriptor *pcrd;
	void *cached;  /* This PCR's position in the NVMEM cache. */

	if (pcr_index > NUM_OF_PCRS)
		return; /* This is an error. */

	pcrd = pcr_arrays + pcr_index / NUM_STATIC_PCR;
	if (pcrd->pcr_size != size)
		return; /* This is an error. */

	scd = get_scd();
	cached = (uint8_t *)&scd->pcrSave + pcrd->pcr_array_offset +
		pcrd->pcr_size * (pcr_index % NUM_STATIC_PCR);

	memcpy(cached, pad, size);
}

/* Restore a reserved object found in flash on initialization. */
static void restore_reserved(void *pad, size_t size)
{
	NV_RESERVED_ITEM ri;
	uint16_t type;
	void *cached;

	/*
	 * Index is saved as a single byte, update pad to point at the
	 * payload.
	 */
	type = *(uint8_t *)pad++;
	size--;

	if (type < NV_VIRTUAL_RESERVE_LAST) {
		NvGetReserved(type, &ri);

		switch (type) {
		case NV_STATE_CLEAR:
			unmarshal_state_clear(pad, size, ri.offset);
			break;

		case NV_STATE_RESET:
			unmarshal_state_reset(pad, size, ri.offset);
			break;

		default:
			cached = ((uint8_t *)nvmem_cache_base(NVMEM_TPM) +
				  ri.offset);
			memcpy(cached, pad, size);
			break;
		}
		return;
	}

	restore_pcr(type - NV_VIRTUAL_RESERVE_LAST, pad, size);
}

/* Restore an evictable object found in flash on initialization. */
static void restore_object(void *pad, size_t size)
{
	static uint32_t next_obj_base;
	uint8_t *dest;

	if (!next_obj_base)
		next_obj_base = s_evictNvStart;

	dest = ((uint8_t *)nvmem_cache_base(NVMEM_TPM) + next_obj_base);
	next_obj_base += size + sizeof(next_obj_base);
	memcpy(dest, &next_obj_base, sizeof(next_obj_base));

	dest += sizeof(next_obj_base);
	memcpy(dest, pad, size);
	dest += size;

	memset(dest, 0, sizeof(next_obj_base));
}

/*
 * At startup iterate over flash contents and move TPM objects into the
 * appropriate locations in the NVMEM cache.
 */
static void retrieve_nvmem_contents(void)
{
	int rv;
	struct max_var_container *vc;
	struct nn_container *nc;

	memset(&master_pt, 0, sizeof(master_pt));

	/* No saved object will exceed CONFIG_FLASH_BANK_SIZE in size. */
	nc = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);

	while ((rv = get_next_object(&master_pt, nc, 0)) == EC_SUCCESS) {
		switch (nc->container_type) {
		case NN_OBJ_TUPLE:
			vc = (struct max_var_container *)nc;
			total_var_space += vc->t_header.key_len +
				vc->t_header.val_len;
			break;  /* Keep tuples in flash. */
		case NN_OBJ_TPM_RESERVED:
			restore_reserved(nc + 1, nc->size);
			break;

		case NN_OBJ_TPM_OBJECT:
			restore_object(nc + 1, nc->size);
			break;
		default:
			break;
		}
	}
	shared_mem_release(nc);
}

/*
 * When starting from scratch (flash fully erased) seed NVMEM flash with
 * 'dummy' reserved objects, they will be replaced with real contents when
 * nvmem_save() is called.
 *
 * We need the dummys in the flash because they are expected to be there
 * during nvmem_save(), so that we can compare them with the cache contents.
 *
 * TODO(vbendeb): maybe just keep a map of discovered objects and add missing
 * ones instead...
 */
static enum ec_error_list populate_reserved(void)
{
	enum ec_error_list rv;
	int i;
	struct nn_container *ch;
	uint8_t *container_body;

	ch = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);
	memset(ch, 0, CONFIG_FLASH_BANK_SIZE);

	container_body = (uint8_t *)(ch + 1);

	ch->container_type = ch->container_type_copy = NN_OBJ_TPM_RESERVED;
	ch->encrypted = 1;
	rv = EC_SUCCESS;

	for (i = 0; i < NV_VIRTUAL_RESERVE_LAST; i++) {
		NV_RESERVED_ITEM ri;

		NvGetReserved(i, &ri);
		container_body[0] = i;

		switch (i) {
		case NV_FIRMWARE_V1:
		case NV_FIRMWARE_V2:
			/* No need to keep these in the flash. */
			continue;

			/*
			 * No need to save these on initialization from
			 * scratch, unmarshaling code will properly expand
			 * size of zero.
			 */
		case NV_STATE_CLEAR:
		case NV_STATE_RESET:
			ri.size = 0;
			break;

			/*
			 * This is used for Ram Index field, prepended by
			 * size. Set the size to minimum, the size of the size
			 * field.
			 */
		case NV_RESERVE_LAST:
			ri.size = sizeof(uint32_t);
			memset(container_body + 1, 0, ri.size);
			break;

		default:
			break;
		}

		ch->size = ri.size + 1;
		rv = save_container(ch);

		if (rv != EC_SUCCESS)
			break;

		/* Restore container body to all zeros. */
		memset(container_body + 1, 0, ri.size);
	}

	shared_mem_release(ch);

	return rv;
}

enum ec_error_list new_nvmem_init(void)
{
	enum ec_error_list rv;
	struct nn_page_header *ph;
	timestamp_t start, init, check;

	total_var_space = 0;

	/* Initialize NVMEM indices. */
	NvEarlyStageFindHandle(0);

	init_page_list();

	ph = flash_index_to_ph(page_list[0]);
	if ((ph->data_offset == sizeof(*ph)) &&
	    (((uint8_t *)(ph + 1))[0] == 0xff)) {
		/*
		 * This is initialization from scratch, populate reserved
		 * objects.
		 */
		rv = populate_reserved();
		if (rv != EC_SUCCESS)
			return rv;
	}

	start = get_time();

	check = get_time();
	memset(nvmem_cache_base(NVMEM_TPM), 0, nvmem_user_sizes[NVMEM_TPM]);

	retrieve_nvmem_contents();

	init = get_time();

	ccprintf("init took %d, check %d\n", (uint32_t)(check.val - start.val),
		 (uint32_t)(init.val - check.val));

	return EC_SUCCESS;
}

/*
 * Browse through the flash storage and save all evictable objects' offsets in
 * the passed in array. This is used to keep track of objects added or deleted
 * by the TPM library.
 */
static size_t init_object_offsets(uint16_t *offsets, size_t count)
{
	size_t num_objects = 0;
	uint32_t next_obj_base;
	uint32_t obj_base;
	void *obj_addr;

	obj_base = s_evictNvStart;
	obj_addr = (uint8_t *)nvmem_cache_base(NVMEM_TPM) + obj_base;
	memcpy(&next_obj_base, obj_addr, sizeof(next_obj_base));

	while (next_obj_base && (next_obj_base <= s_evictNvEnd)) {
		if (num_objects == count) {
			/* What do we do here?! */
			ccprintf("Too many objects!\n");
			break;
		}

		offsets[num_objects++] = obj_base - s_evictNvStart +
			sizeof(next_obj_base);

		obj_addr = nvmem_cache_base(NVMEM_TPM) + next_obj_base;
		obj_base = next_obj_base;
		memcpy(&next_obj_base, obj_addr, sizeof(next_obj_base));
	}

	return num_objects;
}

static enum ec_error_list delete_object(const struct page_tracker *pt,
					struct nn_container *ch)
{
	enum ec_error_list rv;
	void *flash_ch;

	flash_ch = (void *)((uintptr_t)pt->orig_ph + pt->orig_data_offset);

	if (memcmp(ch, flash_ch, sizeof(uint32_t))) {
		ccprintf("%s: pre-erase mismatch ptr %p, page %p, offset %d!\n",
			 __func__, flash_ch, pt->orig_ph, pt->orig_data_offset);
		return EC_ERROR_INVAL;
	}

	ch->container_type = NN_OBJ_OLD_COPY;

	rv = write_to_flash(flash_ch, ch, sizeof(uint32_t));

	if (memcmp(ch, flash_ch, sizeof(uint32_t)))
		ccprintf("%s: post-erase mismatch (%d)!\n", __func__, rv);

	return rv;
}

static enum ec_error_list update_object(const struct page_tracker *pt,
					struct nn_container *ch,
					void *cached_object,
					size_t new_size)
{
	size_t copy_size = new_size;
	size_t preserved_size;
	uint32_t preserved_hash;
	uint8_t *dst = (uint8_t *)(ch + 1);

	preserved_size = ch->size;
	preserved_hash = ch->container_hash;

	/*
	 * Need to copy data into the container, skip reserved type if it is a
	 * reserved object.
	 */
	if (ch->container_type == NN_OBJ_TPM_RESERVED) {
		dst++;
		copy_size--;
	}
	memcpy(dst, cached_object, copy_size);

	ch->generation++;
	ch->size = new_size;
	save_container(ch);

	ch->generation--;
	ch->size = preserved_size;
	ch->container_hash = preserved_hash;
	return delete_object(pt, ch);
}

static enum ec_error_list update_pcr(const struct page_tracker *pt,
				     struct nn_container *ch,
				     uint8_t index,
				     uint8_t *cached)
{
	uint8_t preserved;

	cached--;
	preserved = cached[0];
	cached[0] = index;
	update_object(pt, ch, cached, ch->size);
	cached[0] = preserved;

	return EC_SUCCESS;
}

static enum ec_error_list save_pcr(struct nn_container *ch,
				   uint8_t reserved_index,
				   const void *pcr,
				   size_t pcr_size)
{
	uint8_t *container_body;

	ch->container_type = ch->container_type_copy = NN_OBJ_TPM_RESERVED;
	ch->encrypted = 1;
	ch->size = pcr_size + 1;
	ch->generation = 0;

	container_body = (uint8_t *)(ch + 1);
	container_body[0] = reserved_index;
	memcpy(container_body + 1, pcr, pcr_size);

	return save_container(ch);
}

static enum ec_error_list maybe_save_pcr(struct nn_container *ch,
					 size_t pcr_index)
{
	const STATE_CLEAR_DATA *scd;
	const struct pcr_descriptor *pcrd;
	const void *cached;
	size_t pcr_size;

	pcrd = pcr_arrays + pcr_index / NUM_STATIC_PCR;
	scd = get_scd();

	pcr_size =  pcrd->pcr_size;

	cached = (const uint8_t *)&scd->pcrSave + pcrd->pcr_array_offset +
		pcr_size * (pcr_index % NUM_STATIC_PCR);

	if (is_empty(cached, pcr_size))
		return EC_SUCCESS;

	return save_pcr(ch, pcr_index + NV_VIRTUAL_RESERVE_LAST,
			cached, pcr_size);
}

/*
 * The process_XXX functions below are used to check and if necessary add,
 * update or delete objects from the flash based on the NVMEM cache
 * contents.
 */
static enum ec_error_list process_pcr(const struct page_tracker *pt,
				      struct nn_container *ch,
				      uint8_t index,
				      const uint8_t *saved,
				      uint8_t *pcr_bitmap)
{
	STATE_CLEAR_DATA *scd;
	const struct pcr_descriptor *pcrd;
	size_t pcr_bitmap_index;
	size_t pcr_index;
	size_t pcr_size;
	uint8_t *cached;

	pcr_bitmap_index = index - NV_VIRTUAL_RESERVE_LAST;

	if (pcr_bitmap_index > NUM_OF_PCRS)
		return EC_ERROR_INVAL; /* This is an error. */

	pcrd = pcr_arrays + pcr_bitmap_index / NUM_STATIC_PCR;
	pcr_index = pcr_bitmap_index % NUM_STATIC_PCR;

	pcr_size =  pcrd->pcr_size;

	if (pcr_size != (ch->size - 1))
		return EC_ERROR_INVAL; /* This is an error. */

	/* Find out base address of the cached PCR. */
	scd = get_scd();
	cached = (uint8_t *)&scd->pcrSave + pcrd->pcr_array_offset +
		pcr_size * pcr_index;
	/* Clear bitmap bit to indicate that this PCR was looked at. */
	pcr_bitmap[pcr_bitmap_index/8] &= ~(1 << (pcr_bitmap_index % 8));

	if (memcmp(saved, cached, pcr_size))
		return update_pcr(pt, ch, index, cached);

	return EC_SUCCESS;
}

static enum ec_error_list process_reserved(const struct page_tracker *pt,
					   struct nn_container *ch,
					   uint8_t *pcr_bitmap)
{
	NV_RESERVED_ITEM ri;
	size_t new_size;
	uint8_t *saved;
	uint8_t index;
	void *cached;

	/*
	 * Find out this object's location in the cache (first byte of the
	 * contents is the index of the reserved object.
	 */
	saved = (uint8_t *)(ch + 1);
	index = *saved++;

	NvGetReserved(index, &ri);

	if (ri.size) {
		void *marshaled;

		cached = (uint8_t *)nvmem_cache_base(NVMEM_TPM) + ri.offset;

		/*
		 * For NV_STATE_CLEAR and NV_STATE_RESET cases Let's marshal
		 * cached data to be able to compare it with saved data.
		 */
		if (index == NV_STATE_CLEAR) {
			marshaled = ((uint8_t *)(ch + 1)) + ch->size;
			new_size = marshal_state_clear
				(cached, marshaled, sizeof(STATE_CLEAR_DATA));
			cached = marshaled;
		} else if (index == NV_STATE_RESET) {
			marshaled = ((uint8_t *)(ch + 1)) + ch->size;
			new_size = marshal_state_reset_data
				(cached, marshaled, sizeof(STATE_RESET_DATA));
			cached = marshaled;
		} else {
			new_size = ri.size;
		}

		if ((new_size == (ch->size - 1)) &&
		    !memcmp(saved, cached, new_size))
			return EC_SUCCESS;

		return update_object(pt, ch, cached, new_size + 1);
	}

	/* This must be a PCR. */
	return process_pcr(pt, ch, index, saved, pcr_bitmap);
}

static enum ec_error_list process_object(const struct page_tracker *pt,
					 struct nn_container *ch,
					 uint16_t *tpm_object_offsets,
					 size_t *num_objects)
{
	size_t i;
	uint32_t cached_size;
	uint32_t cached_type;
	uint32_t flash_type;
	uint32_t next_obj_base;
	uint8_t *evict_start;
	void *pcache;

	evict_start = (uint8_t *)nvmem_cache_base(NVMEM_TPM) + s_evictNvStart;
	memcpy(&flash_type, ch + 1, sizeof(flash_type));
	for (i = 0; i < *num_objects; i++) {

		/* Find TPM object in the NVMEM cache. */
		pcache =  evict_start + tpm_object_offsets[i];
		memcpy(&cached_type, pcache, sizeof(cached_type));
		if (cached_type == flash_type)
			break;
	}

	if (i == *num_objects) {
		/*
		 * This object is not in the cache any more, delete it from
		 * flash.
		 */
		return delete_object(pt, ch);
	}

	memcpy(&next_obj_base,
	       (uint8_t *)pcache - sizeof(next_obj_base),
	       sizeof(next_obj_base));
	cached_size = next_obj_base - s_evictNvStart - tpm_object_offsets[i];
	if ((cached_size != ch->size) || memcmp(ch + 1, pcache, cached_size)) {
		/*
		 * Object changed. Let's delete the old copy and save the new
		 * one.
		 */
		update_object(pt, ch, pcache, ch->size);
	}

	tpm_object_offsets[i] = tpm_object_offsets[*num_objects - 1];
	*num_objects -= 1;

	return EC_SUCCESS;
}

static enum ec_error_list save_new_object(uint16_t obj_base, void *buf)
{
	size_t obj_size;
	struct nn_container *ch = buf;
	uint32_t next_obj_base;
	void *obj_addr;

	obj_addr = (uint8_t *)nvmem_cache_base(NVMEM_TPM) +
		obj_base + s_evictNvStart;
	memcpy(&next_obj_base,
	       obj_addr - sizeof(next_obj_base),
	       sizeof(next_obj_base));
	obj_size = next_obj_base - obj_base - s_evictNvStart;

	ch->container_type_copy = ch->container_type = NN_OBJ_TPM_OBJECT;
	ch->encrypted = 1;
	ch->size = obj_size;
	ch->generation = 0;
	memcpy(ch + 1, obj_addr, obj_size);

	return save_container(ch);
}

enum ec_error_list new_nvmem_save(void)
{
	const void *fence_ph;
	size_t i;
	size_t num_objs;
	struct nn_container *ch;
	struct page_tracker pt = {};
	uint16_t fence_offset;
	/* We don't foresee ever storing this many objects. */
	uint16_t tpm_object_offsets[20];
	uint8_t pcr_bitmap[(NUM_STATIC_PCR * ARRAY_SIZE(pcr_arrays) + 7)/8];

	/* See if compaction is needed. */
	if (master_pt.list_index >= (ARRAY_SIZE(page_list) - 3)) {
		enum ec_error_list rv;

		rv = compact_nvmem();
		if (rv != EC_SUCCESS)
			return rv;
	}

	fence_ph = master_pt.ph;
	fence_offset = master_pt.data_offset;

	num_objs = init_object_offsets(tpm_object_offsets,
				       ARRAY_SIZE(tpm_object_offsets));

	memset(pcr_bitmap, 0xff, sizeof(pcr_bitmap));
	ch = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);

	while ((fence_ph != pt.ph) || (fence_offset != pt.data_offset)) {
		int rv;

		rv = get_next_object(&pt, ch, 0);

		if (rv == EC_ERROR_MEMORY_ALLOCATION)
			break;

		if (rv != EC_SUCCESS) {
			ccprintf("%s: failed to read flash when saving (%d)!\n",
				 __func__, rv);
			shared_mem_release(ch);
			return rv;
		}

		if (ch->container_type == NN_OBJ_TPM_RESERVED) {
			process_reserved(&pt, ch, pcr_bitmap);
			continue;
		}

		if (ch->container_type == NN_OBJ_TPM_OBJECT) {
			process_object(&pt, ch,
				       tpm_object_offsets, &num_objs);
			continue;
		}

	}

	/* Now save new objects, if any. */
	for (i = 0; i < num_objs; i++)
		save_new_object(tpm_object_offsets[i], ch);

	/* And new pcrs, if any. */
	for (i = 0; i < NUM_OF_PCRS; i++) {
		if (!(pcr_bitmap[i / 8] & (1 << (i % 8))))
			continue;
		maybe_save_pcr(ch, i);
	}
	shared_mem_release(ch);

	return EC_SUCCESS;
}

/* Caller must free memory allocated by this function! */
static struct max_var_container *find_var(const uint8_t *key, size_t key_len,
					  struct page_tracker *pt)
{
	int rv;
	struct max_var_container *vc;

	vc = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);

	/*
	 * Let's iterate over all objects there are and look for matching
	 * tuples.
	 */
	while ((rv = get_next_object(pt, &vc->c_header, 1)) == EC_SUCCESS) {

		if (vc->c_header.container_type != NN_OBJ_TUPLE)
			continue;

		/* Verify consistency, first that the sizes match */
		if ((vc->t_header.key_len + vc->t_header.val_len +
		     sizeof(vc->t_header)) != vc->c_header.size) {
			ccprintf("%s: - inconsistent sizes!\n",	 __func__);
			/* report error here. */
			continue;
		}

		/* Ok, found a tuple, does the key match? */
		if ((key_len == vc->t_header.key_len) &&
		    !memcmp(key, vc->body, key_len))
			/* Yes, it does! */
			return vc;
	}

	shared_mem_release(vc);
	return NULL;
}

struct tuple *getvar(const uint8_t *key, uint8_t key_len)
{
	struct max_var_container *vc;
	struct page_tracker pt = {};

	if (!key || !key_len)
		return NULL;

	vc = find_var(key, key_len, &pt);

	if (vc)
		return &vc->t_header;

	return NULL;
}

int freevar(struct tuple *var)
{
	void *vc;

	vc = (uint8_t *)var - offsetof(struct max_var_container, t_header);
	shared_mem_release(vc);

	return EC_SUCCESS; /* Could verify var first before releasing. */
}

static enum ec_error_list invalidate_object(const struct nn_container *ch)
{
	struct nn_container c_copy;

	c_copy = *ch;
	c_copy.container_type = 0;

	return write_to_flash(ch, &c_copy, sizeof(uint32_t));
}

static enum ec_error_list save_container(struct nn_container *nc)
{
	uint32_t hash;
	uint32_t salt[4];

	nc->container_hash = 0;
	app_compute_hash_wrapper(nc, sizeof(*nc) + nc->size,
				 &hash, sizeof(hash));
	nc->container_hash = hash; /* This will truncate it. */

	salt[0] = master_pt.ph->page_number;
	salt[1] = master_pt.data_offset;
	salt[2] = nc->container_hash;
	salt[3] = 0;

	app_cipher(salt, nc + 1, nc + 1, nc->size);

	return save_object(nc);
}

int setvar(const uint8_t *key, uint8_t key_len,
	   const uint8_t *val, uint8_t val_len)
{
	enum ec_error_list rv;
	int erase_request;
	size_t new_var_space;
	size_t old_var_space;
	struct max_var_container *vc;
	struct page_tracker pt = {};

	if (!key || !key_len)
		return EC_ERROR_INVAL;

	new_var_space = key_len + val_len;

	if (new_var_space > MAX_VAR_BODY_SPACE)
		/* Too much space would be needed. */
		return EC_ERROR_INVAL;

	erase_request = !val || !val_len;

	/* See if compaction is needed. */
	if (!erase_request &&
	    (master_pt.list_index >= (ARRAY_SIZE(page_list) - 3))) {
		rv = compact_nvmem();
		if (rv != EC_SUCCESS)
			return rv;
	}

	vc = find_var(key, key_len, &pt);

	if (erase_request) {
		if (!vc)
			/* Nothing to erase. */
			return EC_SUCCESS;

		rv = invalidate_object((struct nn_container *)
				       ((uintptr_t)pt.orig_ph +
					pt.orig_data_offset));

		if (rv == EC_SUCCESS)
			total_var_space -= vc->t_header.key_len +
				vc->t_header.val_len;

		shared_mem_release(vc);
		return rv;
	}

	/* Is this variable already there? */
	if (!vc) {
		/* No, it is not. Will it fit? */
		if ((new_var_space + total_var_space) > MAX_VAR_TOTAL_SPACE)
			/* No, it will not. */
			return EC_ERROR_OVERFLOW;

		return save_var(key, key_len, val, val_len, vc);
	}

	/* The variable was found, let's see if the value is being changed. */
	if (vc->t_header.val_len == val_len &&
	    !memcmp(val, vc->body + key_len, val_len)) {
		shared_mem_release(vc);
		return EC_SUCCESS;
	}

	/* Ok, the variable was found, and is of a different value. */
	old_var_space = vc->t_header.val_len + vc->t_header.key_len;

	if ((old_var_space < new_var_space) &&
	    ((total_var_space + new_var_space - old_var_space) >
	     MAX_VAR_BODY_SPACE))
		return EC_ERROR_OVERFLOW;

	/* Save the new instance first with the larger generation number. */
	vc->c_header.generation++;
	rv = save_var(key, key_len, val, val_len, vc);
	shared_mem_release(vc);
	if (rv == EC_SUCCESS) {
		rv = invalidate_object((struct nn_container *)
				       ((uintptr_t)pt.orig_ph +
					pt.orig_data_offset));
		if (rv == EC_SUCCESS)
			total_var_space -= old_var_space;
	}
	return rv;
}

static void dump_contents(const struct nn_container *ch)
{
	const uint8_t *buf = (const void *)ch;
	size_t i;
	size_t total_size = sizeof(*ch) + ch->size;

	for (i = 0; i < total_size; i++) {
		if (!(i % 16)) {
			ccprintf("\n");
			cflush();
		}
		ccprintf(" %02x", buf[i]);
	}
	ccprintf("\n");
}

/*
 * Clear tpm data from nvmem. First fill up the current top page with erased
 * objects, then compact the flash sotrage, removing all TPM related objects.
 * This would garantee that all pages where TPM objecs were stored would be
 * erased.
 *
 * TODO(vbendeb): need to seed reserved objecst after this is done.
 */
int nvmem_erase_tpm_data(void)
{
	const uint8_t *key;
	const uint8_t *val;
	int rv;
	struct nn_container *ch;
	struct page_tracker pt = {};
	uint8_t saved_list_index;

	ch = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);

	while (get_next_object(&pt, ch, 1) == EC_SUCCESS) {

		if ((ch->container_type != NN_OBJ_TPM_RESERVED) &&
		    (ch->container_type != NN_OBJ_TPM_OBJECT))
			continue;

		delete_object(&pt, ch);
	}

	shared_mem_release(ch);

	/*
	 * Now fill up the current flash page with erased objects to make sure
	 * that it would be erased during next compaction. Use dummy key,
	 * value pairs as the erase objects.
	 */
	saved_list_index = master_pt.list_index;
	key = (const uint8_t *)nvmem_erase_tpm_data;
	val = (const uint8_t *)nvmem_erase_tpm_data;
	do {
		size_t to_go_in_page;
		const uint8_t key_len = MAX_VAR_BODY_SPACE - 255;
		uint8_t val_len;

		to_go_in_page = CONFIG_FLASH_BANK_SIZE - master_pt.data_offset;
		if (to_go_in_page > (MAX_VAR_BODY_SPACE +
				     offsetof(struct max_var_container,
					      body) - 1))
			val_len = MAX_VAR_BODY_SPACE - key_len;
		else
			val_len = to_go_in_page -
				offsetof(struct max_var_container,
					 body) - key_len + 1;

		if (setvar(key, key_len, val, val_len) != EC_SUCCESS)
			ccprintf("%s: adding var failed!\n", __func__);
		if (setvar(key, key_len, NULL, 0) != EC_SUCCESS)
			ccprintf("%s: deleting var failed!\n", __func__);

	} while (master_pt.list_index != (saved_list_index + 2));

	rv = compact_nvmem();
	return rv;
}

/*
 * Function which verifes flash contents integrity (and printing objects it
 * finds, if requested by the caller). All objects' active and deleted alike
 * integrity is verified by get_next_object().
 */
test_export_static enum ec_error_list browse_flash_contents(int print)
{
	int active = 0;
	int count = 0;
	int rv = EC_SUCCESS;
	size_t line_len = 0;
	struct nn_container *ch;
	struct page_tracker pt = {};

	ch = get_scratch_buffer(CONFIG_FLASH_BANK_SIZE);

	while ((rv = get_next_object(&pt, ch, 1)) == EC_SUCCESS) {

		count++;

		if (ch->container_type != NN_OBJ_OLD_COPY)
			active++;

		if (print) {
			char erased;

			if (ch->container_type == NN_OBJ_OLD_COPY)
				erased = 'x';
			else
				erased = ' ';

			if (ch->container_type_copy == NN_OBJ_TPM_RESERVED) {
				ccprintf("%cR:%02x.%d       ",
					 erased,
					 *((uint8_t *)(ch + 1)),
					 ch->generation);
			} else {
				uint32_t index;
				char tag;

				switch (ch->container_type_copy) {
				case NN_OBJ_TPM_OBJECT:
					tag = 'E'; /* evictable object. */
					break;

				case NN_OBJ_TUPLE:
					tag = 'T';
					break;

				default:
					tag = '?';
					break;
				}

				memcpy(&index, ch + 1, sizeof(index));
				ccprintf("%c%c:%08x.%d ", erased, tag,
					 index, ch->generation);
			}
			if (print > 1) {
				dump_contents(ch);
				continue;
			}

			if (line_len > 70) {
				ccprintf("\n");
				cflush();
				line_len = 0;
			} else {
				line_len += 11;
			}
		}
	}

	shared_mem_release(ch);

	if (rv == EC_ERROR_MEMORY_ALLOCATION) {
		ccprintf("%schecked %d objects, %d active\n", print ? "\n" : "",
			 count, active);
		rv = EC_SUCCESS;
	}

	return rv;
}

static int command_dump_nvmem(int argc, char **argv)
{
	nvmem_disable_commits();

	browse_flash_contents(1 + (argc > 1));

	nvmem_enable_commits();

	return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(dump_nvmem, command_dump_nvmem,
			     "",
			     "");
