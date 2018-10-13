/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "compile_time_macros.h"
#include "console.h"
#include "hbm.h"
#include "heci_client.h"
#include "ipc_heci.h"
#include "system_state.h"
#include "task.h"
#include "util.h"

#ifdef DEBUG_IPC_HECI
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)
#else
#define CPUTS(outstr)
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

struct heci_header {
	uint32_t fw_addr      :8;
	uint32_t host_addr    :8;
	uint32_t length       :9;
	uint32_t reserved     :6;
	uint32_t msg_complete :1;
} __packed;

#define HECI_IPC_PAYLOAD_SIZE   \
			(IPC_MAX_PAYLOAD_SIZE - sizeof(struct heci_header))

struct heci_msg {
	struct heci_header hdr;
	uint8_t payload[HECI_IPC_PAYLOAD_SIZE];
} __packed;

/* address for Host Bus */
#define HECI_HBM_ADDRESS				0

/* should be less than HECI_INVALID_HANDLE - 1 */
BUILD_ASSERT(HECI_MAX_NUM_OF_CLIENTS < 0x0FE);

struct heci_client_connect {
	uint8_t is_connected;
	uint8_t host_addr;
	uint8_t ignore_rx_msg;
	uint8_t	rx_msg[HECI_MAX_MSG_SIZE];
	size_t flow_ctrl_creds;
	size_t  rx_msg_length;
	struct mutex lock;
};

struct heci_client_context {
	const struct heci_client *client;
	void *data;

	struct heci_client_connect connect;
	struct ss_subsys_device ss_device;
};

struct heci_bus_context {
	uint8_t ipc_handle;

	int num_of_clients;
	struct heci_client_context client_ctxs[HECI_MAX_NUM_OF_CLIENTS];
};

/* declare heci bus */
struct heci_bus_context heci_bus_ctx = {
		.ipc_handle = IPC_INVALID_HANDLE,
};

#define HECI_CLIENT_CONTEXT(fw_addr) (&heci_bus_ctx.client_ctxs[(fw_addr) - 1])
#define HECI_CLIENT_CONNECT(fw_addr) (&HECI_CLIENT_CONTEXT(fw_addr)->connect)
#define HECI_CLIENT_IS_CONNECTED(fw_addr) \
			(HECI_CLIENT_CONTEXT(fw_addr)->connect.is_connected)
#define HECI_IS_VALID_CLIENT_ADDR(fw_addr) \
			(fw_addr > 0 && fw_addr <= heci_bus_ctx.num_of_clients)
#define TO_HECI_HANDLE(fw_addr)		(fw_addr)

#define ss_device_to_heci_client_context(ss_dev) \
	((struct heci_client_context *)((void *)(ss_dev) - \
	(void *)(&(((struct heci_client_context *)0)->ss_device))))
#define client_context_to_handle(cli_ctx) \
	(((uint32_t)((cli_ctx) - &heci_bus_ctx.client_ctxs[0]) / \
	sizeof(heci_bus_ctx.client_ctxs[0])) + 1)

static int heci_client_suspend(struct ss_subsys_device *ss_device)
{
	struct heci_client_context *cli_ctx =
			ss_device_to_heci_client_context(ss_device);
	uint8_t handle = client_context_to_handle(cli_ctx);

	if (cli_ctx->client->cbs->suspend)
		cli_ctx->client->cbs->suspend(handle);

	return 0;
}

static int heci_client_resume(struct ss_subsys_device *ss_device)
{
	struct heci_client_context *cli_ctx =
			ss_device_to_heci_client_context(ss_device);
	uint8_t handle = client_context_to_handle(cli_ctx);

	if (cli_ctx->client->cbs->resume)
		cli_ctx->client->cbs->resume(handle);

	return 0;
}

struct system_state_callbacks heci_ss_cbs = {
	.suspend = heci_client_suspend,
	.resume = heci_client_resume,
};

/*
 * This function should be called only by HECI_CLIENT_ENTRY()
 */
