/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util_pcap.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/logging/log.h>

#include <drivers/pdc.h>

#define LINK_HDR_SIZE 3
#define LINK_HDR_PORT 0
#define LINK_HDR_DIR 1
#define LINK_HDR_TYPE 2

#define LINK_RX 0
#define LINK_TX 1

#define TRACE_PORT 0

LOG_MODULE_REGISTER(pdc_trace, LOG_LEVEL_INF);

static uint8_t pcap_buf[500];

static void pcap_out(const uint8_t *pcap_buf, size_t buf_len)
{
	static FILE *pcap;

	if (pcap == NULL)
		pcap = pcap_open();

	if (pcap != NULL)
		pcap_append(pcap, pcap_buf, buf_len);
}

void pdc_trace_msg_req(int port, enum pdc_trace_chip_type msg_type,
		       const uint8_t *buf, const int count)
{
	if (port != TRACE_PORT)
		return;

	if (count <= 0)
		return;

	LOG_INF("PDC request: port %d, length %d:", port, count);
	LOG_HEXDUMP_INF(buf, count, "message:");

	pcap_buf[LINK_HDR_PORT] = TRACE_PORT;
	pcap_buf[LINK_HDR_DIR] = LINK_TX;
	pcap_buf[LINK_HDR_TYPE] = msg_type;
	memcpy(&pcap_buf[LINK_HDR_SIZE], buf,
	       MIN(count, sizeof(pcap_buf) - LINK_HDR_SIZE));

	pcap_out(pcap_buf, LINK_HDR_SIZE + count);
}

void pdc_trace_msg_resp(int port, enum pdc_trace_chip_type msg_type,
			const uint8_t *buf, const int count)
{
	if (port != TRACE_PORT)
		return;

	if (count <= 0)
		return;

	LOG_INF("PDC response: port %d, length %d:", port, count);
	LOG_HEXDUMP_INF(buf, count, "message:");

	pcap_buf[LINK_HDR_PORT] = TRACE_PORT;
	pcap_buf[LINK_HDR_DIR] = LINK_RX;
	pcap_buf[LINK_HDR_TYPE] = msg_type;
	memcpy(&pcap_buf[LINK_HDR_SIZE], buf,
	       MIN(count, sizeof(pcap_buf) - LINK_HDR_SIZE));

	pcap_out(pcap_buf, LINK_HDR_SIZE + count);
}
