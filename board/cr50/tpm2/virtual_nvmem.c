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
 *   NV_INDEX index;
 *   BYTE data[];
 * };
 *
 * The NV_INDEX structure contains the numeric NV Index. The TPM functions
 * for operating on NVRam begin by iterating through the list to find the
 * offset for the relevant Index.
 *
 * See NvFindHandle() in //third_party/tpm2/NV.c for more details.
 *
 * Once the offset has been found, read operations on the NV Index will
 * call _plat__NvMemoryRead() twice, first to read the NV_INDEX data, and
 * second to read the actual NV data.
 *
 * The offset x returned by NvFindHandle() is to the beginning of the linked
 * list node; the subsequent reads are therefore to x+sizeof(UINT32) and
 * x+sizeof(UINT32)+sizeof(NV_INDEX).
 *
 * The first read, to retrieve NV_INDEX data, is always a fixed size
 * (sizeof(NV_INDEX)). The size of the second read is user defined, but will
 * not exceed the size of the data.
 *
 * TODO(louiscollard): Check how strict the size checks are.
 */

// Size constraints for virtual NV indexes.
#define VIRTUAL_NV_INDEX_HEADER_SIZE       sizeof(NV_INDEX)
#define MAX_VIRTUAL_NV_INDEX_DATA_SIZE     0x200
#define MAX_VIRTUAL_NV_INDEX_SLOT_SIZE     (sizeof(uint32_t) +		   \
					    VIRTUAL_NV_INDEX_HEADER_SIZE + \
					    MAX_VIRTUAL_NV_INDEX_DATA_SIZE)


// The size of the data available for each virtual NV index.
#define BOARD_ID_DATA_SIZE                 12
#define SERIAL_NUMBER_DATA_SIZE            10

// Prefix for virtual NV offsets. Chosen such that all virtual NV offsets are
// not valid memory addresses, to ensure it is impossibly to accidentally read
// (incorrect) virtual NV data from anywhere other than these functions.
//
// TODO(louiscollard): Check this actually is an invalid memory address.
#define VIRTUAL_NV_OFFSET_START            0xffff0000

// These offsets are the two offsets queried by the TPM code, as a result of the
// design of that code, and the linked list structure described above.
#define NV_INDEX_READ_OFFSET               0x00000004 /* sizeof(uint32_t) */
#define NV_DATA_READ_OFFSET                0x00000098 // NV_INDEX_READ_OFFSET +
						      // sizeof(NV_INDEX)

// Currently supported virtual NV indexes.
//
// The range for virtual NV indexes is chosen such that all indexes
// fall within a range designated by the TCG for use by TPM manufacturers,
// without expectation of consultation with the TCG, or consistent behavior
// across TPM models. See Table 3 in the 'Registry of reserved TPM 2.0
// handles and localities' for more details.
//
// TODO(louiscollard): Check it's really ok to use this range; some other
// values within the TPM manufacturer range appear to be in use already,
// eg 0x01001007.
enum {
	VIRTUAL_NV_INDEX_START = 0x013f0000,
	VIRTUAL_NV_INDEX_BOARD_ID = VIRTUAL_NV_INDEX_START,
	VIRTUAL_NV_INDEX_SERIAL_NUMBER,
	VIRTUAL_NV_INDEX_MAX,
};

// Template for the NV_INDEX data
static const NV_INDEX nv_index_template = {
	.publicArea = {
		.nvIndex = 0 /* Placeholder */,
		.nameAlg = TPM_ALG_SHA256,
		.attributes = {
			.TPMA_NV_AUTHREAD = 1,
			// TODO(louiscollard): Check NV_WRITELOCKED is respected
			.TPMA_NV_WRITELOCKED = 1,
			.TPMA_NV_WRITTEN = 1,
			// TODO(louiscollard): Maybe set attributes to disallow
			// NV_UndefineSpace and NV_ReadLock.
			// TODO(louiscollard): Find and add any other attributes
			// that should be set.
		},
		.authPolicy = { },
		.dataSize = 0 /* Placeholder */,
	},
	.authValue = { },
};

// Used to store result of reading board_id, before returning to caller.
static struct board_id board_id_tmp;

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

// Returns the data size for the specified NV index.
static inline uint16_t GetNvDataSize(uint32_t nvIndex)
{
	// TODO(louiscollard): Do something nicer than this switch.
	switch (nvIndex) {
	case VIRTUAL_NV_INDEX_BOARD_ID:
		return BOARD_ID_DATA_SIZE;
	case VIRTUAL_NV_INDEX_SERIAL_NUMBER:
		return SERIAL_NUMBER_DATA_SIZE;
	default:
		// TODO(louiscollard): Do something more terminal?
	return 0;
	}
}

// Returns a pointer to the data for the specified NV index.
static inline void *GetNvData(uint32_t nvIndex)
{
	// TODO(louiscollard): Do something nicer than this switch.
	switch (nvIndex) {
	case VIRTUAL_NV_INDEX_BOARD_ID:
		read_board_id(&board_id_tmp);
		return &board_id_tmp;
	case VIRTUAL_NV_INDEX_SERIAL_NUMBER:
		// TODO(louiscollard): Implement.
		return "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad";
	default:
		// TODO(louiscollard): Do something more terminal?
		return 0;
	}
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

uint32_t _plat__NvGetHandleVirtualOffset(uint32_t handle)
{
	switch (handle) {
	case VIRTUAL_NV_INDEX_BOARD_ID:
	case VIRTUAL_NV_INDEX_SERIAL_NUMBER:
		return NvIndexToNvOffset(handle);
	default:
		return 0;
	}
}

BOOL _plat__NvOffsetIsVirtual(unsigned int startOffset)
{
	// TODO(louiscollard): Consider checking the full address to ensure it's
	// valid. This shouldn't be necessary though, as these functions should
	// be the only source of addresses in this range (assuming
	// VIRTUAL_NV_OFFSET_START has been chosen properly).
	return (startOffset & 0xffff0000) == VIRTUAL_NV_OFFSET_START;
}

void _plat__NvVirtualMemoryRead(unsigned int startOffset, unsigned int size,
				void *data)
{
	uint32_t nvIndex;
	uint16_t nvDataSize;
	unsigned int offset;
	unsigned int copied;

	nvIndex = NvOffsetToNvIndex(startOffset);
	nvDataSize = GetNvDataSize(nvIndex);

	// Calculate offset within this NV index.
	offset = startOffset - NvIndexToNvOffset(nvIndex);
	copied = 0;

	// The first 4 bytes are supposed to represent a pointer to the next
	// element in the NV index list; we are not doing that here, and this
	// area should not be queried.
	assert(offset >= NV_INDEX_READ_OFFSET);

	// Check the request is reasonable, given the size of this index.
	assert(offset + size <= NV_DATA_READ_OFFSET + nvDataSize);

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

		CopyNvIndex(data, nvIndex, nvDataSize);

		copied += VIRTUAL_NV_INDEX_HEADER_SIZE;
	}

	// We may or may not have copied the header at this point. Either way,
	// check if we need to copy any actual virtual NV data, and do so if
	// necessary.
	if (size - copied > 0) {
		// Note that at this point, offset must either be 0, or >=
		// NV_DATA_READ_OFFSET.
		memcpy((BYTE *)data + copied,
		       (BYTE *)GetNvData(nvIndex) +
		       (offset % NV_DATA_READ_OFFSET),
		       size - copied);
	}
}