uint8_t heci_register_client(const struct heci_client *client)
{
	int ret;
	struct heci_client_context *cli_ctx;

	if (client == NULL || client->cbs == NULL)
		return HECI_INVALID_HANDLE;

	/* we don't need mutex here since this function is called by
	 * entry function which is serialized among heci clients by law
	 */
	if (heci_bus_ctx.num_of_clients == HECI_MAX_NUM_OF_CLIENTS)
		return HECI_INVALID_HANDLE;

	/* we only support 1 connection */
	if (client->max_n_of_connections > 1)
		return HECI_INVALID_HANDLE;

	if (client->max_msg_size > HECI_MAX_MSG_SIZE)
		return HECI_INVALID_HANDLE;

	cli_ctx = &heci_bus_ctx.client_ctxs[heci_bus_ctx.num_of_clients++];

	cli_ctx->client = client;

	if (client->cbs->initialize) {
		ret = client->cbs->initialize(heci_bus_ctx.num_of_clients);

		if (ret) {
			heci_bus_ctx.num_of_clients--;
			return HECI_INVALID_HANDLE;
		}
	}

	if (client->cbs->suspend || client->cbs->resume) {
		cli_ctx->ss_device.cbs = &heci_ss_cbs;
		ss_subsys_register_client(&cli_ctx->ss_device);
	}

	return heci_bus_ctx.num_of_clients;
}

static void heci_build_hbm_header(struct heci_header *hdr, uint32_t length)
{
	hdr->fw_addr = HECI_HBM_ADDRESS;
	hdr->host_addr = HECI_HBM_ADDRESS;
	hdr->length = length;
	hdr->reserved = 0;
	hdr->msg_complete = 1;	/* payload of hbm is less than IPC payload */
}

static void heci_build_fixed_client_header(struct heci_header *hdr,
					   uint8_t fw_addr, uint32_t length)
{
	hdr->fw_addr = fw_addr;
	hdr->host_addr = 0;
	hdr->length = length;
	hdr->reserved = 0;
	hdr->msg_complete = 1;	/* Fixed client payload < IPC payload */
}

static int heci_send_heci_msg(struct heci_msg *msg)
{
	uint8_t ipc_handle = heci_bus_ctx.ipc_handle;
	int length, written;

	if (ipc_handle == IPC_INVALID_HANDLE)
		return -1;

	length = sizeof(msg->hdr) + msg->hdr.length;
	written = ipc_write(ipc_handle, msg, length);

	if (written != length) {
		CPRINTF("%s error : len = %d err = %d\n", __func__,
			(int)length, written);
		return -1;
	}

	return 0;
}

int heci_set_client_data(uint8_t heci_handle, void *data)
{
	struct heci_client_context *cli_ctx;

	if (!HECI_IS_VALID_CLIENT_ADDR(heci_handle))
		return HECI_ERR_INVALID_HANDLE;

	cli_ctx = HECI_CLIENT_CONTEXT(heci_handle);
	cli_ctx->data = data;

	return 0;
}

void *heci_get_client_data(uint8_t heci_handle)
{
	struct heci_client_context *cli_ctx;

	if (!HECI_IS_VALID_CLIENT_ADDR(heci_handle))
		return NULL;

	cli_ctx = HECI_CLIENT_CONTEXT(heci_handle);
	return cli_ctx->data;
}

int heci_send_msg(uint8_t heci_handle, uint8_t *buf, size_t buf_size)
{
	int buf_offset = 0, ret = 0;
	struct heci_client_connect *connect;
	struct heci_msg msg;

	if (!HECI_IS_VALID_CLIENT_ADDR(heci_handle))
		return HECI_ERR_INVALID_HANDLE;

	if (buf_size > HECI_MAX_MSG_SIZE)
		return HECI_ERR_TOO_BIG_MSG_SIZE;

	connect = HECI_CLIENT_CONNECT(heci_handle);
	mutex_lock(&connect->lock);

	if (!HECI_CLIENT_IS_CONNECTED(heci_handle)) {
		ret = HECI_ERR_CLIENT_IS_NOT_CONNECTED;
		goto err_locked;
	}

	if (!connect->flow_ctrl_creds) {
		CPRINTF("no cred\n");
		ret = HECI_ERR_NO_CRED_FROM_CLIENT_IN_HOST;
		goto err_locked;
	}

	msg.hdr.fw_addr = heci_handle;
	msg.hdr.host_addr = connect->host_addr;
	msg.hdr.reserved = 0;
	while (buf_size) {
		if (buf_size > HECI_IPC_PAYLOAD_SIZE) {
			msg.hdr.msg_complete = 0;
			msg.hdr.length = HECI_IPC_PAYLOAD_SIZE;
		} else {
			msg.hdr.msg_complete = 1;
			msg.hdr.length = buf_size;
		}

		memcpy(msg.payload, buf + buf_offset, msg.hdr.length);

		heci_send_heci_msg(&msg);

		buf_size -= msg.hdr.length;
		buf_offset += msg.hdr.length;
	}

	atomic_sub(&connect->flow_ctrl_creds, 1);
	mutex_unlock(&connect->lock);

	return buf_size;

err_locked:
	mutex_unlock(&connect->lock);

	return ret;
}

