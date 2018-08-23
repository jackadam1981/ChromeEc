/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IPC module for ISH */

/**
 * IPC - Inter Processor Communication
 * -----------------------------------
 *
 * IPC is a bi-directional doorbell based message passing interface sans
 * session and transport layers, between hardware blocks. ISH uses IPC to
 * communicate with the Host, PMC (Power Management Controller), CSME
 * (Converged Security and  Manageability Engine), Audio, Graphics and ISP.
 *
 * Both the initiator and target ends each have a 32-bit doorbell register and
 * 128-byte message regions. In addition, the following register pairs help in
 * synchronizing IPC.
 *
 *  - Peripheral Interrupt Status Register (PISR)
 *  - Peripheral Interrupt Mask Register (PIMR)
 *  - Doorbell Clear Status Register (DB CSR)
 */

#include "registers.h"
#include "console.h"
#include "hooks.h"
#include "lpc.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "ipc.h"
#include "heci_internal.h"
#include "heci_counter.h"

#define CPUTS(outstr) cputs(CC_HOOK, outstr)
#define CPRINTS(format, args...) cprints(CC_HOOK, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_HOOK, format, ## args)

#define true  1
#define false 0
#define HECI_CONN_FLOW_CONTROL_EVENT(conn_id)  (1 << (conn_id))

#define CONN_TIMEOUT 100 /* TODO: how OS tick to us?*/
heci_stat heci_counter = {0};
int ipc_read_wait(uint8_t peer_id, void *out_buff);

/* TODO: check this initialize to 0 */
heci_device_t heci_dev = {
	.ipc_fd             = -1,
	.clients            = {{0}},
	.lock               = {0},
	.conn_disconn_lock  = {0},
	/*.client_ready       = 0,*/
	.connections        = {{0}},
	.stat               = &heci_counter,
	.dma_req            = 0,
	.registered_clients = 0,
	.notify_new_clients = 0,
	.reserved           = 0,
	.dma_pages_bitmap   = 0
};

static heci_ipc_bup_t  heci_msg_frag_from_ipc;

/* TODO: who call this should call callback instead */
#if 0
static void heci_signal_to_client(heci_client_ctrl_t *client, uint32_t event)
{
	/* Wake up the task to handle the event */
	task_set_event(client->task_id, event, 0);
	/*event_flag_signal_handle(
	 client->properties.client_event_flag, event, EVENT_FLAG_OR);*/
}
#endif

#if 1
uint8_t heci_send_flow_control (uint32_t conn_id)
{
	return true;
}

uint8_t heci_send_fragments(heci_conn_t *connection, const mrd_t *msg)
{
	return true;
}

uint8_t heci_send_single_fragment(
	uint8_t host_addr,
	uint8_t ish_addr,
	uint8_t last_fragment,
	uint8_t * data,
	uint32_t length)
{
	return true;
}
/*
 * assumption: FW will not start sending msg IF host has not allocated
 * a buffer to hold the msg - we will not wait
 */
uint8_t heci_send_to_connection(heci_conn_t *connection, const mrd_t *msg)
{
	uint8_t sent = false;
	timestamp_t ts = get_time(); /*st = os_ticks();*/
	mrd_t *msg_to_send = (mrd_t*)msg;
	uint32_t msg_len = 0;
	uint8_t send_over_ipc = true;

	if(connection == NULL || msg == NULL) {
		CPRINTS("heci_send_to_connection: bad parameters %p %p",
			connection, msg);
	    return false;
	}

	/*calc msg size and sending method */
	while (msg_to_send != NULL) {
		msg_len += msg_to_send->length;
		msg_to_send = msg_to_send->next;
	}

	if ((msg_len >= MIN_SIZE_FOR_HECI_OVER_DMA) &&
			(heci_dev.dma_pages_bitmap)) {
		send_over_ipc = false;
	}

	do{
		if ((get_time().val - ts.val) > CONN_TIMEOUT) {
			return false; /* TIMEOUT on waiting for connection to be open.*/
		}

		/*if (Take_LOCK(heci_dev.lock) != OS_OK)
		{
			heci_dev.stat->write_error++;
			HECI_LOG_ERR("lock fail\n");
			return false;
		}*/
		mutex_lock(&heci_dev.lock);

		if (!(connection->state & HECI_CONN_STATE_OPEN))
		{
			mutex_unlock(&heci_dev.lock);
			CPRINTS("heci_send_to_connection() fail");
			/*task_sleep(2);*/ /* 1 tick doesn't always jump to other thread*/
			usleep(2);
			continue; /* Wait for connection state to become OPEN */
		}

		/*unsigned int actual_flags = 0;*/
		/* clean old events */
		/*
		int res = event_flag_reltimedwait_handle(heci_dev.flow_control_event_flag,
			HECI_CONN_FLOW_CONTROL_EVENT(connection->connection_id),
			EVENT_FLAG_OR_CLEAR,
			&actual_flags, 0);
		*/
		if (0 == connection->host_buffers)
		{
			mutex_unlock(&heci_dev.lock);
			CPRINTS("wait for FC from host\n");
			heci_dev.stat->client[connection->connection_id].no_buffers++;


			/* waiting to flow control to arrive from host */
			/*res = event_flag_reltimedwait_handle(heci_dev.flow_control_event_flag,*/
			task_wait_event_mask(
				HECI_CONN_FLOW_CONTROL_EVENT(connection->connection_id),
				1 * SECOND);

			/*res = event_flag_reltimedwait_handle(heci_dev.flow_control_event_flag,
			HECI_CONN_FLOW_CONTROL_EVENT(connection->connection_id),
			EVENT_FLAG_OR_CLEAR, &actual_flags, HECI_SEND_TIMEOUT);*/

			if(0 != connection->host_buffers) {
				//The flow control event was received within the required timeframe.
				break;
			} else {
				CPRINTS("can't send msg to host,no FC\n");
				return false;
			}
		}

		break;

	} while(1);

	if (!send_over_ipc) {
#ifdef DMA_XFER_SUPPORTED
		sent = heci_send_by_dma(connection, msg);
		if (sent) {
			heci_dev.stat->dma_xfer_write++;
		} else {
			heci_dev.stat->dma_xfer_write_error++;
		}
#endif
	}

	if (!sent) {
		sent = heci_send_fragments(connection, msg);
	}

	if (!sent) {
		CPRINTS( "send fail. SHOULD NEVER HAPPEN!\n");
		assert(0);   /* we should never be here! */
	} else {
		connection->host_buffers--; /* decrease FC credit */
	}

	mutex_unlock(&heci_dev.lock);
	return sent;
}

