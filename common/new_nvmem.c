#include <stdint.h>
#include <string.h>

#define NV_C
#include "Global.h"
#undef NV_C
#include "NV_fp.h"
#include "tpm_generated.h"

#include "common.h"
#include "board.h"
#include "crypto_api.h"
#include "flash.h"
#include "new_nvmem.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "shared_mem.h"
#include "console.h"

/*
 * Container for storing vars during read. Actual vars would never be this
 * large, but we need to be able to accommodate reads of all possible objects
 * stored in nvmem, hence the max body size.
 */
struct max_var_container {
	struct nn_container c_header;
	struct tuple t_header;
	uint8_t body[CONFIG_FLASH_BANK_SIZE - sizeof(struct nn_container) - sizeof(struct tuple)];
} __packed;

/* Helper structure to keep track of the page size. */
struct nn_stage {
	uint32_t cursor; /* Keep it at 4 bytes so that data is also 4 byte aligned. */
	uint8_t data[2 * CONFIG_FLASH_BANK_SIZE];
};
#define NEW_NVMEM_PARTITION_SIZE (NVMEM_PARTITION_SIZE - CONFIG_FLASH_BANK_SIZE)

union word_union {
	uint32_t  word;
	uint8_t  bytes[4];
};

/*
 * This array contains the list of flash pages in order they have been filled.
 * The 2k page index in the flash array is stored here
 */
static uint8_t page_list[2*NEW_NVMEM_PARTITION_SIZE/CONFIG_FLASH_BANK_SIZE];

/*
 * Total space taken by key, value pairs in flash. Is limited to give TPM
 * objects priority.
 */
test_export_static uint16_t total_var_space;

/* Keeps track of flash contents for adding new elements. */
static struct page_tracker master_pt;

test_export_static enum ec_error_list browse_flash_contents(int print);
static enum ec_error_list save_container(struct nn_container *nc);
static enum ec_error_list save_object(const struct nn_container *cont);

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

static int page_header_is_valid(struct nn_page_header *ph)
{
	uint32_t ph_hash;

	app_compute_hash_wrapper(ph, offsetof(struct nn_page_header, page_hash),
				 &ph_hash, sizeof(ph_hash));

	return ph_hash == ph->page_hash;
}


static struct nn_page_header *flash_index_to_ph(uint8_t index)
{
	return  (struct nn_page_header *)((index * CONFIG_FLASH_BANK_SIZE) +
					  CONFIG_PROGRAM_MEMORY_BASE);
}

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
 * Read or skip the next num_bytes in the storage. Read into buf or skip if
 * buf is NULL.
 */
static size_t nvmem_read_bytes(struct page_tracker *pt, size_t num_bytes,
			       void *buf, int container_fetch)
{
	if (!pt->list_index && !pt->data_offset) {
		/*
		 * Let's start from the beginning, we could not be here before
		 * nvmem is initialized.
		 */
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
	} else {
		size_t togo;

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
			 * No more data to read. Could the end of used flash
			 * be close to the page boundary, so that there is no
			 * room to read an erased container header
			 * contents?
			 */
			if (!container_fetch) {
				ccprintf("%s: premature end of data (%d to go)!\n",
					 __func__, togo);
				return 0;
			}
			/*
			 * Simulate reading of the container header filled
			 * with all ones, which would be an indication of the
			 * end of storage, the caller will roll back ph,
			 * data_offset and list index as appropriate.
			 */
			memset(buf, 0xff, togo);
		}

		if (pt->ph) {
			if (pt->ph->data_offset < sizeof(*pt->ph) + togo) {
				ccprintf("%s: size mismatch %d != %d!\n", __func__,
					 pt->ph->data_offset, pt->data_offset);

				cflush();
				return 0; /* Something's screwed up. */
			}
			if (buf)
				memcpy(buf, pt->ph + 1, togo);

			pt->data_offset = sizeof(*pt->ph) + togo;
		}
	}

	return num_bytes;
}

static size_t set_page_header(void *phv,
			      size_t tail_size,
			      void *tail_data)
{
	struct nn_page_header *ph = phv;

	/* This sets both the buffer and the cursor. */
	ph->page_number++;
	ph->data_offset = sizeof(*ph) + tail_size;
	app_compute_hash_wrapper(ph,
				 offsetof(struct nn_page_header, page_hash),
				 &ph->page_hash, sizeof(ph->page_hash));
	if (tail_data)
		memcpy(ph + 1, tail_data, tail_size);

	return ph->data_offset;
}

static enum ec_error_list save_stage(struct nn_stage *stage)
{
	int byte_offset;
	size_t remaining_data_size;

