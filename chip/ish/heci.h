#ifndef __HECI_H
#define __HECI_H

/* TODO: header cleanup */
#include <string.h>
/*#include <event_flag_defs.h> */
#include <ish_types.h>
#include "task.h"
#include "timer.h"

typedef uint8_t heci_rx_msg_type;

#pragma pack(1)
typedef struct {
	heci_rx_msg_type    type;
	uint16_t            length;
	uint8_t             connection_id    : 7;
	uint8_t             msg_lock         : 1;
	uint8_t             *buffer;
} heci_rx_msg_t;

typedef struct {
	heci_rx_msg_t       heci_rx_msg;
	uint32_t            buf_len;
	uint32_t            preview_len;
	uint8_t             preview[];
} heci_rx_dma_msg_t;
#pragma pack()

typedef uint8_t heci_rx_msg_type;
typedef enum
{
	HECI_MSG_BASE,
	HECI_REQUEST = HECI_MSG_BASE,
	HECI_CONNECT,
	HECI_DISCONNECT,
	HECI_RX_DMA_MSG,
	HECI_SYNC_RESP,
	HECI_MSG_LAST,
} MSG_TYPE;

/* buffer state of heci request */
#define LOCKED 1
#define UNLOCKED 0

/* HECI clients RX buffers must be alligned to 64 to support DMA interfarce */
#define _64_ALIGNED_  __attribute__((aligned(64)))
#define ALIGN_MASK_64 0x3f

#define PAGE_SIZE 4096
#define HECI_MAX_MSG_SIZE 4096

/*((DMA_ALIGN_SIZE-1) + (DMA_ALIGN_SIZE-1))*/
#define HECI_MAX_ALLIGNMENT_EDGES_FOR_DMA 126
#define HECI_CLIENT_MAX_TX_MSG_SIZE \
	(HECI_MAX_MSG_SIZE - HECI_MAX_ALLIGNMENT_EDGES_FOR_DMA)
#define HECI_DMA_MSG_PAGE_SIZE 4096

/*
 * Define number of HECI clients
 * List of all clients:
 * smhi_client, loader_client, vk_client, hid_client,
 * trace_collect_client, trace_config_client, gdb_client
 * Notes: loader_client runs in smhi_client's task.
 */
#define KERNEL_HECI_CLIENTS 3  /* GDB, trace config, trace collector */
#define ISH_CONFIG_ADDITIONAL_HECI_CLIENTS 1 /* HID */
#define HECI_MAX_NUM_OF_CLIENTS \
	(ISH_CONFIG_ADDITIONAL_HECI_CLIENTS +  KERNEL_HECI_CLIENTS)
#define HECI_WAIT_FOR_CLIENTS_NUM HECI_MAX_NUM_OF_CLIENTS

#define HECI_INVALID -1
#define HECI_SEND_TIMEOUT 1000 /* 1 sec */
#define HECI_DMA_FAIL 1

/*
 * ISH2: HECI is blocked until all clients register before
 * connecting to host over IPC.
 * ISH3: need to change heci_task to wait  for N first clients
 * where N < HECI_MAX_NUM_OF_CONNECTIONS!!!
 */
#define HECI_MAX_NUM_OF_CONNECTIONS HECI_MAX_NUM_OF_CLIENTS
#define HECI_DRIVER_ADDRESS 0

#define HECI_SYSTEM_STATE_CLIENT_ADDR 13
/**
*****************************************************************************
* @ingroup HECI
*     HECI Client GUID
*
* @version
*            Supported from ISH2
*
*****************************************************************************/
typedef struct {
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	uint8_t  data4[8];
} heci_guid_t;

/**
*****************************************************************************
* @ingroup HECI
*     HECI return values
*
* @version
*          Supported from ISH2
*
*****************************************************************************/
typedef enum {
	HECI_REGISTER_SUCCEED,
	HECI_REGISTER_OUT_OF_RESOURCES,
	HECI_REGISTER_ALREADY_REGISTERED,
	HECI_REGISTER_INTERNAL_ERROR,
	HECI_REGISTER_NOT_READY
} HECI_REGISTER_STATUS;

typedef struct {
	uint32_t  buf_low_phy_addr;
	uint32_t  buf_high_phy_addr;
} heci_dma_buffer_t;