uint8_t heci_send(const  uint32_t handle, const mrd_t *message)
{
	unsigned total_msg_len = 0;
	const mrd_t *mrd_msg;
	uint32_t max_msg_size;

	CPRINTS("send heci message to handle %d", (int)handle);

	if ((handle == HECI_INVALID) ||
		(handle >= HECI_N_OF_ELEMENTS(heci_dev.connections)) ||
		(NULL == message) ||
		(heci_dev.connections[handle].client == NULL)) {
			heci_dev.stat->write_error++;
			CPRINTS("bad send %d, %x\n", (int)handle, (uint32_t)message);
		return false;
	}

	/* make sure total message length is less than HECI_MAX_MSG_SIZE */
	mrd_msg = message;
	max_msg_size = heci_dev.connections[handle].client->properties.max_msg_size;
	while((mrd_msg != NULL) && (total_msg_len <= max_msg_size)) {
		if(mrd_msg->length == 0 || mrd_msg->buffer == NULL) {
			heci_dev.stat->write_error++;
			CPRINTS("invalid mrd list. message = 0x%p, length = %d\n",
				mrd_msg->buffer, mrd_msg->length);
			return false;
		}
		total_msg_len += mrd_msg->length;
		mrd_msg = mrd_msg->next;
	}
	if(total_msg_len > max_msg_size) {
		heci_dev.stat->write_error++;
		CPRINTS("invalid message length %d\n", total_msg_len);
		return false;
	}

	heci_dev.stat->client[handle].total_out_messages++;
	return heci_send_to_connection(&heci_dev.connections[handle], message);
}

heci_rx_msg_t* heci_get_buffer_from_pool (heci_client_ctrl_t* client)
{
	heci_rx_msg_t* msg;
	if(client == NULL) {
		CPRINTS("invd client\n");
		return NULL;
	}

	if (client->properties.rx_buffer_len == 0 ||
		client->properties.max_n_of_connections == 0) {

		CPRINTS("invd client prop: buff_len = %d, max_conn = %d.\n",
			client->properties.rx_buffer_len,
			client->properties.max_n_of_connections);
		return NULL;
	}

	/* calculates the offset of the next buffer in pool in cyclic mode*/
	client->pool_offset = 0;

	/*(client->pool_offset + client->properties.rx_buffer_len) %
		 (client->properties.rx_buffer_len * client->properties.max_n_of_connections);
	*/
	msg = &client->properties.rx_buffers_pool[client->pool_offset];
	if(msg->msg_lock == LOCKED) { /* no free buffer */
		CPRINTS("client %d no free buff\n", client->client_addr);
		return NULL;
	}
	msg->msg_lock = LOCKED;
	return msg;
}

#endif

heci_conn_t *heci_find_conn(uint8_t ish_addr, uint8_t host_addr, uint8_t state)
{
	heci_conn_t *connection;
	int total_conn = HECI_N_OF_ELEMENTS(heci_dev.connections);

	/* Look-up among dynamic address connections */
	connection = heci_dev.connections;
	for (; connection < heci_dev.connections + total_conn; connection++) {
		if ((connection->state & state) &&
			(connection->ish_addr == ish_addr) &&
			(connection->host_addr == host_addr) ) {
			return connection;
		}
	}
	return NULL;
}


/* Notify client about disconnection
 * Caller should take conn_disconn_lock */
void heci_connection_reset(heci_conn_t * connection)
{
	if (NULL == connection || NULL == connection->client) {
		CPRINTS("can't reset conn %d %d\n", connection,
			((connection) ? connection->client : NULL));
		return;
	}

	connection->state = HECI_CONN_STATE_DISCONNECTING;
	/*heci_signal_to_client(connection->client, connection->client->properties.disconnect_event);*/
	connection->client->properties.heci_disconn_cb();
}

/*
 * heci receive bus message: client disconnect request
 * heci_disconnect_req() -> heci_connection_reset()
 * then notify client, client call heci_complete_disconnect()
 * to free resource
 */