int heci_send_msgs(uint8_t heci_handle, struct heci_msg_list *msg_list)
{
	struct heci_msg_item *msg_item;
	int total_size = 0;
	int i, msg_offset, buf_size, copy_size;
	struct heci_client_connect *connect;
	struct heci_msg msg;

	if (!HECI_IS_VALID_CLIENT_ADDR(heci_handle))
		return HECI_ERR_INVALID_HANDLE;

	for (i = 0; i < msg_list->num_of_items; i++)
		total_size += msg_list->items[i]->size;

	if (total_size > HECI_MAX_MSG_SIZE)
		return HECI_ERR_TOO_BIG_MSG_SIZE;

	if (msg_list->num_of_items > HECI_MAX_MSGS)
		return HECI_ERR_TOO_MANY_MSG_ITEMS;

	connect = HECI_CLIENT_CONNECT(heci_handle);
	mutex_lock(&connect->lock);

	if (!HECI_CLIENT_IS_CONNECTED(heci_handle)) {
		total_size = HECI_ERR_CLIENT_IS_NOT_CONNECTED;
		goto err_locked;
	}

	if (!connect->flow_ctrl_creds) {
		CPRINTF("no cred\n");
		total_size = HECI_ERR_NO_CRED_FROM_CLIENT_IN_HOST;
		goto err_locked;
	}

	msg.hdr.fw_addr = heci_handle;
	msg.hdr.host_addr = connect->host_addr;
	msg.hdr.reserved = 0;

	i = 1;
	msg_offset = 0;
	buf_size = 0;
	msg_item = msg_list->items[0];
	while (1) {
		if (msg_offset == msg_item->size) {
			if (i == msg_list->num_of_items)
				break;

			msg_item = msg_list->items[i++];
			msg_offset = 0;
		}

		if (buf_size == HECI_IPC_PAYLOAD_SIZE) {
			msg.hdr.length = buf_size;
			msg.hdr.msg_complete = 0;

			heci_send_heci_msg(&msg);
			buf_size = 0;
		}

		if (msg_item->size - msg_offset >
		    HECI_IPC_PAYLOAD_SIZE - buf_size) {
			copy_size = HECI_IPC_PAYLOAD_SIZE - buf_size;
		} else {
			copy_size = msg_item->size - msg_offset;
		}

		memcpy(msg.payload + buf_size, msg_item->buf + msg_offset,
		       copy_size);

		msg_offset += copy_size;
		buf_size += copy_size;
	}

	if (buf_size != 0) {
		msg.hdr.length = buf_size;
		msg.hdr.msg_complete = 1;

		heci_send_heci_msg(&msg);
	}

	atomic_sub(&connect->flow_ctrl_creds, 1);

err_locked:
	mutex_unlock(&connect->lock);

	return total_size;

}

/* For now, we only support fixed client payload size < IPC payload size */
int heci_send_fixed_client_msg(uint8_t fw_addr, uint8_t *buf, size_t buf_size)
{
	struct heci_msg msg;

	heci_build_fixed_client_header(&msg.hdr, fw_addr, buf_size);

	memcpy(msg.payload, buf, buf_size);

	heci_send_heci_msg(&msg);

	return 0;
}

