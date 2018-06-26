/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "virtual_nvmem.h"

#include <string.h>

// TODO(louiscollard): It doesn't seem great to include Global.h here;  we could
// include "tpm_types.h" instead, but would need to replicate NV_INDEX structure
#include "Global.h"

#include "assert.h"
#include "board_id.h"
#include "link_defs.h"

/*
 * Functions to allow access to non-NVRam data through NVRam Indexes.
 *
 * These functions map virtual NV indexes to virtual offsets, and allow
 * reads from those virtual offsets. The functions are contrained based on the
 * implementation of the calling TPM functions; these constraints and other
 * assumptions are described below.
 *
 * The TPM NVRam functions make use of the available NVRam space to store NVRam
 * Indexes in a linked list with the following structure:
 *
 * struct nvram_list_node {
 *   UINT32 next_node_offset;
 *   TPM_HANDLE this_node_handle;
 *   NV_INDEX index;
 *   BYTE data[];
 * };
 *
 * The TPM functions for operating on NVRam begin by iterating through the list
 * to find the offset for the relevant Index.
 *
 * See NvFindHandle() in //third_party/tpm2/NV.c for more details.
 *
 * Once the offset has been found, read operations on the NV Index will
 * call _plat__NvMemoryRead() twice, first to read the NV_INDEX data, and
 * second to read the actual NV data.
 *
 * The offset x returned by NvFindHandle() is to the this_node_handle element of
 * the linked list node; the subsequent reads are therefore to
 * x+sizeof(TPM_HANDLE) and x+sizeof(TPM_HANDLE)+sizeof(NV_INDEX).
 *
 * The first read, to retrieve NV_INDEX data, is always a fixed size
 * (sizeof(NV_INDEX)). The size of the second read is user defined, but will
 * not exceed the size of the data.
 */

// Size constraints for virtual NV indexes.
#define VIRTUAL_NV_INDEX_HEADER_SIZE       sizeof(NV_INDEX)
#define MAX_VIRTUAL_NV_INDEX_DATA_SIZE     0x200
#define MAX_VIRTUAL_NV_INDEX_SLOT_SIZE     (sizeof(TPM_HANDLE) +	   \
					    VIRTUAL_NV_INDEX_HEADER_SIZE + \
					    MAX_VIRTUAL_NV_INDEX_DATA_SIZE)

// Prefix for virtual NV offsets. Chosen such that all virtual NV offsets are
// not valid memory addresses, to ensure it is impossibly to accidentally read
// (incorrect) virtual NV data from anywhere other than these functions.
#define VIRTUAL_NV_OFFSET_START            0xffff0000
#define VIRTUAL_NV_OFFSET_END		   0xffffffff

// These offsets are the two offsets queried by the TPM code, as a result of the
// design of that code, and the linked list structure described above.
#define NV_INDEX_READ_OFFSET               0x00000004 /* sizeof(uint32_t) */
#define NV_DATA_READ_OFFSET                0x00000098 // NV_INDEX_READ_OFFSET +
						      // sizeof(NV_INDEX)

// Template for the NV_INDEX data.
static const NV_INDEX nv_index_template = {
	.publicArea = {
		.nvIndex = 0 /* Placeholder */,
		.nameAlg = TPM_ALG_SHA256,
		.attributes = {
			// Allow index to be read using its authValue.
			.TPMA_NV_AUTHREAD = 1,
			// The spec requires at least one write
			// authentication method to be specified. We
			// intentionally don't include one, so that
			// this index cannot be spoofed by an
			// attacker running a version of cr50 that
			// pre-dates the implementation of virtual
			// NV indices.
			// .TPMA_NV_AUTHWRITE = 1,
			// Only allow deletion if the authPolicy is
			// satisied. The authPolicy is empty, and so
			// cannot be satisfied, so this effectively
			// disables deletion.
			.TPMA_NV_POLICY_DELETE = 1,
			// Prevent writes.
			.TPMA_NV_WRITELOCKED = 1,
			// Write-lock will not be cleared on startup.
			.TPMA_NV_WRITEDEFINE = 1,
			// Index has been written, can be read.
			.TPMA_NV_WRITTEN = 1,
		},
		.authPolicy = { },
		.dataSize = 0 /* Placeholder */,
	},
	.authValue = { },
};