int heci_complete_disconnect(uint32_t conn_id, uint32_t *new_conn_id)
{
	*new_conn_id  = HECI_INVALID;
	if (HECI_MAX_NUM_OF_CONNECTIONS <= conn_id) {
		CPRINTS("bad conn id %d", (int)conn_id);
		return -1;
	}

	if (!(HECI_CONN_STATE_DISCONNECTING & heci_dev.connections[conn_id].state)) {
		CPRINTS("disconn conn %d, state 0x%x\n",
			(int)conn_id, heci_dev.connections[conn_id].state);
		return -1;
	}

	CPRINTS("disconn conn %d. host_addr = %d ish_addr = %d.",
		conn_id,
		heci_dev.connections[conn_id].host_addr,
		heci_dev.connections[conn_id].ish_addr);


	/* clean connection rx buffer */
	if(heci_dev.connections[conn_id].rx_buffer != NULL) {
		heci_rx_msg_t *buff = heci_dev.connections[conn_id].rx_buffer;
		buff->type = 0;
		buff->length = 0;
		buff->connection_id = 0;
		buff->msg_lock = 0;
	}

	mutex_lock(&heci_dev.conn_disconn_lock);
	/*if (Take_LOCK(heci_dev.conn_disconn_lock) != OS_OK)
	{
		HECI_LOG_DEBUG("disconn-fail conn_state_lock\n");
		return -1;
	}*/

	if (heci_dev.connections[conn_id].state &
		HECI_CONN_STATE_SEND_DISCONNECT_RESP) {

		/* send a disconnect response to host with the old host_addr */
		heci_disconn_resp_t  disconnect_resp;
		/* build a disconnect response message */
		disconnect_resp.s.command  =
			HECI_BUS_MSG_CLIENT_DISCONNECT_RESP;
		disconnect_resp.s.host_addr =
			heci_dev.connections[conn_id].host_addr;
		disconnect_resp.s.ish_addr =
			heci_dev.connections[conn_id].ish_addr;
		disconnect_resp.s.status = HECI_CONNECT_STATUS_SUCCESS;

		/* send disconnect response message to host */
		heci_send_single_fragment(HECI_DRIVER_ADDRESS,
			HECI_DRIVER_ADDRESS, true,
			(uint8_t *)disconnect_resp.dw,
			sizeof(disconnect_resp));

		heci_dev.connections[conn_id].state &=
			~HECI_CONN_STATE_SEND_DISCONNECT_RESP;
		heci_dev.stat->client[conn_id].disconnect_resp++;
	}

	if (heci_dev.connections[conn_id].state &
			HECI_CONN_STATE_CONNECTION_REQUEST) {
		heci_conn_resp_t conn_resp;
		*new_conn_id = conn_id;
		/* save the new host address */
		heci_dev.connections[conn_id].host_addr =
			heci_dev.connections[conn_id].waiting_connection;
		heci_dev.connections[conn_id].state = HECI_CONN_STATE_OPEN;

		conn_resp.dw[0] = 0;
		conn_resp.s.command = HECI_BUS_MSG_CLIENT_CONNECT_RESP;
		conn_resp.s.ish_addr = heci_dev.connections[conn_id].ish_addr;
		conn_resp.s.host_addr =
			heci_dev.connections[conn_id].host_addr;
		conn_resp.s.status = HECI_CONNECT_STATUS_SUCCESS;
		CPRINTS( "sending connect resp to conn id %d\n", conn_id);
		heci_send_single_fragment(
			HECI_DRIVER_ADDRESS, HECI_DRIVER_ADDRESS,
			true, (uint8_t *) conn_resp.dw, sizeof(conn_resp));
		heci_dev.stat->client[conn_id].connect_resp++;
	} else {
		heci_dev.connections[conn_id].client->n_of_conns --;
		memset(&heci_dev.connections[conn_id], 0, sizeof(heci_conn_t));
	}

	mutex_unlock(&heci_dev.conn_disconn_lock);
	return 0;
}

void heci_version_req(heci_ipc_bup_t *packet, uint32_t length)
{
	heci_version_resp_t ver_response;

	if (sizeof(heci_version_req_t) != length) {
		return;
	}

	ver_response.dw[0] = 0;
	ver_response.s.command   = HECI_BUS_MSG_VERSION_RESP;
	ver_response.s.major_ver = HECI_DRIVER_MAJOR_VERSION;
	ver_response.s.minor_ver = HECI_DRIVER_MINOR_VERSION;
	if( (packet->payload.ver_req.s.major_ver ==
		HECI_DRIVER_MAJOR_VERSION) &&
		(packet->payload.ver_req.s.minor_ver ==
		HECI_DRIVER_MINOR_VERSION)) {
		ver_response.s.supported = 1;
	} else {
		ver_response.s.supported = 0;
	}
	heci_send_single_fragment(HECI_DRIVER_ADDRESS, HECI_DRIVER_ADDRESS,
		true, (uint8_t *) ver_response.dw, sizeof(ver_response));
}

