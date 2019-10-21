/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_INCLUDE_VBOOT_H
#define __CROS_EC_INCLUDE_VBOOT_H

#include "common.h"
#include "vb21_struct.h"
#include "rsa.h"
#include "sha256.h"
#include "timer.h"

/**
 * Validate key contents.
 *
 * @param key
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vb21_is_packed_key_valid(const struct vb21_packed_key *key);

/**
 * Validate signature contents.
 *
 * @param sig Signature to be validated.
 * @param key Key to be used for validating <sig>.
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vb21_is_signature_valid(const struct vb21_signature *sig,
			    const struct vb21_packed_key *key);

/**
 * Check data region is filled with ones
 *
 * @param data  Data to be validated.
 * @param start Offset where validation starts.
 * @param end   Offset where validation ends. data[end] won't be checked.
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vboot_is_padding_valid(const uint8_t *data, uint32_t start, uint32_t end);

/**
 * Verify data by RSA signature
 *
 * @param data Data to be verified.
 * @param len  Number of bytes in <data>.
 * @param key  Key to be used for verification.
 * @param sig  Signature of <data>
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vboot_verify(const uint8_t *data, int len,
		 const struct rsa_public_key *key, const uint8_t *sig);

/**
 * Entry point of EC EFS
 */
void vboot_main(void);

/**
 * Get if vboot requires PD comm to be enabled or not
 *
 * @return 1: need PD communication. 0: PD communication is not needed.
 */
int vboot_need_pd_comm(void);

/**
 * Callback for boards to notify users of vboot error when no display is
 * available.
 *
 * Typically this happens when a Chromebox is booting on a Type-C adapter and
 * EFS failed.
 */
void led_critical(void);

/**
 * Interrupt handler for packet mode entry. Unused.
 *
 * @param signal
 */
void packet_mode_interrupt(enum gpio_signal signal);

#define CR50_COMM_PREAMBLE	0xec
#define CR50_PACKET_MAGIC	0x4345	/* 'EC' in little endian */

/**
 * EC-Cr50 data stream looks like as follows:
 *
 *   [preamble]|[header][payload]
 *
 * preamble: 0xec ...
 * header: struct cr50_comm_packet
 * payload: data[]
 */
struct cr50_comm_packet {
	/* Header */
	uint16_t magic;	/* CR50_PACKET_MAGIC */
	uint8_t crc;	/* checksum computed from all bytes after crc */
	uint16_t type;	/* CR50_CMD_* (or control packet with no data) */
	uint8_t size;	/* Payload size. Max 256 bytes. Easy on Cr50 buffer. */
	/* Payload */
	uint8_t data[];
} __packed;

#define CR50_COMM_MAX_PACKET_SIZE	(sizeof(struct cr50_comm_packet) + 32)
#define CR50_UART_RX_BUFFER_SIZE	32	/* TODO: Get from Cr50 header */

/*
 * Timeout for EC to wait for response from Cr50.
 */
#define CR50_COMM_TIMEOUT		(50 * MSEC)

/* commands */
#define CR50_COMM_CMD_HELLO		0x0
#define CR50_COMM_CMD_SET_BOOT_MODE	0x1
#define CR50_COMM_CMD_VERIFY_HASH	0x2

/* return code */
enum cr50_comm_res {
	CR50_COMM_SUCCESS = 0xec00,
	CR50_COMM_ERROR_UNKNOWN = 0xece0,
	CR50_COMM_ERROR_MAGIC = 0xece1,
	CR50_COMM_ERROR_CRC = 0xece2,
	CR50_COMM_ERROR_ROLLBACK = 0xece3,
	CR50_COMM_ERROR_TIMEOUT = 0xece4,
	CR50_COMM_ERROR_HASH_MISMATCH = 0xece7,
} __packed;
BUILD_ASSERT(sizeof(enum cr50_comm_res) == sizeof(uint16_t));

enum boot_mode {
	BOOT_MODE_RESET = 0,
	BOOT_MODE_NORMAL = 1,
	BOOT_MODE_NO_BOOT = 2,
	BOOT_MODE_RECOVERY = 3,
	BOOT_MODE_NO_RECOVERY = 4,
	/* boot_mode is uint8_t */
	BOOT_MODE_LIMIT = 255,
};

#endif  /* __CROS_EC_INCLUDE_VBOOT_H */