	/* Save current page. */
	byte_offset = ((int)page_list[master_pt.list_index]) *
		CONFIG_FLASH_BANK_SIZE;

	flash_physical_write(byte_offset, CONFIG_FLASH_BANK_SIZE, stage->data);

	/* Next time write into the next page. */
	master_pt.list_index++;
	if (page_list[master_pt.list_index] == 0)
		/*
		 * Not the entire list has been initialized yet, and there is
		 * no more pages in the list. We sure are in trouble.
		 */
		return EC_ERROR_OVERFLOW;

	/* Prepare new page header. */
	remaining_data_size = stage->cursor - CONFIG_FLASH_BANK_SIZE;
	stage->cursor = set_page_header(stage->data,
					remaining_data_size,
					stage->data +
					CONFIG_FLASH_BANK_SIZE);

	return EC_SUCCESS;
}

static enum ec_error_list write_to_flash(const void *flash_addr,
					 const void *obj, size_t size)
{
	return flash_physical_write((uintptr_t)flash_addr - CONFIG_PROGRAM_MEMORY_BASE,
			   size, obj);
}

static int container_is_valid(struct nn_container *ch)
{
	struct nn_container dummy_c;
	uint32_t preserved_hash;
	uint8_t preserved_type;
	uint32_t hash;

	preserved_hash = ch->container_hash;
	preserved_type = ch->container_type;

	ch->container_type = ch->container_type_copy;
	ch->container_hash = 0;
	app_compute_hash_wrapper(ch, ch->size + sizeof(*ch), &hash, sizeof(hash));

	ch->container_hash = preserved_hash;
	ch->container_type = preserved_type;

	dummy_c.container_hash = hash;

	return dummy_c.container_hash == ch->container_hash;

}

static uint32_t aligned_container_size(const struct nn_container *ch)
{
	return (ch->size + sizeof(*ch) + 3) & ~3;
}

static enum ec_error_list clear_up_to_the_fence(const struct page_tracker *pt)
{
	struct nn_container *ch;
	struct nn_container ch_copy;
	size_t offset;

	offset = pt->ph->data_offset;
	while (offset < pt->data_offset) {
		ch = (struct nn_container *)((uintptr_t)pt->ph + offset);
		if (ch->container_type != NN_OBJ_OLD_COPY) {
			memcpy(&ch_copy, ch, sizeof(ch_copy));
			ch_copy.container_type = NN_OBJ_OLD_COPY;
			write_to_flash(ch, &ch_copy, sizeof(uint32_t));
		}

		offset += aligned_container_size(&ch_copy);

	} ;

	return EC_SUCCESS;
}

/*
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
			ccprintf("%s:%d\n", __func__, __LINE__);
			return EC_ERROR_INVAL;
		}

		/* Should we check for all 0xff? */
		if (ch->container_type == NN_OBJ_ERASED) {
			/* Roll back container size. */
			pt->data_offset = pt->orig_data_offset;
			pt->ph = pt->orig_ph;
			if ((CONFIG_FLASH_BANK_SIZE - pt->data_offset) <
			    sizeof(struct nn_container))
				pt->list_index--;

			return EC_ERROR_MEMORY_ALLOCATION;
		}

		aligned_remaining_size = aligned_container_size(ch)
			- sizeof(*ch);

		if (aligned_remaining_size >
		    (CONFIG_FLASH_BANK_SIZE - sizeof(*ch))) {
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
	} while(!include_deleted && (ch->container_type == NN_OBJ_OLD_COPY));

	return EC_SUCCESS;
}

