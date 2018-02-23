/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "desc_parser.h"
#include "gsctool.h"
#include "tpm_vendor_cmds.h"
#include "verify_ro.h"

static void dump_area(const char *title, uint32_t offset, size_t size, const void *data)
{
	const uint8_t *bytes = data;
	size_t i;
	uint8_t alignment;

	/* Calculate how many characters we need to skip in the first dump line. */
	alignment = offset % 16;
	if (alignment) {
		size += alignment;
		alignment = 16 - alignment;
		offset &= ~0xf;
	}

	if  (title)
		printf("%s\n", title);

	/* Let's print data space separated 16 bytes per line. */
	for (i = 0; i < size; i++) {
		if (!(i % 16))
			printf("\n%06zx", offset + i);

		if (alignment) {
			printf("   ");
			alignment--;
		} else {
			printf(" %02x", bytes[i]);
		}
	}
}

static int set_new_range(struct transfer_descriptor *td,
			 enum range_type_t  new_type)
{
	uint32_t rv;
	struct vendor_cc_spi_hash_request req;

	memset(&req, 0, sizeof(req));

	/* Need to send command to change spihash mode. */
	switch(new_type) {
	case AP_RANGE:
		req.subcmd = SPI_HASH_SUBCMD_AP;
		break;
	case EC_RANGE:
		req.subcmd = SPI_HASH_SUBCMD_EC;
		break;
	case EC_GANG_RANGE:
		req.subcmd = SPI_HASH_SUBCMD_EC;
		req.flags = SPI_HASH_FLAG_EC_GANG;
		break;
	default: /* Should never happen. */
		return -EINVAL;
	}

	rv = send_vendor_command(td, VENDOR_CC_SPI_HASH, &req, sizeof(req),
				 0, NULL);

	if (!rv)
		return 0;

	if (rv == VENDOR_RC_IN_PROGRESS) {
		/* This will exit() on error. */
		poll_for_pp(td, VENDOR_CC_SPI_HASH, SPI_HASH_PP_POLL);
	} else {
		fprintf(stderr,
			"%s: failed setting range type %d, error %d\n",
			__func__, new_type, rv);
		return -EINVAL;
	}

	return 0;
}
static int verify_hash_section(struct transfer_descriptor *td,
			       struct vendor_cc_spi_hash_request *req,
			       struct addr_range *range)
{
	/* This is a hash comparison case. */
	static ssize_t matching_range = -1;
	size_t i;
	uint8_t response[100];
	size_t response_size;
	int rv;

	/* Let's retrieve the hash from the DUT. */
	response_size = sizeof(response);
	req->subcmd = SPI_HASH_SUBCMD_SHA256;
	rv = send_vendor_command(td, VENDOR_CC_SPI_HASH,
				 req, sizeof(*req), response, &response_size);

	if (rv) {
		fprintf(stderr,
			"%s: failed retrieving hash at %x, tpm error %d\n",
			__func__, req->offset, rv);
		return -EINVAL;
	}

	if (response_size != sizeof(range->variants->expected_result)) {
		fprintf(stderr, "got %zd bytes in response for range %x:%x\n",
			response_size, req->offset, req->size);
		return -EINVAL;
	}

	if (matching_range < 0) {
		/* This is the first hash range to be processed. */
		struct result_node *variant = range->variants;

		for (i = 0; i < range->variant_count; i++) {
			if (!memcmp(variant->expected_result,
				    response, response_size)) {
				matching_range = i;
				return 0;
			}
			variant++;
		}

		fprintf(stderr, "no matching hash found for range %x:%x\n",
			req->offset, req->size);
		return -EINVAL;
	}

	if (!memcmp(range->variants[matching_range].expected_result,
		    response, response_size))
		return 0;

	fprintf(stderr, "hash mismatch for range %x:%x\n",
		req->offset, req->size);

	return -EINVAL;
}

static int dump_range(struct transfer_descriptor *td,
		      struct vendor_cc_spi_hash_request *req)
{
	size_t  remaining_size = req->size;
	uint8_t response[100];
	size_t response_size;