static int handle_version_req(struct hbm_version_req *ver_req)
{
	struct hbm_version_res *ver_res;
	struct heci_msg heci_msg;
	struct hbm_i2h *i2h;

	heci_build_hbm_header(&heci_msg.hdr,
			      sizeof(i2h->cmd) + sizeof(*ver_res));

	i2h = (struct hbm_i2h *)heci_msg.payload;
	i2h->cmd = HECI_BUS_MSG_VERSION_RESP;
	ver_res = (struct hbm_version_res *)&i2h->data;

	memset(ver_res, 0, sizeof(*ver_res));

	ver_res->version.major = HBM_MAJOR_VERSION;
	ver_res->version.minor = HBM_MINOR_VERSION;
	if (ver_req->version.major == HBM_MAJOR_VERSION &&
	    ver_req->version.minor == HBM_MINOR_VERSION) {
		ver_res->supported = 1;
	} else {
		ver_res->supported = 0;
	}

	heci_send_heci_msg(&heci_msg);

	return 0;
}

static int handle_enum_req(struct hbm_enum_req *enum_req)
{
	struct hbm_enum_res *enum_res;
	struct heci_msg heci_msg;
	struct hbm_i2h *i2h;
	int i, size_in_bits;

	heci_build_hbm_header(&heci_msg.hdr,
			      sizeof(i2h->cmd) + sizeof(*enum_res));

	i2h = (struct hbm_i2h *)heci_msg.payload;
	i2h->cmd = HECI_BUS_MSG_HOST_ENUM_RESP;
	enum_res = (struct hbm_enum_res *)&i2h->data;

	memset(enum_res, 0, sizeof(*enum_res));

	size_in_bits = sizeof(enum_res->valid_addresses[0]) * 8;
	for (i = 0; i < heci_bus_ctx.num_of_clients; i++) {
		enum_res->valid_addresses[(i + 1) / size_in_bits] |=
					1 << ((i + 1) & (size_in_bits - 1));
	}

	heci_send_heci_msg(&heci_msg);

	return 0;
}

static int handle_client_prop_req(struct hbm_client_prop_req *client_prop_req)
{
	struct hbm_client_prop_res *client_prop_res;
	struct heci_msg heci_msg;
	struct hbm_i2h *i2h;
	struct heci_client_context *client_ctx;
	const struct heci_client *client;

	heci_build_hbm_header(&heci_msg.hdr,
			      sizeof(i2h->cmd) + sizeof(*client_prop_res));

	i2h = (struct hbm_i2h *)heci_msg.payload;
	i2h->cmd = HECI_BUS_MSG_HOST_CLIENT_PROP_RESP;
	client_prop_res = (struct hbm_client_prop_res *)&i2h->data;

	memset(client_prop_res, 0, sizeof(*client_prop_res));

	client_prop_res->address = client_prop_req->address;
	if (!HECI_IS_VALID_CLIENT_ADDR(client_prop_req->address)) {
		client_prop_res->status = HECI_CONNECT_STATUS_CLIENT_NOT_FOUND;
	} else {
		struct hbm_client_properties *client_prop;

		client_ctx = HECI_CLIENT_CONTEXT(client_prop_req->address);
		client = client_ctx->client;
		client_prop = &client_prop_res->client_prop;

		client_prop->protocol_name = client->protocol_id;
		client_prop->protocol_version = client->protocol_ver;
		client_prop->max_number_of_connections =
						client->max_n_of_connections;
		client_prop->max_msg_length = client->max_msg_size;
		client_prop->dma_hdr_len = client->dma_header_length;
		client_prop->dma_enabled = client->dma_enabled;
	}

	heci_send_heci_msg(&heci_msg);

	return 0;
}

static int heci_send_flow_control(uint8_t fw_addr)
{
	struct heci_client_connect *connect;
	struct hbm_i2h *i2h;
	struct hbm_flow_control *flow_ctrl;
	struct heci_msg heci_msg;

	connect = HECI_CLIENT_CONNECT(fw_addr);

	heci_build_hbm_header(&heci_msg.hdr,
			      sizeof(i2h->cmd) + sizeof(*flow_ctrl));

	i2h = (struct hbm_i2h *)heci_msg.payload;
	i2h->cmd = HECI_BUS_MSG_FLOW_CONTROL;
	flow_ctrl = (struct hbm_flow_control *)&i2h->data;

	memset(flow_ctrl, 0, sizeof(*flow_ctrl));

	flow_ctrl->fw_addr = fw_addr;
	flow_ctrl->host_addr = connect->host_addr;

	heci_send_heci_msg(&heci_msg);

	return 0;
}

