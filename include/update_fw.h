/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_UPDATE_FW_H
#define __CROS_EC_UPDATE_FW_H

#include <stddef.h>

/* TODO: Handle this in update_fw.c, not usb_update.c */
#define UPDATE_DONE          0xB007AB1E

/*
 * The payload of the update command. (Integer values in network byte order).
 *
 * block digest: the first four bytes of the sha1 digest of the rest of the
 *               structure.
 * block_base:  address where this block needs to be written to.
 * block_body:  variable size data to written at address 'block_base'.
 */
struct update_command {
	uint32_t  block_digest;
	uint32_t  block_base;
	uint8_t   block_body[0];
} __packed;

struct update_pdu_header {
	/* Total size of the block, including this field. */
	uint32_t block_size;

	union {
		struct update_command cmd;
		/*
		 * The programmer puts response to the same buffer where the
		 * command was.
		 */
		uint32_t resp;
	};
};

/*
 * A convenience structure which allows to group together various revision
 * fields of the header created by the signer.
 *
 * These fields are compared when deciding if versions of two images are the
 * same or when deciding which one of the available images to run.
 */
struct signed_header_version {
	uint32_t minor;
	uint32_t major;
	uint32_t epoch;
};

/*
 * Response to the connection establishment request.
 *
 * When responding to the very first packet of the upgrade sequence, the
 * original USB update implementation was responding with a four byte value,
 * just as to any other block of the transfer sequence.
 *
 * It became clear that there is a need to be able to enhance the upgrade
 * protocol, while staying backwards compatible.
 *
 * All newer protocol versions (starting with version 2) respond to the very
 * first packet with an 8 byte or larger response, where the first 4 bytes are
 * a version specific data, and the second 4 bytes - the protocol version
 * number.
 *
 * This way the host receiving of a four byte value in response to the first
 * packet is considered an indication of the target running the 'legacy'
 * protocol, version 1. Receiving of an 8 byte or longer response would
 * communicates the protocol version in the second 4 bytes.
 */
struct first_response_pdu {
	uint32_t return_value;

	/* The below fields are present in versions 2 and up. */
	uint32_t protocol_version;

	/* The below fields are present in versions 3 and up. */
	uint32_t  backup_ro_offset;
	uint32_t  backup_rw_offset;

	/* The below fields are present in versions 4 and up. */
	/* Versions of the currently active RO and RW sections. */
	struct signed_header_version shv[2];

	/* The below fields are present in versions 5 and up */
	/* keyids of the currently active RO and RW sections. */
	uint32_t keyid[2];
};

/*
 * This array defines possible sections available for the firmare update.
 * The section which does not map the current execting code is picked as the
 * valid update area. The values are offsets into the flash space.
 *
 * This should be defined in board.c, with each entry containing:
 * {CONFIG_RW_MEM_OFF, CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE}
 * for its relevant section.
 */
struct section_descriptor {
	uint32_t sect_base_offset;
	uint32_t sect_top_offset;
};

extern const struct section_descriptor * const rw_sections;
extern const int num_rw_sections;

/* Various update extension command return values. */
enum update_return_value {
	UPDATE_SUCCESS = 0,
	UPDATE_BAD_ADDR = 1,
	UPDATE_ERASE_FAILURE = 2,
	UPDATE_DATA_ERROR = 3,
	UPDATE_WRITE_FAILURE = 4,
	UPDATE_VERIFY_ERROR = 5,
	UPDATE_GEN_ERROR = 6,
	UPDATE_MALLOC_ERROR = 7,
	UPDATE_ROLLBACK_ERROR = 8,
	UPDATE_RATE_LIMIT_ERROR = 9,
};

void fw_update_command_handler(void *body,
				size_t cmd_size,
				size_t *response_size);

#endif  /* ! __CROS_EC_UPDATE_FW_H */