// Currently supported virtual NV indexes.
//
// The range for virtual NV indexes is chosen such that all indexes
// fall within a range designated by the TCG for use by TPM manufacturers,
// without expectation of consultation with the TCG, or consistent behavior
// across TPM models. See Table 3 in the 'Registry of reserved TPM 2.0
// handles and localities' for more details.
//
// Entries in this enum must have a size and data function registered in a
// REGISTER_VIRTUAL_NV_INDEX_CONFIG statement below.
enum virtual_nv_index {
	VIRTUAL_NV_INDEX_START = 0x013f0000,
	VIRTUAL_NV_INDEX_BOARD_ID = VIRTUAL_NV_INDEX_START,
	VIRTUAL_NV_INDEX_SN_BITS,
	VIRTUAL_NV_INDEX_MAX,
};
// Reserved space for future virtual indexes.
#define VIRTUAL_NV_INDEX_END 0x013fffff
BUILD_ASSERT(VIRTUAL_NV_INDEX_MAX <= VIRTUAL_NV_INDEX_END);
// Check we don't overrun the virtual address space.
BUILD_ASSERT((VIRTUAL_NV_INDEX_MAX - VIRTUAL_NV_INDEX_START) *
	     MAX_VIRTUAL_NV_INDEX_SLOT_SIZE <
	     (VIRTUAL_NV_OFFSET_END - VIRTUAL_NV_OFFSET_START));

// Configuration of virtual NV indexes.
struct virtual_nv_index_cfg {
	enum virtual_nv_index index;
	uint16_t size;

	BYTE* (*get_data_fn)(void);
} __packed;

#define REGISTER_VIRTUAL_NV_INDEX_CONFIG(r_index, r_size, r_get_data_fn) \
	const struct virtual_nv_index_cfg __keep nv_index_cfg_ ## r_index \
	__attribute__((section(".rodata.virtualnvindexes"))) = \
	{ .index = r_index, .size = r_size, .get_data_fn = r_get_data_fn}

static BYTE *GetBoardId(void);
static BYTE *GetSnBits(void);

REGISTER_VIRTUAL_NV_INDEX_CONFIG(
	VIRTUAL_NV_INDEX_BOARD_ID, 12 /* data size */, GetBoardId);
REGISTER_VIRTUAL_NV_INDEX_CONFIG(
	VIRTUAL_NV_INDEX_SN_BITS, 10 /* data size */, GetSnBits);

//
// Helpers for dealing with NV indexes, associated configs and offsets.
//
////////////////////////////////////////////////////////////////////////////////

// Returns the config for the specified virtual NV index, or NULL if not found.
static const struct virtual_nv_index_cfg *GetNvIndexConfig(
	enum virtual_nv_index index)
{
	const struct virtual_nv_index_cfg *cur_p;
	const struct virtual_nv_index_cfg *end_p;

	cur_p = (const struct virtual_nv_index_cfg *)&__virtual_nv_indexes;
	end_p = (const struct virtual_nv_index_cfg *)&__virtual_nv_indexes_end;

	while (cur_p != end_p) {
		if (cur_p->index == index)
			return cur_p;
		cur_p++;
	}

	return NULL;
}

// Converts a virtual NV index to the corresponding virtual offset.
static inline BOOL NvIndexToNvOffset(uint32_t index)
{
	return VIRTUAL_NV_OFFSET_START +
		((index - VIRTUAL_NV_INDEX_START) *
		 MAX_VIRTUAL_NV_INDEX_SLOT_SIZE);
}

