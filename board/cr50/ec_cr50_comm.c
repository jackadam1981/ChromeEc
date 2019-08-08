/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "crc8.h"
#include "hooks.h"
#include "registers.h"
#include "string.h"
#include "timer.h"
#include "usart.h"
#include "vboot.h"

/*
 * EC communications state machine (FSM) is supposed to be active between TPM
 * reset (including power on) and the moment EC_IN_RW transition is latched.
 *
 * Each packet is supposed to be prepended by at least one synchronization
 * character (0xEC). If packet does not verify, the entire stream contents,
 * including all sync characters is delivered to the USB bridge.
 *
 * Meaning of the states included in the enum below is as follows:
 */
enum ec_comms_state {
	EC_SYNCING,
	EC_PREFIX,
	EC_RECEIVING_PACKET,
};

static struct {
	enum ec_comms_state phase : 8;
	unsigned prefix_count : 8;
	unsigned packet_count : 8;
	union {
		struct cr50_comm_packet ph;
		uint8_t packet[32];
	};
} ec_comms_info;

uint32_t stored_version;

static uint8_t process_ec_packet(const struct cr50_comm_packet *ph)
{
	uint32_t new_version;
	/*
	 * We know the size field makes sense and matches the actual packet
	 * length. Let's verify CRC.
	 */
	if (crc8((const uint8_t *)&ph->type, sizeof(struct cr50_comm_packet) - offsetof(struct cr50_comm_packet, type) + ph->size) != ph->crc)
		return CR50_COMM_ERROR_CRC;

	if (ph->type != CR50_CMD_FW_VERSION)
		return CR50_COMM_ERROR_UNSUPPORTED;

	if (ph->size != sizeof(new_version))
		return CR50_COMM_ERROR_SIZE;

	memcpy(&new_version, ph + 1, sizeof(new_version));
	if (new_version < stored_version)
		/* Rollback attempt. */
		return CR50_COMM_ERROR_ROLLBACK;
	else
		return CR50_COMM_SUCCESS;
}

int ec_comms_active(uint8_t c)
{
	size_t expected_size;
	int rv;

	switch (ec_comms_info.phase) {
	case EC_SYNCING:
		/* The EC is not trying to talk to us. */
		if (c != 0xec) {
			ec_comms_info.prefix_count = 0;
			return 0;
		}
		ec_comms_info.prefix_count++;
		ec_comms_info.phase = EC_PREFIX;
		return 1;

	case EC_PREFIX:
		if (c == 0xec) {
			ec_comms_info.prefix_count++;
			return 1;
		}
		if (c != CR50_MAGIC_CHAR0)
			return CR50_COMM_ERROR_MAGIC;

		ec_comms_info.phase = EC_RECEIVING_PACKET;
		ec_comms_info.packet[0] = c;
		ec_comms_info.packet_count = 1;
		return 1;

	default: /* In fact the only option here is EC_RECEIVING_PACKET. */
		break;
	}

	ec_comms_info.packet[ec_comms_info.packet_count++] = c;

	if (ec_comms_info.packet_count < sizeof(struct cr50_comm_packet))
		return 1; /* Keep going. */

	if (ec_comms_info.packet_count >= sizeof(ec_comms_info.packet)) {
		rv = CR50_COMM_ERROR_SIZE;
		goto exit;
	}

	/* We are receiving packet, let's see what we got. */
	if (ec_comms_info.ph.magic != CR50_PACKET_MAGIC) {
		rv = CR50_COMM_ERROR_MAGIC;
		goto exit;
	}


	expected_size = ec_comms_info.ph.size + offsetof(struct cr50_comm_packet, data);

	if (expected_size > (sizeof(ec_comms_info.packet))) {
		rv = CR50_COMM_ERROR_SIZE;
		goto exit;
	}

	if (ec_comms_info.packet_count != expected_size)
		return 1; /* Not yet. */


	/*
	 * The EC is done sending the packet, let's process it.
	 *
	 * We can spend some time inside the ISR, as at this point the AP is
	 * most likely not sending us anything, neither on the TPM interface,
	 * nor on the console. I.e there no other activity to worry about
	 * interrupt latency impact.
	 */
	rv = process_ec_packet(&ec_comms_info.ph);
 exit:
	ec_comms_info.phase = EC_SYNCING;
	ec_comms_info.prefix_count = 0;
	ec_comms_info.packet_count = 0;
	return rv;
}

DECLARE_DEFERRED(check_ec_entering_rw);
void check_ec_entering_rw(void)
{
	if (!GREAD_FIELD(RBOX, INT_STATE, INTR_ENTERING_RW_RED)) {
		ec_comms_uart = UART_EC;
		hook_call_deferred(&check_ec_entering_rw_data, 1 * SECOND);
		return;
	}
	GWRITE_FIELD(RBOX, INT_STATE, INTR_ENTERING_RW_RED, 1);
	ec_comms_uart = ~0;
}
