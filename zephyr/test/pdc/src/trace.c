/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util-pcap.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <drivers/pdc.h>

#define LINK_HDR_SIZE 1
#define LINK_TX 54
#define LINK_RX 0

#define TRACE_PORT 0

static uint8_t pcap_buf[500];

static void pcap_out(const uint8_t *pcap_buf, size_t buf_len)
{
	static FILE *pcap;

	if (pcap == NULL)
		pcap = pcap_open();

	if (pcap != NULL)
		pcap_append(pcap, pcap_buf, buf_len);
}

void _pdc_trace_rts_req(int port, const uint8_t *buf, const int count)
{
	if (port != TRACE_PORT)
		return;

	if (count <= 0)
		return;

	printk("%s: [port %d]", __func__, port);
	for (int i = 0; i < count; ++i)
		printk(" %02x", buf[i]);
	printk("\n");

	pcap_buf[0] = LINK_TX;
	memcpy(&pcap_buf[LINK_HDR_SIZE], buf,
	       MIN(count, sizeof(pcap_buf) - LINK_HDR_SIZE));

	pcap_out(pcap_buf, LINK_HDR_SIZE + count);
}

void _pdc_trace_rts_resp(int port, const uint8_t *buf, const int count)
{
	if (port != TRACE_PORT)
		return;

	if (count <= 0)
		return;

	printk("%s: [port %d]", __func__, port);
	for (int i = 0; i < count; ++i)
		printk(" %02x", buf[i]);
	printk("\n");

	pcap_buf[0] = LINK_RX;
	memcpy(&pcap_buf[LINK_HDR_SIZE], buf,
	       MIN(count, sizeof(pcap_buf) - LINK_HDR_SIZE));

	pcap_out(pcap_buf, LINK_HDR_SIZE + count);
}