	/* Max size of a single shot is 32 bytes. */
	req->subcmd = SPI_HASH_SUBCMD_DUMP;
	while (remaining_size) {
		size_t shot_size = 32;
		uint8_t alignment;
		uint32_t rv;

		alignment = req->offset % 16;

		if (alignment && ((alignment + remaining_size) > 16))
			/* first line should be truncated. */
			shot_size = 16 - alignment;
		else if (shot_size > remaining_size)
			shot_size = remaining_size;

		req->size = shot_size;
		response_size = shot_size;
		rv = send_vendor_command(td, VENDOR_CC_SPI_HASH,
					 req, sizeof(*req), response,
					 &response_size);
		if (rv) {
			fprintf(stderr,
				"%s: failed getting dump contents at %x\n",
				__func__, req->offset);
			return -EINVAL;
		}

		if (response_size != shot_size) {
			fprintf(stderr,
				"%s: dump error: got %zd bytes, expected %zd\n",
				__func__, response_size, shot_size);
			return -EINVAL;
		}

		dump_area(NULL, req->offset, shot_size, response);
		remaining_size -= shot_size;
		req->offset += shot_size;
	}
	printf("\n");

	return 0;
}

static int process_descriptor_secitons(struct transfer_descriptor *td)
{
	struct vendor_cc_spi_hash_request req;
	int rv;
	struct addr_range *range;
	enum range_type_t current_range = NOT_A_RANGE;

	do {
		/*
		 * Retrieve next range descriptor from the file. The function
		 * below is guaranteed to set range to NULL on any error.
		 */
		rv = parser_get_next_range(&range);
		if (rv) {
			 /*
			  * ENODATA means all board's sections have been
			  * processed.
			  */
			if (rv == -ENODATA)
				rv = 0;
			break;
		}

		if (current_range != range->range_type) {
			rv = set_new_range(td, range->range_type);
			if (rv)
				break;
		}

		memset(&req, 0, sizeof(req));
		req.offset = range->base_addr;
		req.size = range->range_size;

		if (range->variant_count)
			rv = verify_hash_section(td, &req, range);
		else
			rv = dump_range(td, &req);

		free(range);
		range = NULL;
	}  while (!rv);

	if (range)
		free(range);

	if (rv)
		return rv;

	memset(&req, sizeof(req), 0);
	req.subcmd = SPI_HASH_SUBCMD_DISABLE;
	rv = send_vendor_command(td, VENDOR_CC_SPI_HASH, &req,
				    sizeof(req), 0, NULL);
	if (rv) {
		fprintf(stderr,
			"%s: spi hash disable TPM error %d\n", __func__, rv);
		return -EINVAL;
	}
	return 0;
}

int verify_ro(struct transfer_descriptor *td,
	      const char *desc_file_name)
{
	/* First find out board ID of the target. */
	struct board_id bid;
	char rlz_code[sizeof(bid.type) + 1];

	/*
 	 * Find out what Board ID is the device we are talking to. This
	 * function calls exit() on any error.
	 */
	process_bid(td, bid_get, &bid);

	if (bid.type != ~bid.type_inv) {
		fprintf(stderr, "Inconsistent board ID: %08x != ~%08x\n",
			bid.type, bid.type_inv);
		return -EINVAL;
	}

	/*
	 * Convert bid from int to asciiz so that it could be used for
	 * strcmp() on the descriptor file section headers.
	 */
	memcpy(rlz_code, &bid.type, sizeof(rlz_code) - 1);
	rlz_code[sizeof(rlz_code) - 1] = '\0';

	if (!parser_find_board(desc_file_name, rlz_code)) {
		/* Opened the file and found descriptors for DUT. */
		printf("Processing sections for board ID %s\n", rlz_code);
		return process_descriptor_secitons(td);
	}

	printf("No descripton for board ID %s found\n", rlz_code);
	return -1;
 }
