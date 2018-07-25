#ifndef __HECI_COUNTER_H
#define __HECI_COUNTER_H

#include <heci.h>

typedef struct {
	uint32_t total_out_messages;
	uint32_t disconnect_resp;
	uint32_t connect_resp;
	uint32_t recv_flow_ctrl;
	uint32_t send_flow_ctrl;
	uint32_t flow_ctrl_failed;
	uint32_t client_req;
	uint32_t no_buffers;
} heci_client_stat;

typedef struct {
	uint32_t read;
	uint32_t error;
	uint32_t write;
	uint32_t write_error;
	uint32_t ipc_reset;
	uint32_t new_client_response;
	uint32_t client_register;
	uint32_t disconnect_req;
	uint32_t connect_req;
	uint32_t driver_packets;
	uint32_t dma_xfer_write;
	uint32_t dma_xfer_write_error;
	uint32_t dma_xfer_ack_recieved;
	uint32_t dma_xfer_read;
	uint32_t dma_xfer_read_error;
	heci_client_stat client[HECI_MAX_NUM_OF_CLIENTS];
} heci_stat;
#endif /* __HECI_COUNTER_H_ */