void heci_enum_req(heci_ipc_bup_t *packet, uint32_t length)
{
	heci_host_enum_resp_t enum_resp;
	heci_client_ctrl_t *client;
	uint32_t i;

	if (sizeof(heci_host_enum_req_t) != length)	{
		return;
	}
	memset(&enum_resp, 0, sizeof(enum_resp));
	client = heci_dev.clients;

	/* TODO: why client_addr only 4 bits while host allow 256 address? */
	/* each bit 1/0 indicates that address is assigned to client or not */
	for (i = 0; i < heci_dev.registered_clients; i++, client++) {
		enum_resp.s.valid_addresses[client->client_addr / 32] |=
			 1 << (client->client_addr & (32 - 1));
		client->active = true;
	}
	enum_resp.s.command = HECI_BUS_MSG_HOST_ENUM_RESP;

	heci_send_single_fragment(
		HECI_DRIVER_ADDRESS, HECI_DRIVER_ADDRESS,
		true, (uint8_t *)enum_resp.dw, sizeof(enum_resp));

	/* Assume by now all clients have registered with HECI */
	/* client_req_bits is set means new client can be dynamically added
	after HECI client enumeration procedure */
	if(packet->payload.enum_req.s.client_req_bits) {
		heci_dev.notify_new_clients = true;
	}
}

void heci_client_prop_req(heci_ipc_bup_t *packet, uint32_t length)
{
	uint32_t i;
	heci_client_prop_resp_t prop_resp;

	if ( sizeof(heci_client_prop_req_t) != length) {
		return;
	}

	/* Find the client */
	for (i = 0 ; i < HECI_MAX_NUM_OF_CLIENTS; i++)
	{
		if (heci_dev.clients[i].client_addr ==
				packet->payload.prop_req.s.address) {
			break;
		}
	}
	memset(&prop_resp, 0, sizeof(prop_resp));

	prop_resp.s.command = HECI_BUS_MSG_HOST_CLIENT_PROP_RESP;
	prop_resp.s.address = packet->payload.prop_req.s.address;
	if (i == HECI_MAX_NUM_OF_CLIENTS) {
		prop_resp.s.status = HECI_CONNECT_STATUS_CLIENT_NOT_FOUND;
	} else {
		prop_resp.s.protocol_id =
			heci_dev.clients[i].properties.protocol_id;
		prop_resp.s.status = HECI_CONNECT_STATUS_SUCCESS;
		prop_resp.s.protocol_ver=
			heci_dev.clients[i].properties.protocol_ver;
		prop_resp.s.max_n_of_conns =
			heci_dev.clients[i].properties.max_n_of_connections;
		prop_resp.s.max_msg_size =
			heci_dev.clients[i].properties.max_msg_size;

		prop_resp.s.dma_header_length =
			heci_dev.clients[i].properties.dma_header_length;
		prop_resp.s.dma_enabled =
			heci_dev.clients[i].properties.dma_enabled;

	}
	heci_send_single_fragment(
			HECI_DRIVER_ADDRESS, HECI_DRIVER_ADDRESS,
			true, (uint8_t *)prop_resp.dw, sizeof(prop_resp));
}