test_export_static enum ec_error_list compact_nvmem(void)
{
	struct page_tracker pt = {};
	const void *fence_ph;
	uint16_t fence_offset;
	struct nn_container *ch;
	size_t before = master_pt.list_index * CONFIG_FLASH_BANK_SIZE + master_pt.data_offset;
	enum ec_error_list rv = EC_SUCCESS;

	/* One page is enough even for the largest object. */
	shared_mem_acquire(CONFIG_FLASH_BANK_SIZE, (char **)&ch);

	fence_ph = master_pt.ph;
	fence_offset = master_pt.data_offset;

	do {
		switch(get_next_object(&pt, ch, 1)) {
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
	} while ((pt.ph != fence_ph) ||
		 (pt.data_offset != fence_offset));

	shared_mem_release(ch);

	if (rv == EC_SUCCESS) {
		clear_up_to_the_fence(&pt);

		ccprintf("Compaction done, went from %d to %d bytes\n", before,
			 master_pt.list_index * CONFIG_FLASH_BANK_SIZE + master_pt.data_offset);
	}
	return rv;
}

static enum ec_error_list save_object(const struct nn_container *cont)
{
	const void *save_data = cont;
	/* Size must be 4 bytes aligned. */
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
					 offsetof(struct nn_page_header, page_hash),
					 &ph.page_hash, sizeof(ph.page_hash));
		master_pt.list_index++;
		master_pt.ph = (const void *)(((uintptr_t)page_list[master_pt.list_index] *
					       CONFIG_FLASH_BANK_SIZE) +
					      CONFIG_PROGRAM_MEMORY_BASE);

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

static enum ec_error_list fill_container(struct nn_stage *stage,
					 enum nn_object_type object_type,
					 size_t object_size,
					 uint8_t object_generation)
{
	struct nn_container *pc;
	struct nn_page_header *ph;
	void *object;
	uint32_t hash;
	uint32_t salt[4];

	ph = (struct nn_page_header *)stage->data;
	pc = (struct nn_container *)(stage->data + stage->cursor);
	object = pc + 1; /* Body has been copied already into the stage. */

	memset(pc, 0, sizeof(*pc));

	pc->container_type_copy = pc->container_type = object_type;
	pc->encrypted = 1;
	pc->size = object_size;
	pc->generation = object_generation;

	/*
	 * Pre-encryption hash. Must cast object to non const pointer to
	 * comply with app_compute_hash_wrapper() prototype expectations.
	 */
	pc->container_hash = 0;
	app_compute_hash_wrapper(pc, object_size + sizeof(*pc), &hash, sizeof(hash));
	pc->container_hash = hash; /* Let compiler do the masking. */

	/* Now encrypt payload. */
	salt[0] = ph->page_number;
	salt[1] = stage->cursor;
	salt[2] = pc->container_hash;
	salt[3] = 0;

	/* Encrypt in place. */
	app_cipher(salt, object, object, object_size);

	stage->cursor += object_size + sizeof(struct nn_container);

	/* Pad the object to 32 bit boundary. */
	while (stage->cursor & 3)
		stage->data[stage->cursor++] = 0xff;

	if (stage->cursor >= CONFIG_FLASH_BANK_SIZE)
		return save_stage(stage);

	return EC_SUCCESS;
}

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
 * For each bank store the number of saved PCRs followed by tuples of (index,
 * contents).
 */
static size_t copy_pcr(const uint8_t *pcr_base,
		       size_t pcr_size,
		       uint8_t* dst,
		       uint8_t reserved_index)
{
	if (is_empty(pcr_base, pcr_size))
		return 0;  /* No need to save this. */

	*dst = reserved_index;
	memcpy(dst + 1, pcr_base, pcr_size);

	return pcr_size + 1;
}


struct pcr_descriptor {
	uint16_t pcr_array_offset;
	uint8_t pcr_size;
} __packed;

static const struct pcr_descriptor pcr_arrays [] = {
	{offsetof(PCR_SAVE, sha1), SHA1_DIGEST_SIZE},
	{offsetof(PCR_SAVE, sha256), SHA256_DIGEST_SIZE},
	{offsetof(PCR_SAVE, sha384), SHA384_DIGEST_SIZE},
	{offsetof(PCR_SAVE, sha512), SHA512_DIGEST_SIZE}
};
#define NUM_OF_PCRS (ARRAY_SIZE(pcr_arrays) * NUM_STATIC_PCR)

static void fill_pcr(STATE_CLEAR_DATA *scd, size_t array_index,
		     size_t pcr_index, struct nn_stage *stage)
{
	uint8_t reserved_index; /* Unique ID of this PCR in reserved storage. */
	uint8_t *pcr_base;
	const struct pcr_descriptor *pdsc;

	pdsc = pcr_arrays + array_index;
	pcr_base = (uint8_t *) &scd->pcrSave +
		pdsc->pcr_array_offset + pdsc->pcr_size * pcr_index;
	reserved_index = NV_VIRTUAL_RESERVE_LAST + array_index * NUM_STATIC_PCR + pcr_index;

	if (copy_pcr(pcr_base, pdsc->pcr_size,
		     stage->data + stage->cursor + sizeof(struct nn_container),
		     reserved_index)) {
		fill_container(stage, NN_OBJ_TPM_RESERVED,
			       pdsc->pcr_size + 1, 0);
		return;
	}
}

static uint32_t preserved_value;
static void *preserve_struct(void *p, size_t size)
{
	uint32_t misalignment = ((uintptr_t)p & 3);
	void *new_p;

	if (!misalignment)
		return p; /* Nothing to adjust. */

	memcpy(&preserved_value, (uint8_t *)p + size, sizeof(preserved_value));
	new_p = (void *)((((uintptr_t) p) + 3) & ~3);
	memmove(new_p, p, size);

	return new_p;
}

static void restore_struct(void *new_p, void *old_p, size_t size)
{
	memmove(old_p, new_p, size);
	memcpy((uint8_t *)old_p + size, &preserved_value, sizeof(preserved_value));
}

/* Note that PCRs are not marshaled here. */
static uint16_t marshal_state_clear(STATE_CLEAR_DATA *scd, uint8_t *dst, int room)
{
	PCR_AUTHVALUE *new_pav;
	STATE_CLEAR_DATA *new_scd;
	size_t bottom_size;
	size_t i;
	size_t top_size;
	uint8_t *base;

	bottom_size = offsetof(STATE_CLEAR_DATA, pcrSave);
	top_size = sizeof(scd->pcrAuthValues);

	if (is_empty(scd, bottom_size) &&
	    is_empty(&scd->pcrAuthValues, top_size))
		return 0;

	new_scd = preserve_struct(scd, bottom_size);

	base = dst;

	*dst++ = (!!new_scd->shEnable) | ((!!new_scd->ehEnable) << 1) |
		((!!new_scd->phEnableNV) << 1);

	memcpy(dst, &new_scd->platformAlg, sizeof(new_scd->platformAlg));
	dst += sizeof(new_scd->platformAlg);

	room -= (dst - base);

	TPM2B_DIGEST_Marshal(&new_scd->platformPolicy, &dst, &room);

	TPM2B_AUTH_Marshal(&new_scd->platformAuth, &dst, &room);

	memcpy(dst, &new_scd->pcrSave.pcrCounter, sizeof(new_scd->pcrSave.pcrCounter));
	dst += sizeof(new_scd->pcrSave.pcrCounter);
	room -= sizeof(new_scd->pcrSave.pcrCounter);

	if (new_scd != scd)
		restore_struct(new_scd, scd, bottom_size);

	new_pav = preserve_struct(&scd->pcrAuthValues, top_size);
	for (i = 0; i < ARRAY_SIZE(new_scd->pcrAuthValues.auth); i++)
		TPM2B_DIGEST_Marshal(new_pav->auth + i, &dst, &room);

	if (new_pav != (void *)&scd->pcrAuthValues)
		restore_struct(new_pav, &scd->pcrAuthValues, top_size);

	return dst - base;
}

static uint16_t marshal_state_reset_data(STATE_RESET_DATA *srd,
					 uint8_t *dst, int room)
{
	uint8_t *base;
	STATE_RESET_DATA *new_srd;

	if (is_empty(srd, sizeof(*srd)))
		return 0;

	new_srd = preserve_struct(srd, sizeof(*srd));

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
#endif //TPM_ALG_ECC

	if (new_srd != srd)
		restore_struct(new_srd, srd, sizeof(*srd));

	return dst - base;
}

static enum ec_error_list migrate_tpm_reserved(struct nn_stage *stage)
{
	uint8_t index = 0;
	uint8_t *p_tpm_nvmem = nvmem_cache_base(NVMEM_TPM);
	STATE_CLEAR_DATA *scd;
	STATE_RESET_DATA *srd;
	size_t pcr_type_index;

	while (1) {
		NV_RESERVED_ITEM ri;
		uint8_t *stage_p;
		int copy_needed = 1;

		NvGetReserved(index, &ri);

		if (!ri.size)
			break;

		/* Address where the next payload will go. */
		stage_p = stage->data + stage->cursor +
			sizeof(struct nn_container);

		switch(index) {
		case NV_FIRMWARE_V1:
		case NV_FIRMWARE_V2:
			/* No need to store these in flash. */
			index++;
			continue;

		case NV_STATE_CLEAR:
			scd = (STATE_CLEAR_DATA *)(p_tpm_nvmem + ri.offset);
			ri.size = marshal_state_clear(scd, stage_p + 1,
						      sizeof(stage->data) -
						      stage->cursor);
			copy_needed = 0;
			break;

		case NV_STATE_RESET:
			srd = (STATE_RESET_DATA *)(p_tpm_nvmem + ri.offset);
			ri.size = marshal_state_reset_data(srd, stage_p + 1,
							   sizeof(stage->data) -
							   stage->cursor);
			copy_needed = 0;
			break;
		}

		*stage_p++ = index;
		if (copy_needed) {
			/*
			 * Copy data into the stage area unless already done
			 * by marshaling function above.
			 */
			memcpy(stage_p, p_tpm_nvmem + ri.offset, ri.size);
		}

		fill_container(stage, NN_OBJ_TPM_RESERVED, ri.size + 1, 0);

		index++;
	}

	/*
	 * Now all components but the PCRs have been saved, let's deal with
	 * the PCR arrays. We want to save each PCR in a separate element, as
	 * if all PCRs are extended, the total combined size of the arrays
	 * would exceed flash page size, and PCRs are most likely to change
	 * one at a time.
	 */
	for (pcr_type_index = 0;
	     pcr_type_index < ARRAY_SIZE(pcr_arrays);
	     pcr_type_index++) {
		size_t pcr_index;

		for (pcr_index = 0; pcr_index < NUM_STATIC_PCR; pcr_index++)
			fill_pcr(scd, pcr_type_index, pcr_index, stage);

	}

	return EC_SUCCESS;
}

struct packed_4b {
	uint32_t v;
} __packed;

static enum ec_error_list migrate_objects(struct nn_stage *stage)
{
	uint32_t obj_base;
	uint32_t obj_size;
	uint32_t next_obj_base;
	void *obj_addr;

	obj_base = s_evictNvStart;
	obj_addr = nvmem_cache_base(NVMEM_TPM) + obj_base;

	memcpy(&next_obj_base, obj_addr, sizeof(next_obj_base));

	while (next_obj_base && (next_obj_base <= s_evictNvEnd)) {
		obj_size = next_obj_base - obj_base - sizeof(obj_size);
		ccprintf("%s: saving object %x of %d bytes\n", __func__,
			 ((struct packed_4b *)obj_addr)[1].v, obj_size); cflush();
		memcpy(stage->data + stage->cursor +
		       sizeof(struct nn_container),
		       (uint32_t *)obj_addr + 1,
		       obj_size);

		fill_container(stage, NN_OBJ_TPM_OBJECT, obj_size, 0);

		obj_addr = nvmem_cache_base(NVMEM_TPM) + next_obj_base;
		obj_base = next_obj_base;
		memcpy(&next_obj_base, obj_addr, sizeof(next_obj_base));
	}

	return EC_SUCCESS;
}

static enum ec_error_list migrate_tpm_nvmem(struct nn_stage *stage)
{
	int byte_offset;

	/* Call this to initialize NVMEM indices. */
	NvEarlyStageFindHandle(0);

	migrate_tpm_reserved(stage);
	migrate_objects(stage);

	/* Save current page. */
	byte_offset = ((int)page_list[master_pt.list_index]) *
		CONFIG_FLASH_BANK_SIZE;

	flash_physical_write(byte_offset, stage->cursor, stage->data);

	return EC_SUCCESS;
}

static enum ec_error_list migrate_vars(struct nn_stage *stage)
{
	const struct tuple *var;

	/*
	 * During migration (key, value) pairs need to be manually copied from
	 * the NVMEM cache.
	 */
	set_local_copy();
	var = getnextvar(NULL);
	total_var_space = 0;

	while(var) {
		size_t obj_size;
		void *dest;

		obj_size = var->key_len + var->val_len + sizeof(struct tuple);

		/*
		 * Drop large objects if any, there should not be any on
		 * 'normal' cromebooks. If someone ran private code creating
		 * those - it would have been wiped out when transitioning
		 * from dev to normal anyways.
		 */
		if (obj_size <= (MAX_VAR_BODY_SPACE + sizeof(struct tuple))) {
			/*
			 * Move contents of var into the stage leaving room
			 * for object header.
			 */
			dest = stage->data + stage->cursor +
				sizeof(struct nn_container);

			memcpy(dest, var, obj_size);

			fill_container(stage, NN_OBJ_TUPLE, obj_size, 0);
			total_var_space += obj_size - sizeof(struct tuple);
		}
		var = getnextvar(var);
	}

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
		CONFIG_FLASH_NVMEM_BASE_A : CONFIG_FLASH_NVMEM_BASE_B;;
	flash_base -= CONFIG_PROGRAM_MEMORY_BASE;

	rv = flash_physical_erase(flash_base, NVMEM_PARTITION_SIZE);

	if (rv != EC_SUCCESS) {
		ccprintf("%s: flash erase failed", __func__);
		return -rv;
	}

	return flash_base + CONFIG_FLASH_BANK_SIZE;
}

enum ec_error_list new_nvmem_migrate(unsigned int act_partition)
{
	int flash_base;
	int i, j;
	int rv;
	struct nn_stage *stage;

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

	rv = shared_mem_acquire(sizeof(struct nn_stage), (char **)&stage);
	if (rv != EC_SUCCESS) {
		ccprintf("%s: failed to allocate stage buffer\n", __LINE__);
		return rv;
	}

	/* Prepare the first page header. */
	memset(stage->data, 0, sizeof(struct nn_page_header));
	stage->cursor = set_page_header(stage->data, 0, NULL);

	/* Populate page_list with available page offsets. */
	for (i = 0;
	     i < (NEW_NVMEM_PARTITION_SIZE / CONFIG_FLASH_BANK_SIZE);
	     i++)
		page_list[i] = flash_base/CONFIG_FLASH_BANK_SIZE + i;

	/* Make sure master page tracker is ready. */
	memset(&master_pt, 0, sizeof(master_pt));

	migrate_vars(stage);
	migrate_tpm_nvmem(stage);

	/* Populate master_pt so that we could add more objects. */
	master_pt.data_offset = stage->cursor;
	master_pt.ph = flash_index_to_ph(page_list[master_pt.list_index]);

	shared_mem_release(stage);

	if (browse_flash_contents(0) != EC_SUCCESS) {
		ccprintf("Migration failure!\n");
		return EC_ERROR_INVAL;
	}

	ccprintf("Migration success, used %d bytes of flash\n",
		 master_pt.list_index * CONFIG_FLASH_BANK_SIZE + stage->cursor);
	/*
	 * Now we can erase the active partition and add its flash to the pool.
	 */
	flash_base = erase_partition(act_partition, 0);
	if (flash_base < 0) {
		ccprintf("%s: main partition erase failed", __func__);
		return -flash_base;
	}

	for (j = 0;
	     j < (NEW_NVMEM_PARTITION_SIZE / CONFIG_FLASH_BANK_SIZE);
	     j++)
		page_list[i + j] = flash_base/CONFIG_FLASH_BANK_SIZE + j;

	return EC_SUCCESS;
}

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

static void init_page_list(void)
{
	/* Let's check all pages in new flash format space. */
	size_t i, j;
	struct nn_page_header *ph;
	size_t tail_index;
	size_t page_list_index = 0;

	tail_index = ARRAY_SIZE(page_list);

	for (i = 0; i < ARRAY_SIZE(page_list); i++) {
		uint32_t page_index;

		if (i < (ARRAY_SIZE(page_list)/2)) {
			page_index = (CONFIG_FLASH_NEW_NVMEM_BASE_A - CONFIG_PROGRAM_MEMORY_BASE)/CONFIG_FLASH_BANK_SIZE + i;
		} else {
			page_index = (CONFIG_FLASH_NEW_NVMEM_BASE_B - CONFIG_PROGRAM_MEMORY_BASE)/CONFIG_FLASH_BANK_SIZE - ARRAY_SIZE(page_list)/2 + i;
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
					sizeof(page_list[0]) * (page_list_index - j));
				break;
			}
		}

		page_list[j] = page_index;
		page_list_index++;
	}

	if (!page_list_index) {
		struct nn_page_header ph;

		ccprintf("Init nvmem from scratch\n");

		set_page_header(&ph, 0, NULL);
		write_to_flash(flash_index_to_ph(page_list[0]), &ph, sizeof(ph));
	}
}

