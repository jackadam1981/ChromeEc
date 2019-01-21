/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "compile_time_macros.h"
#include "common.h"
#include "console.h"
#include "heci_client.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)

static struct host_packet heci_packet;	/* For host command processing */
static struct host_cmd_handler_args host_cmd_args;
static uint8_t mem_mapped[0x200] __attribute__ ((section(".bss.big_align")));

#define __packed __attribute__((packed))

#define HECI_CLIENT_CROS_EC_ISH_GUID { 0x7b7154d0, 0x56f4, 0x4bdc,\
			 { 0xb0, 0xd8, 0x9e, 0x7c, 0xda, 0xe0, 0xd6, 0xa0 } }

/* ISH Loader Host Commands */
enum hostif_commands {
        CROS_EC_CMD_HOST2ISH = 0,
        /*TODO CROS_EC_CMD_ISH2HOST, */
};

#define COMMAND_MASK			0x7F
#define RESPONSE_FLAG			0x80

struct cros_ec_ishtp_msg_hdr {
        uint8_t command; /* Bit7 indicates a response to command */
        uint8_t status;
        uint8_t reserved[2];
} __packed;

struct cros_ec_ishtp_out_msg {
	struct cros_ec_ishtp_msg_hdr hdr;
	struct ec_host_response ec_response;
} __packed;

struct cros_ec_ishtp_in_msg {
	struct cros_ec_ishtp_msg_hdr hdr;
	uint8_t protocol_version;
	struct ec_host_request ec_request;
} __packed;

struct cros_ec_ishtp_cl_data {
	heci_handle_t heci_handle;
	/*TODO: Add protocol version etc? */
};

static struct cros_ec_ishtp_cl_data cros_ec_ishtp_data = {
	.heci_handle = HECI_INVALID_HANDLE,
};

static uint8_t *heci_get_hostcmd_data_range(void)
{
	return mem_mapped;
}

//TODO: Global
static uint8_t global_out_buf[sizeof(struct cros_ec_ishtp_out_msg) + EC_LPC_HOST_PACKET_SIZE] __attribute__ ((section(".bss.big_align")));

static int heci_get_protocol_info(struct host_cmd_handler_args *args)
{
	struct ec_response_get_protocol_info *r = args->response;

	memset(r, 0, sizeof(*r));
	r->protocol_versions = (1 << 3);
	r->max_request_packet_size = EC_LPC_HOST_PACKET_SIZE;
	r->max_response_packet_size = EC_LPC_HOST_PACKET_SIZE;

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PROTOCOL_INFO, heci_get_protocol_info,
		     EC_VER_MASK(0));

static void heci_send_response_packet(struct host_packet *pkt)
{
	struct cros_ec_ishtp_out_msg *out_msg = (struct cros_ec_ishtp_out_msg *)&global_out_buf;

	CPRINTS("heci_send_response_packet()");

	//TODO: if more HECI commands are to be added, we need a way
	// to pass the in_msg->hdr.command for which response is sent
	out_msg->hdr.command = CROS_EC_CMD_HOST2ISH | RESPONSE_FLAG;
	out_msg->hdr.status = 0;
	memcpy((void *)&out_msg->ec_response, pkt->response, pkt->response_size);
	heci_send_msg(cros_ec_ishtp_data.heci_handle, (uint8_t *)out_msg,
		sizeof(struct cros_ec_ishtp_out_msg) + pkt->response_size);
}

static void cros_ec_ishtp_subsys_new_msg_received(const heci_handle_t handle,
					uint8_t *msg, const size_t msg_size)
{
	struct cros_ec_ishtp_in_msg *in_msg = (struct cros_ec_ishtp_in_msg *)msg;
	struct cros_ec_ishtp_out_msg *out_msg = (struct cros_ec_ishtp_out_msg *)&global_out_buf;
	struct ec_host_request *ec_request = (struct ec_host_request *)&in_msg->ec_request;

	/* We only support new style command (v3) now */
	if (in_msg->protocol_version == EC_COMMAND_PROTOCOL_3) {
		CPRINTS("EC_COMMAND_PROTOCOL_3");

		heci_packet.send_response = heci_send_response_packet;

		heci_packet.request = (const void *)ec_request;
		heci_packet.request_temp = NULL;
		heci_packet.request_max = EC_LPC_HOST_PACKET_SIZE;
		/* Don't know the request size so pass in
		 * the entire buffer
		 */
		heci_packet.request_size = host_request_expected_size(ec_request);

		heci_packet.response = (void *)heci_get_hostcmd_data_range();
		//TODO: heci_packet.response = (void *)global_out_buf;
		heci_packet.response_max = EC_LPC_HOST_PACKET_SIZE;
		heci_packet.response_size = 0;

		heci_packet.driver_result = EC_RES_SUCCESS;
		host_packet_receive(&heci_packet);

	} else {
		/* Old style command unsupported */
		CPRINTS("ERROR: Protocol V2 is not supported!");

		/* Hand off to host command handler */
		host_cmd_args.result = EC_RES_INVALID_COMMAND;
		//TODO: host_command_received(&host_cmd_args);

		//TODO: send HECI ack for now
		out_msg->hdr.command = CROS_EC_CMD_HOST2ISH | RESPONSE_FLAG;
		out_msg->hdr.status = 0;
		heci_send_msg(cros_ec_ishtp_data.heci_handle, (uint8_t *)out_msg,
			sizeof(struct cros_ec_ishtp_out_msg));
	}
}

static int cros_ec_ishtp_subsys_initialize(const heci_handle_t heci_handle)
{
	cros_ec_ishtp_data.heci_handle = heci_handle;
	//TODO
	return 0;
}

/* return zero if resume request handled successfully */
static int cros_ec_ishtp_subsys_resume(const heci_handle_t heci_handle)
{
	//TODO
	return 0;
}

/* return zero if suspend request handled successfully */
static int cros_ec_ishtp_subsys_suspend(const heci_handle_t heci_handle)
{
	//TODO
	return 0;
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
