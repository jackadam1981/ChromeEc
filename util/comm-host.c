/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm-host.h"
#include "ec_commands.h"

#define EC_BUF_SIZE_CACHE "/tmp/ec_buffer_size"
#define EC_BUF_SIZE_MIN   32

int (*ec_command_proto)(int command, int version,
			const void *outdata, int outsize,
			void *indata, int insize);

int (*ec_readmem)(int offset, int bytes, void *dest);

int ec_max_outsize, ec_max_insize;
void *ec_outbuf;
void *ec_inbuf;
static int command_offset;

int comm_init_dev(const char *device_name) __attribute__((weak));
int comm_init_lpc(void) __attribute__((weak));
int comm_init_i2c(void) __attribute__((weak));

static int fake_readmem(int offset, int bytes, void *dest)
{
	struct ec_params_read_memmap p;
	int c;
	char *buf;

	p.offset = offset;

	if (bytes) {
		p.size = bytes;
		c = ec_command(EC_CMD_READ_MEMMAP, 0, &p, sizeof(p),
			       dest, p.size);
		if (c < 0)
			return c;
		return p.size;
	}

	p.size = EC_MEMMAP_TEXT_MAX;

	c = ec_command(EC_CMD_READ_MEMMAP, 0, &p, sizeof(p), dest, p.size);
	if (c < 0)
		return c;

	buf = dest;
	for (c = 0; c < EC_MEMMAP_TEXT_MAX; c++) {
		if (buf[c] == 0)
			return c;
	}

	buf[EC_MEMMAP_TEXT_MAX - 1] = 0;
	return EC_MEMMAP_TEXT_MAX - 1;
}

void set_command_offset(int offset)
{
	command_offset = offset;
}

int ec_command(int command, int version,
	       const void *outdata, int outsize,
	       void *indata, int insize)
{
	/* Offset command code to support sub-devices */
	return ec_command_proto(command_offset + command, version,
				outdata, outsize,
				indata, insize);
}

static int read_cached_buffer_size(void)
{
	FILE *fp;
	int in_size, out_size;
	int ret = 0;
	fp = fopen(EC_BUF_SIZE_CACHE, "r");
	if (!fp) {
		/* cached buffer size not found. Will read from ec. */
		return 1;
	}

	if (fscanf(fp, "%d %d\n", &in_size, &out_size) < 0) {
		fprintf(stderr, "Error read cached buffer size\n");
		ret = 1;
		goto read_cached_buffer_size_end;
	}
	if (in_size < EC_BUF_SIZE_MIN || ec_max_insize < in_size ||
	    out_size < EC_BUF_SIZE_MIN || ec_max_outsize < out_size) {
		fprintf(stderr, "Cached buffer size does not make sense\n");
		ret = 1;
		goto read_cached_buffer_size_end;
	}
	ec_max_outsize = out_size;
	ec_max_insize = in_size;

read_cached_buffer_size_end:
	fclose(fp);
	return ret;
}

static void write_cached_buffer_size(void)
{
	FILE *fp;
	fp = fopen(EC_BUF_SIZE_CACHE, "w");
	if (!fp) {
		fprintf(stderr, "Error open file %s\n", EC_BUF_SIZE_CACHE);
		return;
	}
	if (fprintf(fp, "%d %d\n", ec_max_insize, ec_max_outsize) < 0)
		fprintf(stderr, "Error write cached buffer size\n");
	fclose(fp);
}

int comm_init(int interfaces, const char *device_name)
{
	struct ec_response_get_protocol_info info;
	int new_size;
	int cache_size_flag;

	/* Default memmap access */
	ec_readmem = fake_readmem;

	/* Prefer new /dev method */
	if ((interfaces & COMM_DEV) && comm_init_dev &&
	    !comm_init_dev(device_name))
		goto init_ok;

	/* Fallback to direct LPC on x86 */
	if ((interfaces & COMM_LPC) && comm_init_lpc && !comm_init_lpc())
		goto init_ok;

	/* Fallback to direct i2c on ARM */
	if ((interfaces & COMM_I2C) && comm_init_i2c && !comm_init_i2c())
		goto init_ok;

	/* Give up */
	fprintf(stderr, "Unable to establish host communication\n");
	return 1;

 init_ok:
	/* read disk for cache buffer_size first */
	cache_size_flag = read_cached_buffer_size();

	/* Allocate shared I/O buffers */
	ec_outbuf = malloc(ec_max_outsize);
	ec_inbuf = malloc(ec_max_insize);
	if (!ec_outbuf || !ec_inbuf) {
		fprintf(stderr, "Unable to allocate buffers\n");
		return 1;
	}

	if (!cache_size_flag) {
		return 0;
	}
	/*
	 * If we do not have cached buffer size, we will ask ec by
	 * read max request / response size from ec and then reduce the buffer
	 * size if the size supported by ec is less than the default value.
	 */
	if (ec_command(EC_CMD_GET_PROTOCOL_INFO, 0, NULL, 0, &info,
		sizeof(info)) == sizeof(info)) {
		new_size = info.max_request_packet_size -
			sizeof(struct ec_host_request);
		if (ec_max_outsize > new_size) {
			ec_max_outsize = new_size;
			ec_outbuf = realloc(ec_outbuf, ec_max_outsize);
		}
		new_size = info.max_response_packet_size -
			sizeof(struct ec_host_response);

		if (ec_max_insize > new_size) {
			ec_max_insize = new_size;
			ec_inbuf = realloc(ec_inbuf, ec_max_insize);
		}

		if (!ec_outbuf || !ec_inbuf) {
			fprintf(stderr, "Unable to reallocate buffers\n");
			return 1;
		}
	}
	write_cached_buffer_size();
	return 0;

}