void heci_connect_req(heci_ipc_bup_t *packet, uint32_t length)
{
	heci_conn_resp_t conn_resp;
	heci_conn_t *idle_connection = NULL;
	int client_id;
	int conn_id;

	if (NULL == packet) {
		return;
	}

	if (sizeof(heci_conn_req_t) != length) {
		return;
	}

	conn_resp.dw[0] = 0;
	conn_resp.s.command = HECI_BUS_MSG_CLIENT_CONNECT_RESP;
	conn_resp.s.ish_addr = packet->payload.conn_req.s.ish_addr;
	conn_resp.s.host_addr = packet->payload.conn_req.s.host_addr;

	/* Try to find the client */
	conn_resp.s.status = HECI_CONNECT_STATUS_CLIENT_NOT_FOUND;
	for (client_id = 0; client_id < HECI_MAX_NUM_OF_CLIENTS; client_id++) {
		if (heci_dev.clients[client_id].client_addr ==
				packet->payload.conn_req.s.ish_addr) {
			break;
		}
	}

	/* Client wasn't found */
	if(client_id == HECI_MAX_NUM_OF_CLIENTS) {
		CPRINTS("conn-client %d not found",
			packet->payload.conn_req.s.ish_addr);
		goto error;
	}

	/*
	 * Check if it's a dynamic client that the host doesn't
	 * acknowledge with HECI_BUS_MSG_ADD_CLIENT_RESP message
	 */
	if (!heci_dev.clients[client_id].active)	{
		conn_resp.s.status = HECI_CONNECT_STATUS_INACTIVE_CLIENT;
		CPRINTS("client %d is inactive",
			heci_dev.clients[client_id].client_addr);
		goto error;
	}

	CPRINTS("got conn request: host_addr = %d ish_addr = %d",
		packet->payload.conn_req.s.host_addr,
		packet->payload.conn_req.s.ish_addr);

	/* conn_disconn_lock protects connection state */
	/* TODO: what if can't get lock? */
	mutex_lock(&heci_dev.conn_disconn_lock);

	/*
	 * Look-up among existing dynamic address connections
	 * in order to validate the request.
	 */
	for (conn_id = 0; conn_id < HECI_MAX_NUM_OF_CONNECTIONS; conn_id++) {
		/* Check for available IDLE connection */
		if (heci_dev.connections[conn_id].state ==
					HECI_CONN_STATE_UNUSED)	{
			if (idle_connection == NULL) {
				idle_connection =
					&heci_dev.connections[conn_id];
				idle_connection->connection_id = conn_id;
			}
		} else if (heci_dev.connections[conn_id].ish_addr ==
					packet->payload.conn_req.s.ish_addr) {
			CPRINTS("got conn req to existing ish_addr %d",
					packet->payload.conn_req.s.ish_addr);
			if ((heci_dev.connections[conn_id].state &
				HECI_CONN_STATE_DISCONNECTING) &&
				!(heci_dev.connections[conn_id].state &
				HECI_CONN_STATE_CONNECTION_REQUEST)) {

				/*
				 * if a connect request is coming while the
				 * previous disconnect wasn't fully handled
				 * a HECI_CONN_STATE_CONNECT_REQUEST bit is
				 * set and the client will be notified about
				 * this request once heci_complete_disconnect()
				 * will be called.
				 */
				idle_connection =
					&heci_dev.connections[conn_id];
				heci_dev.connections[conn_id].waiting_connection
					= packet->payload.conn_req.s.host_addr;
				idle_connection->state |=
					HECI_CONN_STATE_CONNECTION_REQUEST;
				CPRINTS("got conn before prev disconn req completed");
				mutex_unlock(&heci_dev.conn_disconn_lock);
		                return;
			} else if (heci_dev.connections[conn_id].host_addr ==
					packet->payload.conn_req.s.host_addr) {
				/*
				 * Connection for the same pair of
				 * ISH & Host address already exists
				 */
				conn_resp.s.status =
					HECI_CONNECT_STATUS_ALREADY_EXISTS;
				/*
				 * Clear the indication meaning an available
				 * connection is found
				 */
				idle_connection = NULL;
				CPRINTS("conn pair:ISH(%d) & Host(%d) exists",
					heci_dev.connections[conn_id].ish_addr,
					heci_dev.connections[conn_id].host_addr
					);
				break;
			}
		}
	}
	if (heci_dev.clients[client_id].n_of_conns ==
		heci_dev.clients[client_id].properties.max_n_of_connections) {
		/*
		 * Return error if the client reached the maximum connections
		 * number and it doesn't have a connection in
		 * HECI_CONN_STATE_DISCONNECTING.
		 */
		if ((idle_connection != NULL) &&
			(idle_connection->state == HECI_CONN_STATE_UNUSED)) {
			conn_resp.s.status = HECI_CONNECT_STATUS_REJECTED;
			CPRINTS("conn limit of client %d reached. \
				host_addr = %d ish_addr = %d",
				heci_dev.clients[client_id].client_addr,
				packet->payload.conn_req.s.host_addr,
				packet->payload.conn_req.s.ish_addr);
			idle_connection = NULL;
		}
	}

	/* Idle connection is found */
	if (idle_connection != NULL) {
		if (idle_connection->state == HECI_CONN_STATE_UNUSED) {
			idle_connection->rx_buffer =
				heci_get_buffer_from_pool(
					&heci_dev.clients[client_id]);

			/*
			 * every connection saves its current rx buffer
			 * in order to free it after the client will read
			 * the content.
			 */
			if (idle_connection->rx_buffer == NULL)	{
				conn_resp.s.status =
					HECI_CONNECT_STATUS_REJECTED;
			} else {
				idle_connection->client =
					&heci_dev.clients[client_id] ;
				idle_connection->host_addr =
					packet->payload.conn_req.s.host_addr;
				idle_connection->ish_addr =
					packet->payload.conn_req.s.ish_addr;
				idle_connection->state =
					HECI_CONN_STATE_OPEN;

				/* send connection handle to client */
				idle_connection->rx_buffer->type =
					HECI_CONNECT;
				idle_connection->rx_buffer->connection_id =
					idle_connection->connection_id;
				idle_connection->rx_buffer->length = 0;
				heci_dev.clients[client_id].n_of_conns++;
			}
		}

		/* Send response to host */
		conn_resp.s.status = HECI_CONNECT_STATUS_SUCCESS;
		CPRINTS("sending conn resp to client %d, sts %d",
			client_id, conn_resp.s.status);

		heci_send_single_fragment(
			HECI_DRIVER_ADDRESS, HECI_DRIVER_ADDRESS,
			true, (uint8_t *) conn_resp.dw, sizeof(conn_resp));

		heci_dev.stat->client[idle_connection->connection_id].connect_resp++;

		/* Signal to client, in case it isn't in disconnect process */
		/* TODO: instead of using event notification, call callback */
		if (idle_connection->state == HECI_CONN_STATE_OPEN) {
			/*
			heci_signal_to_client(
				&heci_dev.clients[client_id],
			heci_dev.clients[client_id].properties.new_msg_event);*/
			heci_dev.clients[client_id].properties.heci_msg_cb();
		}
		mutex_unlock(&heci_dev.conn_disconn_lock);
		return;
	}
	mutex_unlock(&heci_dev.conn_disconn_lock);

error: /* send response to host with failure status */
	CPRINTS("sending conn resp to client %d, sts %d",
		client_id, conn_resp.s.status);
	heci_send_single_fragment(HECI_DRIVER_ADDRESS, HECI_DRIVER_ADDRESS,
		true, (uint8_t *) conn_resp.dw, sizeof(conn_resp));
}

