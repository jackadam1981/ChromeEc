/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Megachips DisplayPort to HDMI protocol converter / level shifter driver.
 */

#include "config.h"
#include "console.h"
#include "common.h"
#include "mcdp28x0.h"
#include "timer.h"
#include "usart-stm32f0.h"
#include "util.h"

USART_CONFIG(usart_mcdp, CONFIG_MCDP28X0, 115200, 64, 64, NULL, NULL);

#define MCDP_DEBUG

/**
 * Compute checksum
 *
 * @cnt count of data characters
 * @msg message bytes
 * @return computed checksum (two's complement of sum of msg)
 */
static char compute_checksum(int cnt, const char *msg)
{
	int i;
	int chksum = 0;
	for (i = 0; i < cnt; i++)
		chksum += msg[i];
	return ~chksum + 1;
}

/**
 * transmit serial packet.
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
static int tx_serial(int cnt, const char *msg)
{
	int rv;
	size_t wrote;
	uint8_t outbuf[MCDP_OUTBUF_MAX];

	outbuf[0] = cnt + 2;
	if (outbuf[0] > MCDP_OUTBUF_MAX)
		return EC_ERROR_PARAM1;

	memcpy(&outbuf[1],  msg, cnt);
	outbuf[cnt + 1] = compute_checksum(cnt + 1, outbuf);

#ifdef MCDP_DEBUG
	ccprintf("outbuf:");
	for (rv = 0; rv < outbuf[0]; rv++)
		if (rv && !(rv % 4))
			ccprintf("\n       ");
		ccprintf("[%02d]0x%02x ", rv, outbuf[rv]);
	ccprintf("\n");
#endif

	wrote = out_stream_write(&usart_mcdp.out, outbuf, outbuf[0]);
	rv = !(wrote == outbuf[0]);
	return rv;
}

static int rx_serial(int cnt, uint8_t *inbuf)
{
	int rv, retry = 2;
	size_t read = 0;

	read = in_stream_read(&usart_mcdp.in, inbuf, cnt);
	while ((read < cnt) && retry) {
		usleep(100*MSEC);
		read += in_stream_read(&usart_mcdp.in, inbuf + read,
				       cnt - read);
		retry--;
	}

#ifdef MCDP_DEBUG
	ccprintf(" inbuf:");
	for (rv = 0; rv < cnt; rv++) {
		if (rv && !(rv % 4))
			ccprintf("\n       ");
		ccprintf("[%02d]0x%02x ", rv, inbuf[rv]);
	}
	ccprintf("\n");
#endif

	rv = !(read == cnt);
	return rv;
}

void mcdp_init(void)
{
	usart_init(&usart_mcdp);
}

int mcdp_get_info(struct mcdp_info  *info)
{
	uint8_t inbuf[MCDP_INBUF_MAX];
	const char msg[2] = {0x40, 0x00}; /* cmd + msg type */

	if (tx_serial(2, msg))
		return EC_ERROR_UNKNOWN;

	if (rx_serial(15, inbuf))
		return EC_ERROR_UNKNOWN;

	/* cmd out == cmd in.  Note, length is in [0] */
	if (msg[0] != inbuf[1])
		return EC_ERROR_UNKNOWN;

	info->family = (inbuf[2] << 8) | inbuf[3];
	info->chipid = (inbuf[4] << 8) | inbuf[5];
	info->irom.major = inbuf[6];
	info->irom.minor = inbuf[7];
	info->irom.build = (inbuf[8] << 8) | inbuf[9];
	info->fw.major = inbuf[10];
	info->fw.minor = inbuf[11];
	info->fw.build = (inbuf[12] << 8) | inbuf[13];

#ifdef MCDP_DEBUG
	ccprintf("irom:%d.%d.%d fw:%d.%d.%d family:%04x chipid:%04x\n",
		 info->irom.major, info->irom.minor, info->irom.build,
		 info->fw.major, info->fw.minor, info->fw.build,
		 info->family, info->chipid);
#endif
	return EC_SUCCESS;
}
