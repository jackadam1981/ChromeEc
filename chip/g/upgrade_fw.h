/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_CHIP_G_UPGRADE_FW_H
#define __EC_CHIP_G_UPGRADE_FW_H

#include <stddef.h>

#define UPGRADE_PROTOCOL_VERSION 2

/* This is the format of the header the programmer expects. */
struct upgrade_command {
	uint32_t  block_digest;  /* first 4 bytes of sha1 of the rest of the
				    block. */
	uint32_t  block_base;    /* Offset of this block into the flash SPI. */
	/* The actual payload goes here. */
	uint8_t   block_body[0];
} __packed;

/* This is the format of the header the host uses. */
struct update_pdu_header {
	uint32_t block_size;    /* Total size of the block, including this
				   field. */
	union {
		struct upgrade_command cmd;
		uint32_t resp; /* The programmer puts response to the same
				  buffer where the command was. */
	};
	/* The actual payload goes here. */
};

/*
 * Protocol specific response to the message initiating the update sequence.
 *
 * When responding to the very first packet of the upgrade sequence, the
 * original implementation was responding with a four byte value, just as to
 * any other block of the transfer sequence.
 *
 * It became clear that there is a need to be able to enhance the upgrade
 * protocol, while stayng backwards compatible. To achieve that we respond to
 * the very first packet with an 8 byte value, the first 4 bytes the same as
 * before, the second 4 bytes - the protocol version number.
 *
 * This way if on the host side receiving of a four byte value in response to
 * the first packet is an indication of the 'legacy' protocol, version 0.
 * Receiving of an 8 byte or longer response would communicate the protocol
 * version in the second 4 bytes.
 */
struct first_response_pdu {
	uint32_t return_value;
	uint32_t protocol_version;
	union {
		struct {
			uint32_t  backup_ro_offset;
			uint32_t  backup_rw_offset;
		} vers3;
	};
};

union first_pdu_block {
	struct update_pdu_header updu;
	struct {
		uint32_t unused_offset;
		struct first_response_pdu rpdu;
	};
};

/* TODO: Handle this in upgrade_fw.c, not usb_upgrade.c */
#define UPGRADE_DONE          0xB007AB1E

void fw_upgrade_command_handler(void *body,
				size_t cmd_size,
				size_t *response_size);

#endif  /* ! __EC_CHIP_G_UPGRADE_FW_H */