static int handle_client_connect_req(
			struct hbm_client_connect_req *client_connect_req)
{
	struct hbm_client_connect_res *client_connect_res;
	struct heci_msg heci_msg;
	struct hbm_i2h *i2h;
	struct heci_client_connect *connect;

	heci_build_hbm_header(&heci_msg.hdr,
			      sizeof(i2h->cmd) + sizeof(*client_connect_res));

	i2h = (struct hbm_i2h *)heci_msg.payload;
	i2h->cmd = HECI_BUS_MSG_CLIENT_CONNECT_RESP;
	client_connect_res = (struct hbm_client_connect_res *)&i2h->data;

	memset(client_connect_res, 0, sizeof(*client_connect_res));

	client_connect_res->fw_addr = client_connect_req->fw_addr;
	client_connect_res->host_addr = client_connect_req->host_addr;
	if (!HECI_IS_VALID_CLIENT_ADDR(client_connect_req->fw_addr)) {
		client_connect_res->status =
					HECI_CONNECT_STATUS_CLIENT_NOT_FOUND;
	} else {
		connect = HECI_CLIENT_CONNECT(client_connect_req->fw_addr);
		if (connect->is_connected) {
			client_connect_res->status =
					HECI_CONNECT_STATUS_ALREADY_EXISTS;
		} else {
			connect->is_connected = 1;
			connect->host_addr = client_connect_req->host_addr;
		}
	}

	heci_send_heci_msg(&heci_msg);
	heci_send_flow_control(client_connect_req->fw_addr);

	return 0;
}

static int handle_flow_control_cmd(struct hbm_flow_control *flow_ctrl)
{
	struct heci_client_connect *connect;

	if (!HECI_IS_VALID_CLIENT_ADDR(flow_ctrl->fw_addr))
		return -1;

	if (!HECI_CLIENT_IS_CONNECTED(flow_ctrl->fw_addr))
		return -1;

	connect = HECI_CLIENT_CONNECT(flow_ctrl->fw_addr);
	atomic_add(&connect->flow_ctrl_creds, 1);

	return 0;
}

static void heci_handle_client_msg(struct heci_msg *msg, size_t length)
{
	struct heci_client_context *cli_ctx;
	struct heci_client_connect *connect;
	const struct heci_client_callbacks *cbs;

	if (!HECI_IS_VALID_CLIENT_ADDR(msg->hdr.fw_addr))
		return;

	if (!HECI_CLIENT_IS_CONNECTED(msg->hdr.fw_addr))
		return;

	cli_ctx = HECI_CLIENT_CONTEXT(msg->hdr.fw_addr);
	cbs = cli_ctx->client->cbs;
	connect = &cli_ctx->connect;

	if (connect->is_connected &&
	    msg->hdr.host_addr == connect->host_addr) {
		if (!connect->ignore_rx_msg &&
		    connect->rx_msg_length + msg->hdr.length >
							HECI_MAX_MSG_SIZE) {
			connect->ignore_rx_msg = 1; /* too big. discard */
		}

		if (!connect->ignore_rx_msg) {
			memcpy(connect->rx_msg + connect->rx_msg_length,
			       msg->payload, msg->hdr.length);

			connect->rx_msg_length += msg->hdr.length;
		}

		if (msg->hdr.msg_complete) {
			if (!connect->ignore_rx_msg) {
				cbs->new_msg_received(
					TO_HECI_HANDLE(msg->hdr.fw_addr),
					connect->rx_msg,
					connect->rx_msg_length);
			}

			connect->rx_msg_length = 0;
			connect->ignore_rx_msg = 0;

			heci_send_flow_control(msg->hdr.fw_addr);
		}
	}
}

static int handle_client_disconnect_req(
			struct hbm_client_disconnect_req *client_disconnect_req)
{
	struct hbm_client_disconnect_res *client_disconnect_res;
	struct heci_msg heci_msg;
	struct hbm_i2h *i2h;
	struct heci_client_context *cli_ctx;
	struct heci_client_connect *connect;
	const struct heci_client_callbacks *cbs;
	uint8_t fw_addr;