// Converts an virtual offset to the corresponding NV Index.
static inline BOOL NvOffsetToNvIndex(uint32_t offset)
{
	return VIRTUAL_NV_INDEX_START +
		((offset - VIRTUAL_NV_OFFSET_START) /
		 MAX_VIRTUAL_NV_INDEX_SLOT_SIZE);
}

// Copies the template NV_INDEX data to the specified destination, and updates
// it with the specified NV index and size values.
static inline void CopyNvIndex(void *dest, uint32_t nvIndex, uint32_t size)
{
	NV_INDEX *index;

	memcpy(dest, &nv_index_template, sizeof(NV_INDEX));
	index = (NV_INDEX *) dest;
	index->publicArea.nvIndex = nvIndex;
	index->publicArea.dataSize = size;
}

//
// Functions exposed to the TPM2 code.
//
////////////////////////////////////////////////////////////////////////////////

BOOL _plat__NvHandleInVirtualRange(uint32_t handle)
{
	return (handle & 0xffff0000) == VIRTUAL_NV_INDEX_START;
}

uint32_t _plat__NvGetHandleVirtualOffset(uint32_t handle)
{
	switch (handle) {
	case VIRTUAL_NV_INDEX_BOARD_ID:
	case VIRTUAL_NV_INDEX_SN_BITS:
		return NvIndexToNvOffset(handle);
	default:
		return 0;
	}
}

BOOL _plat__NvOffsetIsVirtual(unsigned int startOffset)
{
	return (startOffset & 0xffff0000) == VIRTUAL_NV_OFFSET_START;
}

void _plat__NvVirtualMemoryRead(unsigned int startOffset, unsigned int size,
				void *data)
{
	uint32_t nvIndex;
	const struct virtual_nv_index_cfg *nvIndexConfig;
	unsigned int offset;
	unsigned int copied;

	nvIndex = NvOffsetToNvIndex(startOffset);
	nvIndexConfig = GetNvIndexConfig(nvIndex);

	// It is a programming error for this to fail.
	assert(nvIndexConfig);

	// Calculate offset within this NV index.
	offset = startOffset - NvIndexToNvOffset(nvIndex);
	copied = 0;

	// The first 4 bytes are supposed to represent a pointer to the next
	// element in the NV index list; we are not doing that here, and this
	// area should not be queried.
	assert(offset >= NV_INDEX_READ_OFFSET);

	// Check the request is reasonable, given the size of this index.
	assert(offset + size <= NV_DATA_READ_OFFSET + nvIndexConfig->size);

	// Check if the read includes the header (NV_INDEX) area.
	if (offset < NV_DATA_READ_OFFSET) {
		// If the header is read, the entire header must be read in one
		// go. It is ok to read entire header, and additionally data
		// that follows the header.
		//
		// We could support copying parts of the header, but this is
		// not used by the TPM code, and would complicate logic for
		// replacing values in the NV_INDEX template after copying.
		assert(offset == NV_INDEX_READ_OFFSET);
		assert(size >= VIRTUAL_NV_INDEX_HEADER_SIZE);

		CopyNvIndex(data, nvIndex, nvIndexConfig->size);

		copied += VIRTUAL_NV_INDEX_HEADER_SIZE;
		offset += copied;
		size -= copied;
	}

	// We may or may not have copied the header at this point. Either way,
	// check if we need to copy any actual virtual NV data, and do so if
	// necessary.
	if (size > 0) {
		memcpy((BYTE *)data + copied,
		       nvIndexConfig->get_data_fn() +
		       offset - NV_DATA_READ_OFFSET,
		       size);
	}
}

//
// Helpers to fetch actual virtual NV data.
//
////////////////////////////////////////////////////////////////////////////////

static BYTE *GetBoardId()
{
	static struct board_id board_id_tmp;

	read_board_id(&board_id_tmp);
	return (BYTE *) &board_id_tmp;
}

static BYTE *GetSnBits()
{
	return "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad";
}
