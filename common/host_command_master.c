/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Host command master module for Chrome EC */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "i2c.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_HOSTCMD, outstr)
#define CPRINTF(format, args...) cprintf(CC_HOSTCMD, format, ## args)

/* 8-bit I2C address for PD MCU */
#define I2C_PD_MCU_ADDRESS 0xaa

/*
 * Sends a command to the PD (protocol v2).
 *
 * Returns >= 0 for success, or negative if error.
 *
 */
static int pd_command_i2c(int command, int version,
			  const void *outdata, int outsize,
			  void *indata, int insize)
{
	int ret, i;
	int req_len, resp_len;
	static uint8_t req_buf[EC_PROTO2_MAX_REQUEST_SIZE];
	static uint8_t resp_buf[EC_PROTO2_MAX_RESPONSE_SIZE];
	uint8_t sum;
	const uint8_t *c;
	uint8_t *d;

	if (version > 1) {
		CPRINTF("[%T Command versions >1 unsupported]\n");
		return -EC_RES_ERROR;
	}

	req_buf[0] = version + EC_CMD_VERSION0;
	req_buf[1] = command;
	req_buf[2] = outsize;
	req_len = outsize + EC_PROTO2_REQUEST_OVERHEAD;

	sum = req_buf[0] + req_buf[1] + req_buf[2];
	/* copy message payload and compute checksum */
	for (i = 0, c = outdata; i < outsize; i++, c++) {
		req_buf[i + 3] = *c;
		sum += *c;
	}
	req_buf[req_len - 1] = sum;

	/*
	 * transmit all data and receive 2 bytes for return value and response
	 * length.
	 */
	ret = i2c_xfer(I2C_PORT_PD_MCU, I2C_PD_MCU_ADDRESS, &req_buf[0],
			req_len, &resp_buf[0], 2, I2C_XFER_START);
	if (ret) {
		CPRINTF("[%T i2c transaction 1 failed: %d]\n", ret);
		return -ret;
	}

	ret = resp_buf[0];
	resp_len = resp_buf[1];

	if (ret) {
		CPRINTF("[%T command 0x%02x returned error %d]\n", command,
			ret);
		return -ret;
	}

	if (resp_len > insize)
		resp_len = insize;

	/* receive remaining data */
	ret = i2c_xfer(I2C_PORT_PD_MCU, I2C_PD_MCU_ADDRESS, 0, 0,
			&resp_buf[2], resp_len+1, I2C_XFER_STOP);
	if (ret) {
		CPRINTF("[%T i2c transaction 2 failed: %d]\n", ret);
		return -ret;
	}

	/* copy response packet payload and compute checksum */
	sum = resp_buf[0] + resp_buf[1];
	for (i = 0, d = indata; i < resp_len; i++, d++) {
		*d = resp_buf[i + 2];
		sum += *d;
	}

	if (sum != resp_buf[resp_len + 2]) {
		CPRINTF("[%T command 0x%02x bad checksum returned: expected"
			"%d, got %d]\n", command, sum, resp_buf[resp_len+2]);
		return -EC_RES_ERROR;
	}

	/* Return output buffer size */
	return resp_len;
}

static int command_pd_mcu(int argc, char **argv)
{
	char *e;
	static char outbuf[EC_PROTO2_MAX_REQUEST_SIZE];
	static char inbuf[EC_PROTO2_MAX_RESPONSE_SIZE];
	int command, version;
	int i, ret, tmp;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	command = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	version = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	for (i = 3; i < argc; i++) {
		tmp = strtoi(argv[i], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;
		outbuf[i-3] = tmp;
	}

	ret = pd_command_i2c(command, version, &outbuf, argc - 3, &inbuf,
			EC_PROTO2_MAX_RESPONSE_SIZE);

	ccprintf("Host command 0x%02x, returned %d\n", command, ret);
	for (i = 0; i < ret; i++)
		ccprintf("%02x\n", inbuf[i]);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pdcmd, command_pd_mcu,
			"cmd ver [params]",
			"Send PD host command",
			NULL);