/*
 * HECI_BUS_MSG_CLIENT_DISCONNECT_REQ 0x7
 * Host request to end connection
 * the sender should process incoming messages as usual until
 * Disconnect Response is received.
 */
void heci_disconnect_req(heci_ipc_bup_t *packet, uint32_t length)
{
	heci_conn_t *connection;
	heci_disconn_resp_t  disconnect_resp;

	if (sizeof(heci_disconn_req_t) != length) {
		CPRINTS("heci_disconn_req: invd pkt len");
		return;
	}

	/*
	 * Look-up for a connection in either HECI_CONN_STATE_OPEN state
	 * or HECI_CONN_STATE_CONNECTION_REQUEST state
	 */
	connection = heci_find_conn(
		packet->payload.flow_control.s.ish_addr,
		packet->payload.flow_control.s.host_addr,
		HECI_CONN_STATE_OPEN | HECI_CONN_STATE_CONNECTION_REQUEST);
	if (NULL != connection)	{
		mutex_lock(&heci_dev.conn_disconn_lock);

		if (connection->state & HECI_CONN_STATE_DISCONNECTING) {
			/* In disconnecting, no need to signal to client.*/
			connection->state = HECI_CONN_STATE_DISCONNECTING;
		} else {
			heci_connection_reset(connection);
		}

		connection->state |= HECI_CONN_STATE_SEND_DISCONNECT_RESP;
		mutex_unlock(&heci_dev.conn_disconn_lock);

		CPRINTS("got disconn req. host_addr = %d ish_addr = %d\n",
			packet->payload.flow_control.s.host_addr,
			packet->payload.flow_control.s.ish_addr);
		return;
	}

	CPRINTS("invd disconn req-host_addr = %d ish_addr = %d\n",
		packet->payload.flow_control.s.host_addr,
		packet->payload.flow_control.s.ish_addr);

	/* send disconnection response to host with error status */
	disconnect_resp.s.command  = HECI_BUS_MSG_CLIENT_DISCONNECT_RESP;
	disconnect_resp.s.host_addr = packet->payload.flow_control.s.host_addr;
	disconnect_resp.s.ish_addr = packet->payload.flow_control.s.ish_addr;
	disconnect_resp.s.status = HECI_CONNECT_STATUS_CLIENT_NOT_FOUND;

	heci_send_single_fragment(HECI_DRIVER_ADDRESS, HECI_DRIVER_ADDRESS,
		true, (uint8_t *)disconnect_resp.dw, sizeof(disconnect_resp));
}

void heci_flow_control_recv(heci_ipc_bup_t *packet, uint32_t length)
{
	heci_conn_t *connection;

	if (sizeof(heci_flow_ctrl_t) != length)	{
		return;
	}

	/* find connection */
	connection = heci_find_conn(
		packet->payload.flow_control.s.ish_addr,
		packet->payload.flow_control.s.host_addr,
		HECI_CONN_STATE_OPEN);
	if (NULL != connection) {
	    heci_dev.stat->client[connection->connection_id].recv_flow_ctrl++;
		if (packet->payload.flow_control.s.number_of_packets == 0) {
			connection->host_buffers++;
		} else {
			connection->host_buffers +=
				packet->payload.flow_control.s.number_of_packets;
		}
		/* TODO: heci_send_to_connection() notify flow ctrl msg. */
		/* whoever waiting for flow control, not HECI_TX */
		/*task_set_event(
			TASK_ID_HECI_TX,
			HECI_CONN_FLOW_CONTROL_EVENT(connection->connection_id),
			0);*/

		/* event_flag_signal_handle(heci_dev.flow_control_event_flag,
		 HECI_CONN_FLOW_CONTROL_EVENT(connection->connection_id), EVENT_FLAG_OR);*/
	}
}

/* TODO: Client connection reset, why didn't do below */
/*
 * Stop any in progress large message operation
 * Singal client connection is reset
 * Wait for client clean up
 * Reset flow control credit count for this connection
 * HECI send reset resp message
 * Host HECI reset its flow control credit.
 */
void  heci_reset_req(heci_ipc_bup_t * packet, uint32_t length)
{
	heci_conn_t *connection;
	heci_reset_resp_t reset_respones;

	if (sizeof(heci_reset_req_t) != length) {
		return;
	}

	connection = heci_find_conn(
		packet->payload.flow_control.s.ish_addr,
		packet->payload.flow_control.s.host_addr,
		HECI_CONN_STATE_OPEN);

	/* Find connection */
	if (NULL != connection) {
		connection->host_buffers = 0;
		reset_respones.s.command  = HECI_BUS_MSG_RESET_RESP;
		reset_respones.s.host_addr =
			packet->payload.reset_req.s.host_addr;
		reset_respones.s.ish_addr  =
			packet->payload.reset_req.s.ish_addr;
		reset_respones.s.status   = HECI_CONNECT_STATUS_SUCCESS;
		heci_send_single_fragment(
				HECI_DRIVER_ADDRESS,
				HECI_DRIVER_ADDRESS, true,
				(uint8_t *)reset_respones.dw,
				sizeof(reset_respones) );
	}
	/* Ignore message for non-existing connection/inappropriate state */
}

