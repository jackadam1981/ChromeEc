#ifndef __HECI_INTERNAL_H
#define __HECI_INTERNAL_H

#include <heci.h>
#include <heci_counter.h>
/*#define DMA_XFER_SUPPORTED*/

#define HECI_IPC_WRITE_TIMEOUT 10000  /* 10 sec(since simulators are slow) */
#define HECI_DMA_TIMEOUT 10000  /* 10 sec (since simulators are slow) */

#define HECI_SEND_RETRIES 10
#define HECI_REGISTER_TIMEOUT 10

#define HECI_DMA_MAX_PREVIEW 30

#define HECI_MAX_DMA_SIZE (PAGE_SIZE * 200)
#define HECI_MIN_DMA_SIZE (512)

/* Send HECI message over DMA if size >=  4 IPC messages */
#define MIN_SIZE_FOR_HECI_OVER_DMA 512

#define NUM_BITS_IN_DWORD 32
#define INDEX32_TO_BITMASK(a) ((uint32_t)0x1 << ((a) % 32))
#define INDEX32_TO_BITMASK_OFFSET(a) ((a)/32)

#define HECI_IPC_PACKET_SIZE 128
#define HECI_MAX_PAYLOAD_SIZE (HECI_IPC_PACKET_SIZE - sizeof(heci_hdr_t))

#define HECI_DRIVER_MAJOR_VERSION                1
#ifdef HECI_DMA
#define HECI_DRIVER_MINOR_VERSION                2
#else
#define HECI_DRIVER_MINOR_VERSION                0
#endif
#define HECI_DRIVER_MINOR_VERSION_LEGACY         0

#define HECI_BUS_MSG_VERSION_REQ                 0x01
#define HECI_BUS_MSG_VERSION_RESP                (0x80 | HECI_BUS_MSG_VERSION_REQ)
#define HECI_BUS_MSG_HOST_STOP_REQ               0x02
#define HECI_BUS_MSG_HOST_STOP_RESP              (0x80 | HECI_BUS_MSG_HOST_STOP_REQ)
#define HECI_BUS_MSG_ME_STOP_REQ                 0x03
#define HECI_BUS_MSG_HOST_ENUM_REQ               0x04
#define HECI_BUS_MSG_HOST_ENUM_RESP              (0x80 | HECI_BUS_MSG_HOST_ENUM_REQ)
#define HECI_BUS_MSG_HOST_CLIENT_PROP_REQ        0x05
#define HECI_BUS_MSG_HOST_CLIENT_PROP_RESP       (0x80 | HECI_BUS_MSG_HOST_CLIENT_PROP_REQ)
#define HECI_BUS_MSG_CLIENT_CONNECT_REQ          0x06
#define HECI_BUS_MSG_CLIENT_CONNECT_RESP         (0x80 | HECI_BUS_MSG_CLIENT_CONNECT_REQ)
#define HECI_BUS_MSG_CLIENT_DISCONNECT_REQ       0x07
#define HECI_BUS_MSG_CLIENT_DISCONNECT_RESP      (0x80 | HECI_BUS_MSG_CLIENT_DISCONNECT_REQ)
#define HECI_BUS_MSG_FLOW_CONTROL                0x08
#define HECI_BUS_MSG_RESET_REQ                   0x09
#define HECI_BUS_MSG_RESET_RESP                  (0x80 | HECI_BUS_MSG_RESET_REQ)
#define HECI_BUS_MSG_ADD_CLIENT_REQ				 0x0A
#define HECI_BUS_MSG_ADD_CLIENT_RESP			 (0x80 | HECI_BUS_MSG_ADD_CLIENT_REQ)
#define HECI_BUS_MSG_DMA_REQ                     0x10
#define HECI_BUS_MSG_DMA_RESP                    (0x80 | HECI_BUS_MSG_DMA_REQ)
#define HECI_BUS_MSG_DMA_ALLOC_NOTIFY            0x11
#define HECI_BUS_MSG_DMA_ALLOC_RESP              (0x80 | HECI_BUS_MSG_DMA_ALLOC_NOTIFY)
#define HECI_BUS_MSG_DMA_XFER_REQ                0x12
#define HECI_BUS_MSG_DMA_XFER_RESP               (0x80 | HECI_BUS_MSG_DMA_XFER_REQ)

#define HECI_N_OF_ELEMENTS(x) ARRAY_SIZE(x)

typedef struct {
	uint16_t client_addr :4;
	uint16_t n_of_conns  :4;
	uint16_t active	     :1;
	uint16_t reserved    :7;
	uint8_t  pool_offset;
	uint8_t  *buffer;
	heci_client_t properties;
} heci_client_ctrl_t;

