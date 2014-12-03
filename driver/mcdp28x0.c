/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Megachips DisplayPort to HDMI protocol converter / level shifter driver.
 */
#include "mcdp28x0.h"
#include "usart-stm32f0.h"
#include "util.h"

static void in_ready(struct in_stream const *stream)
{
}

USART_CONFIG(usart3, usart3_hw, 115200, 64, 64, in_ready, NULL)

/**
 * Compute checksum
 *
 * @cnt count of data characters
 * @msg message bytes
 * @return computed checksum (two's complement of sum of msg)
 */
static char compute_checksum(int cnt, char *msg)
{
	int i;
	int chksum = 0;
	for (i = 0; i < cnt; i++)
		chksum += msg[i];
	return ~chksum + 1;
}

/**
 * send serial packet.
 *
 * Packet consists of:
 *   payload[0]     == length including this byte + cnt + chksum
 *   payload[1]     == 1st message byte
 *   payload[cnt]   == last message byte
 *   payload[cnt+1] == checksum
 *
 * @cnt count of message bytes
 * @msg message bytes
 * @return EC_SUCCESS(zero) if success, error code otherwise
 */
int mcdp_send_serial1(int cnt, char *msg)
{
	char payload[10];
	payload[0] = cnt + 2;
	if (payload[0] > 10)
		return EC_ERROR_PARAM1;
	memcpy(&payload[1],  msg, cnt);
	payload[cnt + 1] = compute_checksum(cnt, msg);
	// send it
	return EC_SUCCESS;
}