static void unmarshal_state_clear(uint8_t *pad, int size, uint32_t offset)
{
	STATE_CLEAR_DATA *scd;
	uint8_t booleans;
	size_t i;
	uint32_t preserved;
	STATE_CLEAR_DATA *real_scd;

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
		TPM2B_DIGEST_Unmarshal(scd->pcrAuthValues.auth + i, &pad, &size);

	memmove(real_scd, scd, sizeof(*scd));
	memcpy(real_scd + 1, &preserved, sizeof(preserved));
}

static void unmarshal_state_reset(uint8_t *pad, int size, uint32_t offset)
{
	STATE_RESET_DATA *srd;
	STATE_RESET_DATA *real_srd;
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
#endif //TPM_ALG_ECC

	memmove(real_srd, srd, sizeof(*srd));
	memcpy(real_srd + 1, &preserved, sizeof(preserved));

}

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

static void restore_reserved(void *pad, size_t size)
{
	uint16_t type;
	NV_RESERVED_ITEM ri;
	void *cached;

	/*
	 * Index is saved as a single byte, update pad to point at the
	 * payload.
	 */
	type = *(uint8_t *)pad++;
	size--;

	if (type < NV_VIRTUAL_RESERVE_LAST) {
		NvGetReserved(type, &ri);

		switch(type) {
		case NV_STATE_CLEAR:
			unmarshal_state_clear(pad, size, ri.offset);
			break;

		case NV_STATE_RESET:
			unmarshal_state_reset(pad, size, ri.offset);
			break;

		default:
			cached = (void *)((uint8_t *)
					  nvmem_cache_base(NVMEM_TPM) + ri.offset);

			memcpy(cached, pad, size);
			break;
		}
		return;
	}

	restore_pcr(type - NV_VIRTUAL_RESERVE_LAST, pad, size);
}

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