/* TODO: do we support add client msg */
void heci_add_client_resp(heci_ipc_bup_t *packet, uint32_t length)
{
	heci_add_client_resp_t resp;
	int i;

	if (packet == NULL || sizeof(heci_add_client_resp_t) != length) {
		CPRINTS("bad add_client_resp pkt");
		return;
	}

	resp = packet->payload.new_client_resp;
	if (resp.status != 0) {
		CPRINTS("can't activate client with resp sts %d", resp.status);
		return;
	}

	for (i = 0 ; i < HECI_MAX_NUM_OF_CLIENTS; i++) {
		if (heci_dev.clients[i].client_addr == resp.client_addr) {
			heci_dev.clients[i].active = 1;
			CPRINTS("activate client %d", resp.client_addr);
			return;
		}
	}

	CPRINTS("can't activate client %d -not found.", resp.client_addr);
}

void heci_dma_req(heci_ipc_bup_t * packet, uint32_t length)
{
	heci_conn_t *connection;
	heci_dma_resp_t resp;
	mrd_t mrd_msg = {0};

	if (packet == NULL || sizeof(heci_dma_req_t) > length ||
		packet->payload.dma_req.s.preview_length !=
		length - sizeof(heci_dma_req_t)) {
		CPRINTS("illegal dma req: len = %d", length);
		return;
	}

	/* Look-up connection */
	connection = heci_find_conn(
		packet->payload.dma_req.s.ish_addr,
		packet->payload.dma_req.s.host_addr,
		HECI_CONN_STATE_OPEN);

	if (NULL == connection)
		return;

	if (packet->payload.dma_req.s.length <= HECI_MAX_DMA_SIZE &&
		packet->payload.dma_req.s.length >= HECI_MIN_DMA_SIZE &&
		HECI_DMA_MAX_PREVIEW >=
		packet->payload.dma_req.s.preview_length) {
		if (NULL != connection->client)	{
			if (connection->client->properties.dma_header_length ==
				packet->payload.dma_req.s.preview_length &&
				connection->client->properties.dma_enabled) {
				heci_rx_dma_msg_t *msg;

				CPRINTS("send DMA to client");
				connection->rx_buffer =
					heci_get_buffer_from_pool(
						connection->client);

				if(connection->rx_buffer == NULL) {
					CPRINTS("conn %d no free rx buff\n",
						connection->ish_addr);
					return;
				}

				msg = (heci_rx_dma_msg_t*)connection->rx_buffer;
				msg->heci_rx_msg.type = HECI_RX_DMA_MSG;
				msg->heci_rx_msg.length = sizeof(heci_rx_dma_msg_t) +
					packet->payload.dma_req.s.preview_length
					- sizeof(heci_rx_msg_t);
				msg->preview_len =
					packet->payload.dma_req.s.preview_length;
				memcpy_s(msg->preview,
					connection->client->properties.rx_buffer_len
						- sizeof(*msg),
					packet->payload.dma_req.s.preview,
					packet->payload.dma_req.s.preview_length);

				connection->dma_buff_size = msg->buf_len =
					packet->payload.dma_req.s.length;
				memcpy_s(connection->host_dram_addr,
					sizeof(connection->host_dram_addr),
					packet->payload.dma_req.s.host_dram_addr,
					sizeof(connection->host_dram_addr));
				CPRINTS("heci dram addr:%x",
					(unsigned int)(connection->host_dram_addr));
				/* TODO: what's this dma_ts? */
				connection->dma_ts = get_time(); /*os_ticks();*/
				/*heci_signal_to_client(connection->client,
					connection->client->properties.new_msg_event);*/
				connection->client->properties.heci_msg_cb();
				heci_dev.dma_req = true;
				return;
			}
		}
	}
	CPRINTS("bad dma req %u", connection->dma_buff_size);
	resp.s.command = HECI_BUS_MSG_DMA_RESP;
	resp.s.host_addr = packet->payload.dma_req.s.host_addr;
	resp.s.ish_addr = packet->payload.dma_req.s.ish_addr;
	resp.s.length = packet->payload.dma_req.s.length;
	resp.s.status = 2;

	mrd_msg.buffer = &resp;
	mrd_msg.length = sizeof(resp);
	heci_send_to_connection(connection, &mrd_msg);

}

/*
 * Process HECI bus messages
 * @param packet: pointer to bus message in heci_ipc_bup_t format
 * @param length: payload length of bus message
 * @return none
 */
