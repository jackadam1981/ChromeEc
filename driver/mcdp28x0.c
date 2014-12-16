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

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

#undef MCDP_DEBUG

#ifdef MCDP_DEBUG
static inline void print_buffer(int cnt, uint8_t *buf)
{
	int i;
	CPRINTF("buf:");
	for (i = 0; i < buf[0]; i++) {
		if (i && !(i % 4))
			CPRINTF("\n    ");
		CPRINTF("[%02d]0x%02x ", i, buf[i]);
	}
	CPRINTF("\n");
}
#else
static inline void print_buffer(int cnt, uint8_t *buf) {}
#endif

USART_CONFIG(usart_mcdp, CONFIG_MCDP28X0, 115200, MCDP_INBUF_MAX,
	     MCDP_OUTBUF_MAX, NULL, NULL);

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
 * @return zero if success, error code otherwise
 */
static int tx_serial(int cnt, const char *msg)
{
	size_t wrote;
	uint8_t outbuf[MCDP_OUTBUF_MAX];

	outbuf[0] = cnt + 2;
	if (outbuf[0] > MCDP_OUTBUF_MAX)
		return EC_ERROR_PARAM1;

	memcpy(&outbuf[1],  msg, cnt);
	outbuf[cnt + 1] = compute_checksum(cnt + 1, outbuf);

	print_buffer(outbuf[0], outbuf);

	wrote = out_stream_write(&usart_mcdp.out, outbuf, outbuf[0]);

	return !(wrote == outbuf[0]);
}

/**
 * receive serial packet
 *
 * @cnt count of message bytes
 * @inbuf pointer to buffer to read into
 * @return zero if success, error code otherwise
 */
static int rx_serial(int cnt, uint8_t *inbuf)
{
	size_t read;
	int retry = 2;

	read = in_stream_read(&usart_mcdp.in, inbuf, cnt);
	while ((read < cnt) && retry) {
		usleep(100*MSEC);
		read += in_stream_read(&usart_mcdp.in, inbuf + read,
				       cnt - read);
		retry--;
	}

	print_buffer(read, inbuf);

	return !(read == cnt);
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

	memcpy(info, &inbuf[2], 14);

#ifdef MCDP_DEBUG
	CPRINTF("irom:%d.%d.%d fw:%d.%d.%d family:%04x chipid:%04x\n",
		info->irom.major, info->irom.minor, info->irom.build,
		info->fw.major, info->fw.minor, info->fw.build,
		MCDP_FAMILY(info->family), MCDP_CHIPID(info->chipid));
#endif
	return EC_SUCCESS;
}