static void retrieve_nvmem_contents(void)
{
	int rv;
	struct nn_container *nc;
	struct max_var_container *vc;

	memset(&master_pt, 0, sizeof(master_pt));

	/* No saved object will exceed CONFIG_FLASH_BANK_SIZE in size. */
	rv = shared_mem_acquire(CONFIG_FLASH_BANK_SIZE, (char **)&nc);
	if (rv != EC_SUCCESS) {
		ccprintf("%s: failed to allocate decritpion pad\n",  __func__);
		return;
	}

	while ((rv = get_next_object(&master_pt, nc, 0)) == EC_SUCCESS) {
		switch (nc->container_type) {
		case NN_OBJ_TUPLE:
			vc = (struct max_var_container *)nc;
			total_var_space += vc->t_header.key_len + vc->t_header.val_len;
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

static enum ec_error_list populate_reserved(void)
{
	int i;
	struct nn_container *ch;
	uint8_t *container_body;
	enum ec_error_list rv;

	shared_mem_acquire(CONFIG_FLASH_BANK_SIZE, (char **)&ch);
	memset(ch, 0, CONFIG_FLASH_BANK_SIZE);

	container_body = (uint8_t *)(ch + 1);

	memset(&master_pt, 0, sizeof(master_pt));
	master_pt.ph = list_element_to_ph(0);
	master_pt.data_offset = master_pt.ph->data_offset;

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

#include "timer.h"
enum ec_error_list new_nvmem_init(void)
{
	timestamp_t start, init, check;
	enum ec_error_list rv;
	struct nn_page_header *ph;

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

static size_t init_object_offsets(uint16_t *offsets, size_t count)
{
	uint32_t obj_base;
	void *obj_addr;
	uint32_t next_obj_base;
	size_t num_objects = 0;

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
	void *flash_ch;
	enum ec_error_list rv;

	flash_ch = (void *)((uintptr_t)pt->orig_ph + pt->orig_data_offset);

	if (memcmp(ch, flash_ch, sizeof(uint32_t))) {
		ccprintf("%s: pre-erase mismatch pointer %p, page %p, offset %d!\n",
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
	size_t preserved_size;
	uint32_t preserved_hash;
	uint8_t *dst = (uint8_t *)(ch + 1);
	size_t copy_size = new_size;

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
	memcpy(container_body +1, pcr, pcr_size);

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

	return save_pcr(ch, pcr_index + NV_VIRTUAL_RESERVE_LAST, cached, pcr_size);
}

static enum ec_error_list process_pcr(const struct page_tracker *pt,
				      struct nn_container *ch,
				      uint8_t index,
				      const uint8_t *saved,
				      uint8_t *pcr_bitmap)
{
	size_t pcr_bitmap_index;
	const struct pcr_descriptor *pcrd;
	size_t pcr_index;
	size_t pcr_size;
	STATE_CLEAR_DATA *scd;
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
	uint8_t index;
	void *cached;
	uint8_t *saved;
	size_t new_size;

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
			new_size = marshal_state_clear(cached, marshaled,
						       sizeof(STATE_CLEAR_DATA));
			cached = marshaled;
		} else if (index == NV_STATE_RESET) {
			marshaled = ((uint8_t *)(ch + 1)) + ch->size;
			new_size = marshal_state_reset_data(cached, marshaled,
							    sizeof(STATE_RESET_DATA));
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
	uint32_t cached_size;
	uint32_t cached_type;
	uint32_t flash_type;
	uint32_t next_obj_base;
	uint8_t *evict_start;
	void *pcache;
        size_t i;

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
	void *obj_addr;
	uint32_t next_obj_base;
	size_t obj_size;
	struct nn_container *ch = buf;

	obj_addr = (uint8_t *)nvmem_cache_base(NVMEM_TPM) + obj_base + s_evictNvStart;
	memcpy(&next_obj_base, obj_addr - sizeof(next_obj_base), sizeof(next_obj_base));
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
	uint16_t tpm_object_offsets[20]; /* We don't foresee ever storing this many objects. */
	size_t i;
	struct nn_container *ch;
	struct page_tracker pt = {};
	size_t num_objs;
	uint8_t pcr_bitmap[(NUM_STATIC_PCR * ARRAY_SIZE(pcr_arrays) + 7)/8];
	const void *fence_ph;
	uint16_t fence_offset;

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

	shared_mem_acquire(CONFIG_FLASH_BANK_SIZE, (char **)&ch);

	while((fence_ph != pt.ph) || (fence_offset != pt.data_offset)) {
		int rv;

		rv = get_next_object(&pt, ch, 0);

		if (rv == EC_ERROR_MEMORY_ALLOCATION)
			break;

		if (rv != EC_SUCCESS) {
			ccprintf("%s: - failed to read flash when saving (%d)!\n",
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
		if (!(pcr_bitmap[i % 8] & (1 << (i / 8))))
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

	shared_mem_acquire(CONFIG_FLASH_BANK_SIZE, (char **)&vc);

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
	struct page_tracker pt = {};
	struct max_var_container *vc;

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
	uint32_t salt[4];
	uint32_t hash;

	nc->container_hash = 0;
	app_compute_hash_wrapper(nc, sizeof(*nc) + nc->size, &hash, sizeof(hash));
	nc->container_hash = hash; /* This will truncate it. */

	salt[0] = master_pt.ph->page_number;
	salt[1] = master_pt.data_offset;
	salt[2] = nc->container_hash;
	salt[3] = 0;

	app_cipher(salt, nc + 1, nc + 1, nc->size);

	return save_object(nc);
}

static enum ec_error_list save_var(const uint8_t *key, uint8_t key_len,
				   const uint8_t *val, uint8_t val_len,
				   struct max_var_container *vc)
{
	enum ec_error_list rv;
	const int total_size = key_len + val_len + offsetof(struct max_var_container, body);

	if (!vc) {
		if (shared_mem_acquire(total_size, (char **)&vc) != EC_SUCCESS) {
			ccprintf("%s: - allocation failed!\n", __func__);
			return EC_ERROR_INVAL;
		}
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

	if (vc)
		shared_mem_release(vc);

	return rv;
}

int setvar(const uint8_t *key, uint8_t key_len,
	   const uint8_t *val, uint8_t val_len)
{
	struct page_tracker pt = {};
	struct max_var_container *vc;
	enum ec_error_list rv;
	int erase_request;
	size_t new_var_space;
	size_t old_var_space;

	if (!key || !key_len)
		return EC_ERROR_INVAL;

	new_var_space = key_len + val_len;

	if (new_var_space > MAX_VAR_BODY_SPACE)
		/* Too much space would be needed. */
		return EC_ERROR_INVAL;

	erase_request = !val || !val_len;

	/* See if compaction is needed. */
	if (!erase_request && (master_pt.list_index >= (ARRAY_SIZE(page_list) - 3))) {
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
			total_var_space -= vc->t_header.key_len + vc->t_header.val_len;

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
	    ((total_var_space + new_var_space - old_var_space) > MAX_VAR_BODY_SPACE))
		return EC_ERROR_OVERFLOW;

	/* Save the new instance first with the larger generation number. */
	vc->c_header.generation++;
	rv = save_var(key, key_len, val, val_len, vc);
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
	size_t total_size = sizeof(*ch) + ch->size;
	const uint8_t *buf = (const void *)ch;
	size_t i;

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
 * Clear tpm data from nvmem.
 */
int nvmem_erase_tpm_data(void)
{
	struct page_tracker pt = {};
	struct nn_container *ch;
	uint8_t saved_list_index;
	const uint8_t *key;
	const uint8_t *val;
	int rv;

	shared_mem_acquire(CONFIG_FLASH_BANK_SIZE, (char **)&ch);

	while(get_next_object(&pt, ch, 1) == EC_SUCCESS) {

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
			val_len = to_go_in_page - offsetof(struct max_var_container,
							   body) - key_len + 1;

		if (setvar(key, key_len, val, val_len) != EC_SUCCESS)
			ccprintf("%s: adding var failed!\n", __func__);
		if (setvar(key, key_len, NULL, 0) != EC_SUCCESS)
			ccprintf("%s: deleting var failed!\n", __func__);

	} while (master_pt.list_index != (saved_list_index + 2));

	rv = compact_nvmem();
	return rv;
}

test_export_static enum ec_error_list browse_flash_contents(int print)
{
	struct page_tracker pt = {};
	struct nn_container *ch;
	int rv = EC_SUCCESS;
	int count = 0;
	int active = 0;
	size_t line_len = 0;

	shared_mem_acquire(CONFIG_FLASH_BANK_SIZE, (char **)&ch);

	while((rv = get_next_object(&pt, ch, 1)) == EC_SUCCESS) {

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
				ccprintf("%cR:%02x       ",
					 erased, *((uint8_t *)(ch + 1)));
			} else {
				uint32_t index;
				char tag;

				switch(ch->container_type_copy) {
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
				ccprintf("%c%c:%08x ", erased, tag, index);
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
				line_len += 9;
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