void heci_process_driver_packet(heci_ipc_bup_t *packet, uint32_t length)
{

	CPRINTS("heci req %d", packet->payload.conn_req.s.command);
	heci_dev.stat->driver_packets++;

	switch (packet->payload.conn_req.s.command) {
	case HECI_BUS_MSG_VERSION_REQ:
		heci_version_req(packet, length);
		break;

	case HECI_BUS_MSG_HOST_ENUM_REQ:
		heci_enum_req(packet, length);
		break;

	case HECI_BUS_MSG_HOST_CLIENT_PROP_REQ:
		heci_client_prop_req(packet, length);
		break;

	case HECI_BUS_MSG_CLIENT_CONNECT_REQ:
		heci_dev.stat->connect_req++;
		heci_connect_req(packet, length);
		break;

	case HECI_BUS_MSG_CLIENT_DISCONNECT_REQ:
		heci_dev.stat->disconnect_req++;
		heci_disconnect_req(packet, length);
		break;

	case HECI_BUS_MSG_FLOW_CONTROL:
		heci_flow_control_recv(packet, length);
		break;

	case HECI_BUS_MSG_RESET_REQ:
		heci_dev.stat->ipc_reset++;
		heci_reset_req(packet, length);
		break;

	case HECI_BUS_MSG_DMA_REQ:
		heci_dma_req(packet, length);
		break;

	case HECI_BUS_MSG_ADD_CLIENT_RESP:
		heci_dev.stat->new_client_response++;
		heci_add_client_resp(packet, length);
		break;
#ifdef DMA_XFER_SUPPORTED
	case HECI_BUS_MSG_DMA_ALLOC_NOTIFY: // host notification for DMA buffer allocation
		heci_dma_alloc_notification_recieved(packet, length);
		break;

	case HECI_BUS_MSG_DMA_XFER_REQ: // DMA transfer to FW
		heci_dma_xfer_recieved(packet, length);
		break;

	case HECI_BUS_MSG_DMA_XFER_RESP: // Ack for DMA transfer from FW
		heci_dma_xfer_ack_recieved(packet, length);
		break;
#endif
	default:
		break;
	}
}

void heci_process_client_packet (
	heci_ipc_bup_t * ipc_packet,
	heci_bus_dma_xfer_req_t* dma_xfer_packet,
	uint32_t length)
{
	/* TODO: fill in */
}

void heci_process_sys_state_packet (void * packet, uint32_t length)
{
	/* TODO: fill in */
}

/*
 * Distribute messages to proper recepeint
 *
 * @param packet: pointer to received packet in heci_ipc_bup_t format
 * @param length: total length of packet
 * @return none
 */
void heci_process_packet(heci_ipc_bup_t *packet, uint32_t length)
{

	/* Error checking*/
	if (NULL == packet) {
		/* TODO: heci_dev.stat->error++;*/
		CPRINTS("heci: bad pkt");
		return;
	}
	if (packet->hdr.hdr.length + sizeof(packet->hdr) != length) {
		/*TODO: heci_dev.stat->error++;*/
		CPRINTS("heci: bad len/pkt %d != %d\n",
				(int)length,
				(int)(packet->hdr.hdr.length + sizeof(packet->hdr)));
		return;
	}

	length -= sizeof(packet->hdr);
	mutex_lock(&heci_dev.lock);

	/* heci bus message */
	if( packet->hdr.hdr.ish_addr == HECI_DRIVER_ADDRESS ) {
		heci_process_driver_packet(packet, length);
	} else if( packet->hdr.hdr.ish_addr == HECI_SYSTEM_STATE_CLIENT_ADDR) {
		/* System state message */
		heci_process_sys_state_packet((void *)&packet->payload, length);
	} else {
		/* Message to heci client */
		heci_process_client_packet (packet, NULL, length );
	}

	mutex_unlock(&heci_dev.lock);
}

static void heci_init(void)
{
	CPRINTS("heci_init");
}

DECLARE_HOOK(HOOK_INIT, heci_init, HOOK_PRIO_INIT_LPC);

/* Task that listens for incomming IPC messages from Host and initiate
 * heci RX message processing.
 */
void heci_rx_task(void)
{
	/*int ret = 0;*/
	/*uint32_t out_drbl;*/
	uint32_t pkt_len;

	/* TODO: status = Take_EVENT(heci_dev.client_ready)? */

	/* TODO: ipc_open() */

	for (;;) {
		/* task will be blocked here, waiting for event */
		pkt_len = ipc_read_wait(IPC_PEER_HOST_ID, &heci_msg_frag_from_ipc);

		if (pkt_len > 0) {
			if (heci_msg_frag_from_ipc.hdr.hdr.length +
				/*sizeof(heci_msg_frag_from_ipc.hdr.hdr) == pkt_len) {*/
				sizeof(heci_msg_frag_from_ipc.hdr) == pkt_len) {
				heci_process_packet(&heci_msg_frag_from_ipc, pkt_len);
			} else {
				/* TODO: error */
				/*heci_dev.stat->error++;*/
				/*CPRINTS("msg len mismatch %d != %d\n", (int)pkt_len,
						(int) (heci_msg_frag_from_ipc.hdr.hdr.length
							 + sizeof(heci_msg_frag_from_ipc.hdr)));*/
			}
		}
#if 0
else if (len == -ECONNRESET) {
			heci_dev.stat->ipc_reset++;
			HECI_LOG_DEBUG( "got heci reset\n");
			heci_reset();
		}
		if (heci_dev.dma_req) {
			HECI_LOG_DEBUG( "heci dma clear\n");
			heci_dma_clear();
			if (heci_dev.dma_req) {
				timeout = HECI_DMA_TIMEOUT;
				if (ipc_ioctl(IPC_IF_HOST, IPC_SET_RD_TIMEOUT_IOCTL, &timeout,
						sizeof(timeout)) != 0) {
					DBG_ASSERT(0);
				}

			} else {
				timeout = -1;
				if (ipc_ioctl(IPC_IF_HOST, IPC_SET_RD_TIMEOUT_IOCTL, &timeout,
						sizeof(timeout)) != 0) {
					DBG_ASSERT(0);
				}
			}
		}
#endif

	}
}