typedef struct {
	heci_client_ctrl_t *client;
	uint8_t state;
	uint8_t ish_addr;
	uint8_t host_addr;
	uint8_t waiting_connection; /* host_addresss of a new connection
				     *  request that waits the client to
				     *  complete its work with the previous
				     *  connection
				     */
	heci_rx_msg_t *rx_buffer;   /* every connection saves its current rx
				     * buffer in order to free after the client
				     * reads the content.
				     */
	uint32_t host_buffers;
	uint8_t  host_dram_addr[8];
	uint32_t dma_buff_size;
	timestamp_t dma_ts; /* DMA timestamp */
	uint8_t  connection_id;
} heci_conn_t;

typedef struct {
	uint32_t buf_size;    /* Allocated size */
	uint64_t buf_address; /* The address in host memory for client */
			      /* subsequent DMA messages. [0:11] must be 0 */
} dma_buf_info_t;

typedef struct {
	int ipc_fd;
	heci_client_ctrl_t   clients[HECI_MAX_NUM_OF_CLIENTS];
	struct mutex lock;
	struct mutex conn_disconn_lock; /* protects connections.state in all
					 connect\disconnect flows */
	/* EVENT_TYPE          client_ready; */ /* TODO */
	heci_conn_t connections[HECI_MAX_NUM_OF_CONNECTIONS];
	/*event_flag_handle_t flow_control_event_flag;*/ /* TODO */
	heci_stat *stat; /* power management */
	uint8_t dma_req            : 1;
	uint8_t registered_clients : 4;
	uint8_t notify_new_clients : 1;
	uint8_t reserved : 2;
	dma_buf_info_t tx_host_dma_buffer; /* DMA buffer allocated by host */
	uint32_t num_of_tx_dma_pages;
	uint32_t *dma_pages_bitmap; /* DMA memory pages */
} heci_device_t;

typedef enum {
	HECI_CONN_STATE_UNUSED                = 0,
	HECI_CONN_STATE_OPEN                  = (1 << 0),
	HECI_CONN_STATE_PROCESSING_MSG        = (1 << 1),
	HECI_CONN_STATE_DISCONNECTING         = (1 << 2),
	HECI_CONN_STATE_CONNECTION_REQUEST    = (1 << 3),
	HECI_CONN_STATE_SEND_DISCONNECT_RESP  = (1 << 4),
} HECI_CONN_STATE;

typedef enum {
	HECI_CONNECT_STATUS_SUCCESS           = 0,
	HECI_CONNECT_STATUS_CLIENT_NOT_FOUND  = 1,
	HECI_CONNECT_STATUS_ALREADY_EXISTS    = 2,
	HECI_CONNECT_STATUS_REJECTED          = 3,
	HECI_CONNECT_STATUS_INVALID_PARAMETER = 4,
	HECI_CONNECT_STATUS_INACTIVE_CLIENT   = 5,
} HECI_BUS_MSG_STATUS;

#pragma pack(1)
typedef union {
	uint32_t h;
	struct
	{
		uint32_t  ish_addr  : 8;
		uint32_t  host_addr : 8;
		uint32_t  length    : 9;
		uint32_t  reserved  : 5;
		uint32_t  secure    : 1;
		uint32_t  last_frg  : 1;
	} hdr;
} heci_hdr_t;

typedef union {
	struct
	{
		uint8_t  command; /* = HECI_BUS_MSG_DMA_REQ */
		uint8_t  ish_addr;
		uint8_t  host_addr;
		uint8_t  reserved;
		uint8_t  host_dram_addr[8];
		uint32_t length;
		uint16_t reserved2;
		uint16_t preview_length;
		int8_t   preview[];
	} s;
	uint32_t dw[5];
} heci_dma_req_t;


typedef union {
	struct
	{
		uint8_t  command; /* = HECI_BUS_MSG_DMA_RESP */
		uint8_t  ish_addr;
		uint8_t  host_addr;
		uint8_t  status;
		uint8_t  host_dram_addr[8];
		uint32_t length;
	} s;
	uint32_t dw[4];
} heci_dma_resp_t;


typedef union {
	struct
	{
		uint8_t command; /* = HECI_BUS_MSG_VERSION_REQ */
		uint8_t reserved;
		uint8_t minor_ver;
		uint8_t major_ver;
	} s;
	uint32_t dw[1];
} heci_version_req_t;

typedef union {
	struct
	{
		uint8_t command; /* = HECI_BUS_MSG_VERSION_RESP */
		uint8_t supported;
		uint8_t minor_ver;
		uint8_t major_ver;
	} s;
	uint32_t dw[1];
} heci_version_resp_t;

