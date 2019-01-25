/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "heci_client.h"
#include "host_command.h"
#include "ipc_heci.h"

#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)

#define HECI_CLIENT_CROS_EC_ISH_GUID { 0x7b7154d0, 0x56f4, 0x4bdc,\
			 { 0xb0, 0xd8, 0x9e, 0x7c, 0xda, 0xe0, 0xd6, 0xa0 } }

/* ISH Loader Host Commands */
enum hostif_commands {
	CROS_EC_CMD_HOST2ISH = 0,
};

#define COMMAND_MASK	0x7F
#define RESPONSE_FLAG	0x80

struct cros_ec_ishtp_msg_hdr {
	uint8_t command; /* Bit7 indicates a response to command */
	uint8_t status;
	uint8_t reserved[2];
} __packed;

struct cros_ec_ishtp_in_msg {
	struct cros_ec_ishtp_msg_hdr hdr;
	uint8_t protocol_version;
	struct ec_host_request ec_request;
} __packed;

struct cros_ec_ishtp_out_msg {
	struct cros_ec_ishtp_msg_hdr hdr;
	struct ec_host_response ec_response;
} __packed;

#define CROS_EC_ISHTP_IN_MSG_PREAMBLE                                          \
	offsetof(struct cros_ec_ishtp_in_msg, ec_request)

#define CROS_EC_ISHTP_OUT_MSG_PREAMBLE                                         \
	offsetof(struct cros_ec_ishtp_out_msg, ec_response)

/*
 * If we hit response buffer size issues, we can increase this. This the current
 * size we can always response in a single HECI packet.
 *
 * Aligning will other assumptions in host command stack, only a single host
 * command can be processed at a given time.
 */
static uint8_t response_buffer[IPC_MAX_PAYLOAD_SIZE];
static struct cros_ec_ishtp_out_msg *const out_msg = (void *)&response_buffer;

/* Handle for all heci cros_ec interactions */
static heci_handle_t heci_cros_ec_handle = HECI_INVALID_HANDLE;

static void heci_send_response_packet(struct host_packet *pkt)
{
	/*
	 * The out_msg buffer is already written to by the host command handler,
	 * since we set it as the response buffer.
	 */
	out_msg->hdr.command = CROS_EC_CMD_HOST2ISH | RESPONSE_FLAG;
	out_msg->hdr.status = 0;
	heci_send_msg(heci_cros_ec_handle, (uint8_t *)out_msg,
		CROS_EC_ISHTP_OUT_MSG_PREAMBLE + pkt->response_size);
}

static void cros_ec_ishtp_subsys_new_msg_received(const heci_handle_t handle,
					uint8_t *msg, const size_t msg_size)
{
	struct cros_ec_ishtp_in_msg *in_msg =
		(struct cros_ec_ishtp_in_msg *)msg;
	struct host_packet heci_packet = { 0 };

	if (in_msg->hdr.command != CROS_EC_CMD_HOST2ISH) {
		CPRINTS("ERROR: Only CROS_EC_CMD_HOST2ISH is supported!");
		return;
	}

	/* We only support new style command (v3) since ISH never sent legacy */
	if (in_msg->protocol_version != EC_COMMAND_PROTOCOL_3) {
		CPRINTS("ERROR: Only V3 is supported!");
		return;
	}

	heci_packet.send_response = heci_send_response_packet;

	heci_packet.request = &in_msg->ec_request;
	heci_packet.request_max =
		HECI_MAX_MSG_SIZE - CROS_EC_ISHTP_IN_MSG_PREAMBLE;
	heci_packet.request_size = msg_size - CROS_EC_ISHTP_IN_MSG_PREAMBLE;

	heci_packet.response = &out_msg->ec_response;
	heci_packet.response_max =
		IPC_MAX_PAYLOAD_SIZE - CROS_EC_ISHTP_OUT_MSG_PREAMBLE;
	heci_packet.response_size = 0;

	heci_packet.driver_result = EC_RES_SUCCESS;
	host_packet_receive(&heci_packet);
}

static int cros_ec_ishtp_subsys_initialize(const heci_handle_t heci_handle)
{
	heci_cros_ec_handle = heci_handle;
	return EC_SUCCESS;
}

/* return zero if resume request handled successfully */
static int cros_ec_ishtp_subsys_resume(const heci_handle_t heci_handle)
{
	return EC_SUCCESS;
}

/* return zero if suspend request handled successfully */
static int cros_ec_ishtp_subsys_suspend(const heci_handle_t heci_handle)
{
	return EC_SUCCESS;
}

static const struct heci_client_callbacks cros_ec_ishtp_subsys_heci_cbs = {
	.initialize = cros_ec_ishtp_subsys_initialize,
	.new_msg_received = cros_ec_ishtp_subsys_new_msg_received,
	.suspend = cros_ec_ishtp_subsys_suspend,
	.resume = cros_ec_ishtp_subsys_resume,
};

static const struct heci_client cros_ec_ishtp_heci_client = {
	.protocol_id = HECI_CLIENT_CROS_EC_ISH_GUID,
	.max_msg_size = HECI_MAX_MSG_SIZE,
	.protocol_ver = 1,
	.max_n_of_connections = 1,

	.cbs = &cros_ec_ishtp_subsys_heci_cbs,
};

HECI_CLIENT_ENTRY(cros_ec_ishtp_heci_client);
