/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __HBM_H
#define __HBM_H

#include <stdint.h>
#include <stddef.h>

#include "heci_client.h"

#define HBM_MAJOR_VERSION			1
#ifdef HECI_ENABLE_DMA
#define HBM_MINOR_VERSION			2
#else
#define HBM_MINOR_VERSION			0
#endif

#define __packed __attribute__((packed))

#define HECI_BUS_MSG_VERSION_REQ	0x01
#define HECI_BUS_MSG_VERSION_RESP	(0x80 | HECI_BUS_MSG_VERSION_REQ)
#define HECI_BUS_MSG_HOST_STOP_REQ	0x02
#define HECI_BUS_MSG_HOST_STOP_RESP	(0x80 | HECI_BUS_MSG_HOST_STOP_REQ)
#define HECI_BUS_MSG_ME_STOP_REQ	0x03
#define HECI_BUS_MSG_HOST_ENUM_REQ	0x04
#define HECI_BUS_MSG_HOST_ENUM_RESP	(0x80 | HECI_BUS_MSG_HOST_ENUM_REQ)
#define HECI_BUS_MSG_HOST_CLIENT_PROP_REQ	0x05
#define HECI_BUS_MSG_HOST_CLIENT_PROP_RESP \
				(0x80 | HECI_BUS_MSG_HOST_CLIENT_PROP_REQ)
#define HECI_BUS_MSG_CLIENT_CONNECT_REQ		0x06
#define HECI_BUS_MSG_CLIENT_CONNECT_RESP \
				(0x80 | HECI_BUS_MSG_CLIENT_CONNECT_REQ)
#define HECI_BUS_MSG_CLIENT_DISCONNECT_REQ	0x07
#define HECI_BUS_MSG_CLIENT_DISCONNECT_RESP \
				(0x80 | HECI_BUS_MSG_CLIENT_DISCONNECT_REQ)
#define HECI_BUS_MSG_FLOW_CONTROL	0x08
#define HECI_BUS_MSG_RESET_REQ		0x09
#define HECI_BUS_MSG_RESET_RESP		(0x80 | HECI_BUS_MSG_RESET_REQ)
#define HECI_BUS_MSG_ADD_CLIENT_REQ	0x0A
#define HECI_BUS_MSG_ADD_CLIENT_RESP	(0x80 | HECI_BUS_MSG_ADD_CLIENT_REQ)
#define HECI_BUS_MSG_DMA_REQ		0x10
#define HECI_BUS_MSG_DMA_RESP		(0x80 | HECI_BUS_MSG_DMA_REQ)
#define HECI_BUS_MSG_DMA_ALLOC_NOTIFY	0x11
#define HECI_BUS_MSG_DMA_ALLOC_RESP	(0x80 | HECI_BUS_MSG_DMA_ALLOC_NOTIFY)
#define HECI_BUS_MSG_DMA_XFER_REQ	0x12
#define HECI_BUS_MSG_DMA_XFER_RESP	(0x80 | HECI_BUS_MSG_DMA_XFER_REQ)

enum {
	HECI_CONNECT_STATUS_SUCCESS           = 0,
	HECI_CONNECT_STATUS_CLIENT_NOT_FOUND  = 1,
	HECI_CONNECT_STATUS_ALREADY_EXISTS    = 2,
	HECI_CONNECT_STATUS_REJECTED          = 3,
	HECI_CONNECT_STATUS_INVALID_PARAMETER = 4,
	HECI_CONNECT_STATUS_INACTIVE_CLIENT   = 5,
};

struct hbm_version {
	uint8_t minor;
	uint8_t major;
} __packed;

struct hbm_version_req {
	uint8_t reserved;
	struct hbm_version version;
} __packed;

struct hbm_version_res {
	uint8_t supported;
	struct hbm_version version;
} __packed;

struct hbm_enum_req {
	uint8_t reserved[3];
} __packed;

struct hbm_enum_res {
	uint8_t reserved[3];
	uint8_t valid_addresses[32];
} __packed;

struct hbm_client_prop_req {
	uint8_t address;
	uint8_t reserved[2];
} __packed;

struct hbm_client_properties {
	struct heci_guid protocol_name;
	uint8_t protocol_version;
	uint8_t max_number_of_connections;
	uint8_t fixed_address;
	uint8_t single_recv_buf;
	uint32_t max_msg_length;
	uint8_t dma_hdr_len:7;
	uint8_t dma_enabled:1;
	uint8_t reserved4;
	uint8_t reserved5;
	uint8_t reserved6;
} __packed;

struct hbm_client_prop_res {
	uint8_t address;
	uint8_t status;
	uint8_t reserved[1];
	struct hbm_client_properties client_prop;
} __packed;

struct hbm_client_connect_req {
	uint8_t fw_addr;
	uint8_t host_addr;
	uint8_t reserved;
} __packed;

struct hbm_client_connect_res {
	uint8_t fw_addr;
	uint8_t host_addr;
	uint8_t status;
} __packed;

struct hbm_flow_control {
	uint8_t fw_addr;
	uint8_t host_addr;
	uint8_t reserved[5];
} __packed;

struct hbm_client_disconnect_req {
	uint8_t fw_addr;
	uint8_t host_addr;
	uint8_t reserved;
} __packed;

struct hbm_client_disconnect_res {
	uint8_t fw_addr;
	uint8_t host_addr;
	uint8_t status;
} __packed;

struct hbm_host_stop_req {
	uint8_t reason;
	uint8_t reserved[2];
};

struct hbm_host_stop_res {
	uint8_t reserved[3];
};

/* host bus message : host -> ish */
struct hbm_h2i {
	uint8_t cmd;
	union {
		struct hbm_version_req			ver_req;
		struct hbm_enum_req			enum_req;
		struct hbm_client_prop_req		client_prop_req;
		struct hbm_client_connect_req		client_connect_req;
		struct hbm_flow_control			flow_ctrl;
		struct hbm_client_disconnect_req	client_disconnect_req;
		struct hbm_host_stop_req		host_stop_req;
	} data;
} __packed;

/* host bus message : i2h -> host */
struct hbm_i2h {
	uint8_t cmd;
	union {
		struct hbm_version_res			ver_res;
		struct hbm_enum_res			enum_res;
		struct hbm_client_prop_res		client_prop_res;
		struct hbm_client_connect_res		client_connect_res;
		struct hbm_flow_control			flow_ctrl;
		struct hbm_client_disconnect_res	client_disconnect_res;
		struct hbm_host_stop_res		host_stop_res;
	} data;
} __packed;

#endif /* __HBM_H */