typedef union {
	struct
	{
		uint8_t command; /* = HECI_BUS_MSG_HOST_STOP_REQ */
		uint8_t reason;
		uint8_t reserved[2];
	} s;
	uint32_t dw[1];
} heci_host_stop_req_t;

typedef union {
	struct
	{
		uint8_t command; /* = HECI_BUS_MSG_HOST_STOP_RESP */
		uint8_t reserved[3];
	} s;
	uint32_t dw[1];
} heci_host_stop_resp_t;

typedef union {
	struct
	{
		uint8_t command; /* = HECI_BUS_MSG_HOST_ENUM_REQ */
		uint8_t client_req_bits;
		uint8_t reserved[2];
	} s;
	uint32_t dw[1];
} heci_host_enum_req_t;

typedef union {
	struct
	{
		uint8_t  command; /* = HECI_BUS_MSG_HOST_ENUM_RESP */
		uint8_t  reserved[3];
		uint32_t valid_addresses[8];
	} s;
	uint32_t              dw[9];
} heci_host_enum_resp_t;

typedef union {
	struct
	{
		uint8_t command; /* = HECI_BUS_MSG_HOST_CLIENT_PROP_REQ */
		uint8_t address;
		uint8_t reserved[2];
	} s;
	uint32_t dw[1];
} heci_client_prop_req_t;

typedef union {
	struct {
		uint8_t     command; /* = HECI_BUS_MSG_HOST_CLIENT_PROP_RESP */
		uint8_t     address;
		uint8_t     status;
		uint8_t     reserved_1;
		heci_guid_t protocol_id;
		uint8_t     protocol_ver;
		uint8_t     max_n_of_conns;
		uint8_t     reserved_2;
		uint8_t     reserved_3;
		uint32_t    max_msg_size;
		uint8_t     dma_header_length:7;
		uint8_t     dma_enabled:1;
		uint8_t     reserved_4;
		uint8_t     reserved_5;
		uint8_t     reserved_6;
	} s;
	uint32_t dw[8];
} heci_client_prop_resp_t;

typedef union {
	struct {
		uint8_t command; /* = HECI_BUS_MSG_CLIENT_CONNECT_REQ */
		uint8_t ish_addr;
		uint8_t host_addr;
		uint8_t reserved;
	} s;
	uint32_t dw[1];
} heci_conn_req_t;

typedef union {
	struct {
		uint8_t command; /* = HECI_BUS_MSG_CLIENT_CONNECT_RESP */
		uint8_t ish_addr;
		uint8_t host_addr;
		uint8_t status;  /* HECI_CONNECT_STATUS_SUCCESS
				  * HECI_CONNECT_STATUS_CLIENT_NOT_FOUND
				  * HECI_CONNECT_STATUS_ALREADY_EXISTS
				  * HECI_CONNECT_STATUS_REJECTED
				  */
	} s;
	uint32_t dw[1];
} heci_conn_resp_t;

typedef union {
	struct {
		uint8_t command; /* = HECI_BUS_MSG_CLIENT_DISCONNECT_REQ */
		uint8_t ish_addr;
		uint8_t host_addr;
		uint8_t reserved;
	} s;
	uint32_t dw[1];
} heci_disconn_req_t;

typedef union {
	struct {
		uint8_t command; /* = HECI_BUS_MSG_CLIENT_DISCONNECT_RESP */
		uint8_t ish_addr;
		uint8_t host_addr;
		uint8_t status;  /* HECI_CONNECT_STATUS_SUCCESS */
	} s;
	uint32_t dw[1];
} heci_disconn_resp_t;

typedef union {
	struct {
		uint8_t  command; /* =  HECI_BUS_MSG_FLOW_CONTROL */
		uint8_t  ish_addr;
		uint8_t  host_addr;
		uint8_t  number_of_packets;
		uint32_t reserved;
	} s;
	uint32_t dw[2];
} heci_flow_ctrl_t;

typedef union {
	struct {
		uint8_t command; /* =  HECI_BUS_MSG_RESET_REQ */
		uint8_t ish_addr;
		uint8_t host_addr;
		uint8_t reserved1;
	} s;
	uint32_t dw[1];
} heci_reset_req_t;

typedef union {
	struct {
		uint8_t command; /* = HECI_BUS_MSG_RESET_RESP */
		uint8_t ish_addr;
		uint8_t host_addr;
		uint8_t status;
	} s;
	uint32_t dw[1];
} heci_reset_resp_t;

#ifdef DMA_XFER_SUPPORTED

typedef struct {
	uint8_t command;     /* = HECI_BUS_MSG_DMA_ALLOC_NOTIFY */
	uint8_t	reserved[3]; /* Reserved for future use. Must be zero */
			     /*for this version of the HBM. */
	dma_buf_info_t alloc_dma_buf[0]; /* List of host allocated buffers */
} heci_bus_dma_alloc_notify_req_t;


