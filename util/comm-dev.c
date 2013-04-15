/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cros_ec_dev.h"
#include "comm-host.h"
#include "ec_commands.h"

static int fd = -1;

static int ec_command_dev(int command, int version,
			  void *indata, int insize,
			  void *outdata, int outsize)
{
	struct cros_ec_command s_cmd;

	s_cmd.command = command;
	s_cmd.version = version;
	s_cmd.result = 0xff;
	s_cmd.insize = insize;
	s_cmd.indata = indata;
	s_cmd.outsize = outsize;
	s_cmd.outdata = outdata;

	if (ioctl(fd, CROS_EC_DEV_IOCXCMD, &s_cmd)) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
		return -EC_RES_ERROR;
	}

	return s_cmd.result;
}

static uint8_t read_mapped_mem8_dev(uint8_t offset)
{
	struct cros_ec_read_mem8 s_mem8;

	s_mem8.offset = offset;
	if (ioctl(fd, CROS_EC_DEV_IOCRDMEM8, &s_mem8)) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
		return -EC_RES_ERROR;
	}

	return s_mem8.value;
}

static uint16_t read_mapped_mem16_dev(uint8_t offset)
{
	struct cros_ec_read_mem16 s_mem16;

	s_mem16.offset = offset;
	if (ioctl(fd, CROS_EC_DEV_IOCRDMEM16, &s_mem16)) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
		return -EC_RES_ERROR;
	}

	return s_mem16.value;
}

static uint32_t read_mapped_mem32_dev(uint8_t offset)
{
	struct cros_ec_read_mem32 s_mem32;

	s_mem32.offset = offset;
	if (ioctl(fd, CROS_EC_DEV_IOCRDMEM32, &s_mem32)) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
		return -EC_RES_ERROR;
	}

	return s_mem32.value;
}

static int read_mapped_string_dev(uint8_t offset, char *buffer)
{
	struct cros_ec_read_string s_str;

	s_str.offset = offset;
	s_str.buffer = buffer;
	if (ioctl(fd, CROS_EC_DEV_IOCRDSTR, &s_str)) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
		return -EC_RES_ERROR;
	}

	return s_str.length - 1;	/* ioctl counts trailing '\0' */
}


int comm_init_dev(void)
{
	char version[80];
	int r;
	char *s;

	fd = open("/dev/" CROS_EC_DEV_NAME, O_RDWR);
	if (fd < 0)
		return 1;

	r = read(fd, version, sizeof(version)-1);
	if (r <= 0) {
		close(fd);
		return 2;
	}
	version[r] = '\0';
	s = strrchr(version, '\n');
	if (s)
		*s = '\0';
	if (strcmp(version, CROS_EC_DEV_VERSION)) {
		close(fd);
		return 3;
	}

	ec_command = ec_command_dev;
	read_mapped_mem8 = read_mapped_mem8_dev;
	read_mapped_mem16 = read_mapped_mem16_dev;
	read_mapped_mem32 = read_mapped_mem32_dev;
	read_mapped_string = read_mapped_string_dev;
	return 0;
}
