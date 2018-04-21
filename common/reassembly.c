/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Reassembly is a generic layer providing reassembly services for any user,
 * for instance for USB endpoints.
 *
 * This layer includes error checking and provides error free services to its
 * users.
 *
 * The endpoint doing the reassembly is always a client of the client/server
 * pair.
 *
 * PDUs sent by the server have the following format:
 *
 *  +---+---------+----------+-----......------+
 *  | v |  check  |   size   |   payload       |
 *  +---+---------+----------+-----......------+
 *
 * where
 *   v - a single byte protocol version.
 *   check - a four byte field. The two LSBits are used to communicate the
 *           checksum type (crc32, truncated hash, etc.), the rest the actual
 *           checksum truncated as necessary.
 *   size - a two byte field, total size of the PDU
 *  payload - a variable size field of 'size' bytes.
 *
 * check and size fields are transferred in network byte order (MSB first).
 *
 * Each reassembly session has a context associated with it.
 *
 * Program entities which need to use this reassembly service create a context
 * and register it with the service.
 *
 * The context among other things includes a pointer to the buffer for
 * reassembled PDUs. On success the registration function sets the offset into
 * the buffer to allow the user to retrieve reassembled PDUs.
 *
 * The return value of the API indicates the state of reassembly:
 *  - more required
 *  - reassembly succeeded
 */

#include <common.h>
#include <endian.h>
#include <reassembly.h>
#include <timer.h>
#include <util.h>

/* One second should be plenty for reassembly packets. */
#define RS_TIMEOUT_US SECOND

struct reassembly_context {
	uint32_t us_stamp;
	uint16_t payload_size;
	uint16_t pdu_offset;
	int (*pdu_check_func)(const void *, size_t,
			      const void *, size_t);
	struct reassembly_pdu pdu;
} __packed;

size_t reassembly_overhead(void)
{
	return sizeof(struct reassembly_context);
}

void *reassembly_register(void *buffer, size_t buffer_size,
			  int (*pdu_check_func)(const void *, size_t,
						const void *, size_t))
{
	struct reassembly_context *ctx;

	if (buffer_size <= sizeof(struct reassembly_context))
		return NULL;

	ctx = buffer;
	memset(ctx, 0, sizeof(*ctx));
	ctx->payload_size = buffer_size - sizeof(struct reassembly_context);
	ctx->pdu_check_func = pdu_check_func;

	return buffer;
}

enum reassembly_result reassembly_feed(void *c, void *buffer, size_t size)
{
	timestamp_t tstamp;
	uint16_t expected_size;
	struct reassembly_context *ctx = c;

	/* First things first: does it fit? */
	if ((size + ctx->pdu_offset) > ctx->payload_size) {
		ctx->pdu_offset = 0;
		return RS_OVERFLOW;
	}

	tstamp = get_time();
	if (ctx->pdu_offset && ((tstamp.le.lo - ctx->us_stamp) > RS_TIMEOUT_US)) {
		ctx->pdu_offset = 0;
		return RS_TIMEOUT;
	}

	ctx->us_stamp = tstamp.le.lo;

	memcpy(ctx->pdu.payload + ctx->pdu_offset, buffer, size);
	ctx->pdu_offset += size;

	if (ctx->pdu_offset < sizeof(struct reassembly_pdu))
		return RS_NEED_MORE_DATA;

	expected_size = be16toh(ctx->pdu.size);

	if (ctx->pdu_offset < expected_size)
		return RS_NEED_MORE_DATA;

	if (ctx->pdu_offset > expected_size)
		return RS_OVERFLOW;

	/* Prepare for the next PDU. */
	ctx->pdu_offset = 0;

	/* All right, the exact PDU has been received, let's verify its contents. */
	if (ctx->pdu_check_func(&ctx->pdu.check,
				sizeof(ctx->pdu.check),
				(const uint8_t *)ctx +
				offsetof(struct reassembly_context, pdu.size),
				expected_size + sizeof(be16toh(ctx->pdu.size))))
		return RS_SUCCESS;

	return RS_CHECK_ERROR;
}