typedef enum {
	HECI_DMA_ALLOC_STATUS_OK    = 0,
	HECI_DMA_ALLOC_STATUS_ERROR = 1,
} HECI_BUS_DMA_MSG_STATUS;

typedef union {
	struct {
		uint8_t command; /* = HECI_BUS_MSG_DMA_ALLOC_RESP */
		uint8_t status;  /* 0 = success */
		uint8_t reserved[2];
	} s;
	uint32_t dw[1];
} heci_bus_dma_alloc_response_t;

typedef struct {
	uint64_t msg_addr_in_host; /* address in host memory where message */
				   /* is located. Bits 0-11 must be 0. */
	uint32_t msg_length;       /* The size of the message contained */
				   /* in the Message Address parameter */
	uint8_t  reserved[4];      /* address in host memory where message */
				   /* is located. Bits 0-11 must be 0. */
} dma_msg_info;

typedef struct {
	uint8_t command;           /* =  HECI_BUS_MSG_DMA_XFER_RESP */
	uint8_t ish_addr;
	uint8_t host_addr;
	uint8_t reserved;
	dma_msg_info_t dma_buff[0];
} heci_bus_dma_xfer_resp_t;
#endif

typedef union {
	struct {
		uint8_t  command;  /* =  HECI_BUS_MSG_DMA_XFER_REQ */
		uint8_t  ish_addr;
		uint8_t  host_addr;
		uint8_t  reserved;
		uint64_t msg_addr_in_host; /* address in host memory where */
					   /* message is located. */
					   /* Bits 0-11 must be 0.*/
		uint32_t msg_length; /* The size of the message contained */
				     /* in the Message Address parameter */
		uint8_t  reserved2[4];
	} s;
	uint32_t dw[5];
} heci_bus_dma_xfer_req_t;

typedef struct {
	heci_guid_t protocol_id;
	uint8_t protocol_version;
	uint8_t max_number_of_connections;
	uint8_t fixed_address;
	uint8_t single_receive_buffer;
	uint32_t max_message_length;
	uint8_t dma_header_length :7;
	uint8_t dma_enabled :1;
	uint8_t reserved[3];
} heci_client_properties_t;

typedef struct {
	uint8_t	command;
	uint8_t	client_addr;
	uint8_t	reserved[2];
	heci_client_properties_t client_properties;
} heci_add_client_req_t;

typedef struct {
	uint8_t command;
	uint8_t client_addr;
	uint8_t status;
} heci_add_client_resp_t;

typedef union {
	heci_version_req_t     ver_req;
	heci_host_stop_req_t   stop_req;
	heci_host_enum_req_t   enum_req;
	heci_client_prop_req_t prop_req;
	heci_conn_req_t        conn_req;
	heci_disconn_req_t     disconnect_req;
	heci_disconn_resp_t    disconnect_resp;
	heci_flow_ctrl_t       flow_control;
	heci_reset_req_t       reset_req;
	heci_dma_req_t         dma_req;
	heci_add_client_resp_t new_client_resp;
#ifdef DMA_XFER_SUPPORTED
	heci_bus_dma_alloc_notify_req_t dma_alloc_notify_req;
	heci_bus_dma_alloc_response_t   dma_alloc_resp;
	heci_bus_dma_xfer_req_t  dma_xfer_req;
	heci_bus_dma_xfer_resp_t dma_xfer_resp;
#endif
	uint8_t data[HECI_IPC_PACKET_SIZE - sizeof(heci_hdr_t)];
} heci_bus_msg_t;

#pragma pack()

typedef struct {
	heci_hdr_t hdr;
	heci_bus_msg_t payload;
} heci_ipc_bup_t;

uint8_t heci_send_to_connection(heci_conn_t *connection, const mrd_t *msg);
void heci_reset_req (heci_ipc_bup_t *packet, uint32_t length);
void heci_flow_control_recv (heci_ipc_bup_t *packet, uint32_t length);
void heci_client_prop_req (heci_ipc_bup_t *packet, uint32_t length);
void heci_version_req (heci_ipc_bup_t *packet, uint32_t length);
void heci_connect_req (heci_ipc_bup_t *packet, uint32_t length);
void heci_connection_reset( heci_conn_t *connection);
heci_conn_t* heci_find_conn(
	uint8_t ish_addr, uint8_t host_addr, uint8_t state);
void heci_add_client_resp(heci_ipc_bup_t *packet, uint32_t length);
heci_rx_msg_t* heci_get_buffer_from_pool (heci_client_ctrl_t *client);
void heci_disconnect_req (heci_ipc_bup_t *packet, uint32_t length);

#endif /* __HECI_INTERNAL_H */