	heci_build_hbm_header(&heci_msg.hdr, sizeof(i2h->cmd) +
					     sizeof(*client_disconnect_res));

	i2h = (struct hbm_i2h *)heci_msg.payload;
	i2h->cmd = HECI_BUS_MSG_CLIENT_DISCONNECT_RESP;
	client_disconnect_res = (struct hbm_client_disconnect_res *)&i2h->data;

	memset(client_disconnect_res, 0, sizeof(*client_disconnect_res));

	fw_addr = client_disconnect_req->fw_addr;

	client_disconnect_res->fw_addr = fw_addr;
	client_disconnect_res->host_addr = client_disconnect_req->host_addr;
	if (!HECI_IS_VALID_CLIENT_ADDR(fw_addr) ||
	    !HECI_CLIENT_IS_CONNECTED(fw_addr)) {
		client_disconnect_res->status =
					HECI_CONNECT_STATUS_CLIENT_NOT_FOUND;
	} else {
		connect = HECI_CLIENT_CONNECT(fw_addr);
		cli_ctx = HECI_CLIENT_CONTEXT(fw_addr);
		cbs = cli_ctx->client->cbs;
		mutex_lock(&connect->lock);
		if (connect->is_connected) {
			cbs->disconnected(TO_HECI_HANDLE(fw_addr));
			connect->is_connected = 0;
		}
		mutex_unlock(&connect->lock);
	}

	heci_send_heci_msg(&heci_msg);

	return 0;
}

/* host stops due to version mismatch */
static int handle_host_stop_req(struct hbm_host_stop_req *host_stop_req)
{
	struct hbm_host_stop_res *host_stop_res;
	struct heci_msg heci_msg;
	struct hbm_i2h *i2h;

	heci_build_hbm_header(&heci_msg.hdr,
			      sizeof(i2h->cmd) + sizeof(*host_stop_res));

	i2h = (struct hbm_i2h *)heci_msg.payload;
	i2h->cmd = HECI_BUS_MSG_HOST_STOP_RESP;
	host_stop_res = (struct hbm_host_stop_res *)&i2h->data;

	memset(host_stop_res, 0, sizeof(*host_stop_res));

	heci_send_heci_msg(&heci_msg);

	return 0;
}

static int handle_hbm_validity(struct hbm_h2i *h2i, size_t length)
{
	int valid_msg_len;

	valid_msg_len = sizeof(h2i->cmd);

	switch (h2i->cmd) {
	case HECI_BUS_MSG_VERSION_REQ:
		valid_msg_len += sizeof(struct hbm_version_req);
		break;

	case HECI_BUS_MSG_HOST_ENUM_REQ:
		valid_msg_len += sizeof(struct hbm_enum_req);
		break;

	case HECI_BUS_MSG_HOST_CLIENT_PROP_REQ:
		valid_msg_len += sizeof(struct hbm_client_prop_req);
		break;

	case HECI_BUS_MSG_CLIENT_CONNECT_REQ:
		valid_msg_len += sizeof(struct hbm_client_connect_req);
		break;

	case HECI_BUS_MSG_FLOW_CONTROL:
		valid_msg_len += sizeof(struct hbm_flow_control);
		break;

	case HECI_BUS_MSG_CLIENT_DISCONNECT_REQ:
		valid_msg_len += sizeof(struct hbm_client_disconnect_req);
		break;

	case HECI_BUS_MSG_HOST_STOP_REQ:
		valid_msg_len += sizeof(struct hbm_host_stop_req);
		break;

/* TODO: DMA support for large data */
#if 0
	case HECI_BUS_MSG_DMA_REQ:
		valid_msg_len += sizeof(struct hbm_dma_req);
		break;

	case HECI_BUS_MSG_DMA_ALLOC_NOTIFY:
		valid_msg_len += sizeof(struct hbm_dma_alloc_notify);
		break;

	case HECI_BUS_MSG_DMA_XFER_REQ: /* DMA transfer to FW */
		valid_msg_len += sizeof(struct hbm_dma_xfer_req);
		break;

	case HECI_BUS_MSG_DMA_XFER_RESP: /* Ack for DMA transfer from FW */
		valid_msg_len += sizeof(struct hbm_dma_xfer_resp);
		break;
#endif
	default:
		break;
	}

	if (valid_msg_len != length) {
		CPRINTF("invalid cmd(%d) valid : %d, cur : %d\n",
			valid_msg_len, length);
		/* TODO: invalid cmd. not sure to reply with error ? */
		return 0;
	}

	return 1;
}

