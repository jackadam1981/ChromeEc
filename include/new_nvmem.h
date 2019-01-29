#ifndef __TPM2_NVMEM_TEST_NEW_NVMEM_H
#define __TPM2_NVMEM_TEST_NEW_NVMEM_H

#include "common.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "util.h"

#define NVMEM_NOT_INITIALIZED ((unsigned int)-1)

/*
 * A totally arbitrary byte limit for space occupied by key, value pairs in
 * the flash.
 */
#define MAX_VAR_TOTAL_SPACE        1000

/*
 * Let's be reasonable: we're unlikely to have keys longer than 40 or so
 * bytes, and leave full 255 bytes for the value, shared space not to exceed
 * the value below.
 */
#define MAX_VAR_BODY_SPACE  300

enum nn_object_type {
	NN_OBJ_OLD_COPY = 0,
	NN_OBJ_TUPLE = 1,
	NN_OBJ_TPM_RESERVED = 2,
	NN_OBJ_TPM_OBJECT = 3,
	NN_OBJ_TRANSACTION_DEL = 4,
	NN_OBJ_ESCAPE = 5,
	NN_OBJ_ERASED = 7,
} __packed;

struct nn_page_header {
	unsigned int page_number: 21;
	unsigned int data_offset: 11;
	uint32_t     page_hash;
} __packed;

#define NV_VIRTUAL_RESERVE_LAST (NV_RESERVE_LAST + 2)

struct nn_container {
	/*
	 * Make sure container type is the first field in this structure, a
	 * lot depends on it!.
	 */
	unsigned int container_type :3;
	unsigned int container_type_copy :3;
	unsigned int encrypted: 1;
	unsigned int size: 12;
	unsigned int generation: 2;
	unsigned int container_hash: 19;
} __packed;

/* Helper structure to keep track of accesses to the flash storage. */
struct page_tracker {
	uint8_t list_index;
	uint16_t data_offset;
	uint16_t orig_data_offset;
	const struct nn_page_header *ph;
	const struct nn_page_header *orig_ph;
};

enum ec_error_list new_nvmem_init(void);
enum ec_error_list new_nvmem_migrate(unsigned int nvmem_act_partition);
enum ec_error_list new_nvmem_save(void);

enum ec_error_list get_next_object(struct page_tracker *pt,
				   struct nn_container *ch,
				   int include_deleted);

#if defined(TEST_BUILD) && !defined(TEST_FUZZ)
enum ec_error_list browse_flash_contents(int);
enum ec_error_list compact_nvmem(void);
extern uint16_t total_var_space;
int is_uninitialized(const void *p, size_t size);
#endif

/*
 * Clear tpm data from nvmem.
 */
int nvmem_erase_tpm_data(void);

#endif  /* ! __TPM2_NVMEM_TEST_NEW_NVMEM_H */