typedef struct {
	/* A 16-byte identifier for the protocol supported by the client. */
	heci_guid_t protocol_id;
	uint32_t    max_msg_size;
	uint8_t     protocol_ver;
	uint8_t     max_n_of_connections;
	uint8_t     dma_header_length :7;
	uint8_t     dma_enabled :1;
	/* event to signal that there is a new message from host */
	/*uint32_t    new_msg_event;*/
	void (*heci_msg_cb)(void); /* new HECI msg received callback */

	/* event to signal on disconnect */
	/*uint32_t    disconnect_event;*/
	void (*heci_disconn_cb)(void); /* connection disconnect callback */

	/* Single buffer length, should be max client message
	 * size (including client header) + heci header size */
	uint32_t    rx_buffer_len;

	/* The pool length must be equal to
	 * max_n_of_connections * rx_buffer_len */
	heci_rx_msg_t* rx_buffers_pool;
} heci_client_t;

/**
*****************************************************************************
* @ingroup HECI
*  HECI register client function, registers a new HECI client to HECI service.
*  When a new message for the client arrives from host:
*  1. HECI looks for a free rx buffer in the client pool buffers,
*     marks it as busy and stores the msg there
*  2. HECI sends event to client that there is a new message from the host.
*  3. Once the client completes its work with the buffer, he must send a
*     flow control to host to release the rx buffer
*
* @param[in] Client - client properties.
**
* @return
*      - 0 - if successful.
*      - HECI_REGISTER_OUT_OF_RESOURCES - out of resources
*      - HECI_REGISTER_ALREADY_REGISTERED - client already registered
*      - HECI_REGISTER_INTERNAL_ERROR - internal error
*
* @version
*      Supported from ISH2
*****************************************************************************/
HECI_REGISTER_STATUS heci_register(const heci_client_t *Client);

/**
*****************************************************************************
* @ingroup HECI
*  HECI send function. Sends a message to host.
*
* @param[in] handle - a handle to connection.
* @param[in] message - a message to be sent.
* @param[in] message_len - a message length.
* @param[in] timeout - a sending timeout to wait the message to be sent.
**
* @return
*      - true - if successful.
*      - false - if failure
*
* @version
*      Supported from ISH2
*****************************************************************************/
uint8_t heci_send(const uint32_t handle, const mrd_t *message);

/**
*****************************************************************************
* @ingroup HECI
*  HECI send flow control function. Sends a flow control to host to host.
*
* @param[in] conn_id - a connection ID which to send a flow control message to.
**
* @return
*      - true - if successful.
*      - false - if failure
*
* @version
*      Supported from ISH2
*****************************************************************************/
uint8_t heci_send_flow_control(uint32_t conn_id);

/**
*****************************************************************************
* @ingroup HECI
*  HECI get dma request function. This function is called by client after
*  receiving HECI_RX_DMA_REQ msg from heci driver.
*  The client supplies a 8 bytes physical address of a buffer that will
*  receive the DMA msg.
*
* @param[in] conn_id - a connection to send a flow control message to.
**
* @return
*      - true - if the DMA operation has succeeded and the buffer contains the msg.
*      - false - if failure
*
* @version
*      Supported from ISH2
*****************************************************************************/
uint8_t heci_dma_get(uint32_t handle, const uint64_t buffer, uint32_t buffer_size);

/**
*****************************************************************************
* @ingroup HECI
*  heci_complete_disconnect frees connection resources following a
*  disconnect request from the host.
*  This function must be called by the client after a disconnect
*  event flag was set.
*
* @param[in] conn_id - the disconnected connection id
* @param[out] new_conn_id - if a connect request arrived while the previous
*                           disconnect wasn't fully handled, the new
*                           connection id will be saved to new_conn_id .
*
* @return
*      - 0 - if successful.
*      --1 - if failure
*
* @version
*      Supported from ISH2
*****************************************************************************/
int heci_complete_disconnect(uint32_t disconnected_conn_id, uint32_t* new_conn_id);

uint8_t heci_send_single_fragment(
	uint8_t host_addr,
	uint8_t ish_addr,
	uint8_t last_fragment,
	uint8_t * data,
	uint32_t length);

void memcpy_s(void *dest, uint32_t dmax, const void *src, uint32_t len);
#endif /* __HECI_H */