static void heci_handle_hbm(struct hbm_h2i *h2i, size_t length)
{
	void *data = (void *)&h2i->data;

	if (!handle_hbm_validity(h2i, length))
		return;

	switch (h2i->cmd) {
	case HECI_BUS_MSG_VERSION_REQ:
		handle_version_req((struct hbm_version_req *)data);
		break;

	case HECI_BUS_MSG_HOST_ENUM_REQ:
		handle_enum_req((struct hbm_enum_req *)data);
		break;

	case HECI_BUS_MSG_HOST_CLIENT_PROP_REQ:
		handle_client_prop_req((struct hbm_client_prop_req *)data);
		break;

	case HECI_BUS_MSG_CLIENT_CONNECT_REQ:
		handle_client_connect_req(
					(struct hbm_client_connect_req *)data);
		break;

	case HECI_BUS_MSG_FLOW_CONTROL:
		handle_flow_control_cmd((struct hbm_flow_control *)data);
		break;

	case HECI_BUS_MSG_CLIENT_DISCONNECT_REQ:
		handle_client_disconnect_req(
				(struct hbm_client_disconnect_req *)data);
		break;

	case HECI_BUS_MSG_HOST_STOP_REQ:
		handle_host_stop_req((struct hbm_host_stop_req *)data);
		break;

/* TODO: DMA transfer if data is too big >= ? KB */
#if 0
	case HECI_BUS_MSG_DMA_REQ:
		handle_dma_req((struct hbm_dma_req *)data);
		break;

	case HECI_BUS_MSG_DMA_ALLOC_NOTIFY:
		handle_dma_alloc_notify((struct hbm_dma_alloc_notify *));
		break;

	case HECI_BUS_MSG_DMA_XFER_REQ: /* DMA transfer to FW */
		handle_dma_xfer_req((struct hbm_dma_xfer_req *)data);
		break;

	case HECI_BUS_MSG_DMA_XFER_RESP: /* Ack for DMA transfer from FW */
		handle_dma_xfer_resp((struct hbm_dma_xfer_resp *)data);
		break;
#endif
	default:
		break;
	}
}

static void heci_handle_heci_msg(struct heci_msg *heci_msg, size_t msg_length)
{
	if (heci_msg->hdr.fw_addr) {
		if (heci_msg->hdr.fw_addr == HECI_FIXED_SYSTEM_STATE_ADDR)
			heci_handle_system_state_msg(heci_msg->payload,
						     heci_msg->hdr.length);
		else
			heci_handle_client_msg(heci_msg, msg_length);
	} else {
		heci_handle_hbm((struct hbm_h2i *)heci_msg->payload,
				 heci_msg->hdr.length);
	}
}

#define EVENT_FLAG_BIT_HECI_MSG		(1<<13) /* event flag for HECI msg */

void heci_rx_task(void)
{
	int msg_len;
	struct heci_msg heci_msg;
	uint8_t ipc_handle;

	/* initialize heci bus context */
	heci_bus_ctx.ipc_handle = ipc_open(IPC_PEER_HOST_ID, IPC_PROTOCOL_HECI,
					   EVENT_FLAG_BIT_HECI_MSG);

	ASSERT(heci_bus_ctx.ipc_handle != IPC_INVALID_HANDLE);

	/* get ipc handle */
	ipc_handle = heci_bus_ctx.ipc_handle;

	while (1) {
		/* task will be blocked here, waiting for event */
		msg_len = ipc_read(ipc_handle, &heci_msg, sizeof(heci_msg));

		if (msg_len <= 0) {
			CPRINTS("discard heci packet");
			continue;
		}

		if (heci_msg.hdr.length + sizeof(heci_msg.hdr) == msg_len)
			heci_handle_heci_msg(&heci_msg, msg_len);
		else
			CPRINTS("msg len mismatch.. discard..");
	}
}
