/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <asm/byteorder.h>
#include <endian.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#ifndef __packed
#define __packed __attribute__((packed))
#endif

#include "config_chip.h"
#include "board.h"

#include "compile_time_macros.h"
#include "misc_util.h"
#include "signed_header.h"
#include "tpm_vendor_cmds.h"
#include "upgrade_fw.h"

#ifdef DEBUG
#define debug printf
#else
#define debug(fmt, args...)
#endif

enum exit_values {
	SUCCESS = 0,
	ERROR = 1,
};

const char help_str[] =
	"Commands:\n"
	"  bidget\n"
	"      Read board ID.\n"
	"  bidset <type> <flags>\n"
	"      Write board ID.\n"
	"";

struct command {
	const char *name;
	int (*handler)(int argc, char *argv[]);
};

/* Structure holding Board ID */
struct board_id {
	uint32_t type;          /* Board type */
	uint32_t type_inv;      /* Board type (inverted) */
	uint32_t flags;         /* Flags */
};

struct tpm_pkt {
	__be16	tag;
	__be32	length;
	__be32	ordinal;
	__be16	subcmd;
	char data[0];
} __packed;

#define MAX_BUF_SIZE	(sizeof(struct board_id) + sizeof(struct tpm_pkt))

/*
 * Prepare and send a TPM2 extended/vendor command to /dev/tpm0,
 * and then get a reply.
 */
static int tpm_send_pkt(int fd, const void *data, int data_size,
			void *response, size_t *response_size,
			uint16_t subcmd)
{
	static uint8_t outbuf[MAX_BUF_SIZE];
	struct tpm_pkt *out = (struct tpm_pkt *)outbuf;
	/* Use the same structure, it will not be filled completely. */
	int len, done;

	debug("%s: sending to %#x %d bytes\n", __func__, addr, size);

	len = data_size + sizeof(struct tpm_pkt);

	out->tag = htobe16(0x8001);
	out->length = htobe32(len);
	if (subcmd <= LAST_EXTENSION_COMMAND)
		out->ordinal = htobe32(CONFIG_EXTENSION_COMMAND);
	else
		out->ordinal = htobe32(TPM_CC_VENDOR_BIT_MASK);
	out->subcmd = htobe16(subcmd);
	memcpy(out->data, data, data_size);
#ifdef DEBUG
	{
		int i;

		debug("Writing %d bytes to TPM at %x\n", len, addr);
		for (i = 0; i < len; i++)
			debug("%2.2x ", outbuf[i]);
		debug("\n");
	}
#endif
	done = write(fd, out, len);
	if (done < 0) {
		perror("Could not write to TPM");
		return -1;
	} else if (done != len) {
		fprintf(stderr, "Error: Wrote %d bytes, expected to write %d\n",
			done, len);
		return -1;
	}

	/* Reuse the output buffer as the receive buffer. */
	len = read(fd, outbuf, MAX_BUF_SIZE);
#ifdef DEBUG
	debug("Read %d bytes from TPM\n", len);
	if (len > 0) {
		int i;

		for (i = 0; i < len; i++)
			debug("%2.2x ", outbuf[i]);
		debug("\n");
	}
#endif
	len = len - sizeof(struct tpm_pkt);
	if (len < 0) {
		fprintf(stderr, "Problems reading from TPM, got %d bytes\n",
			len + (int)sizeof(struct tpm_pkt));
		return -1;
	}

	len = MIN(len, *response_size);
	memcpy(response, outbuf + sizeof(struct tpm_pkt), len);
	*response_size = len;
	return 0;
}

static void print_help(const char *prog, int print_cmds)
{
	if (print_cmds)
		puts(help_str);
	else
		printf("Use '%s help' to print a list of commands\n", prog);
}

static int read_board_id(int argc, char *argv[])
{
	struct board_id id;
	size_t bid_size = sizeof(id);
	int tpm_fd;

	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		return -1;
	}

	tpm_fd = open("/dev/tpm0", O_RDWR);
	if (tpm_fd < 0) {
		perror("Could not open TPM");
		return ERROR;
	}

	memset(&id, 0, bid_size);
	if (tpm_send_pkt(tpm_fd, NULL, 0,
			 &id, &bid_size,
			 VENDOR_CC_GET_BOARD_ID) < 0) {
		fprintf(stderr, "Failed to read board ID\n");
		return ERROR;
	}

	if (bid_size != sizeof(id)) {
		fprintf(stderr, "Board ID length is incorrect\n");
		return ERROR;
	}

	printf("Board ID is read as: 0x%08x 0x%08x 0x%08x\n",
	       id.type, id.type_inv, id.flags);
	return SUCCESS;
}


static int write_board_id(int argc, char *argv[])
{
	uint8_t data[8];
	int data_size = sizeof(data);
	size_t response_size = 0;
	int tpm_fd;
	int type;
	int flags;
	char *e;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <type> <flags>\n", argv[0]);
		return -1;
	}

	tpm_fd = open("/dev/tpm0", O_RDWR);
	if (tpm_fd < 0) {
		perror("Could not open TPM");
		return ERROR;
	}

	type = strtol(argv[1], &e, 0);
	flags = strtol(argv[2], &e, 0);

	memcpy(data, &type, sizeof(type));
	memcpy(data + sizeof(type), &flags, sizeof(flags));

	if (tpm_send_pkt(tpm_fd, data, data_size,
			 NULL, &response_size,
			 VENDOR_CC_SET_BOARD_ID) < 0) {
		fprintf(stderr, "Failed to write board ID\n");
		return ERROR;
	}
	printf("New Board ID is written successfully\n");
	return SUCCESS;
}

/* NULL-terminated list of commands */
const struct command commands[] = {
	{"bidget", read_board_id},
	{"bidset", write_board_id},
	{NULL, NULL}
};

int main(int argc, char *argv[])
{
	const struct command *cmd;
	int rv = 0;

	/* 'ectool help' prints help with commands */
	if (argc == 1 || !strcasecmp(argv[1], "help")) {
		print_help(argv[0], 1);
		return ERROR;
	}

	/* Handle commands */
	for (cmd = commands; cmd->name; cmd++) {
		if (!strcasecmp(argv[1], cmd->name)) {
			rv = cmd->handler(argc - 1, argv + 1);
			goto out;
		}
	}

	/* If we're still here, command was unknown */
	fprintf(stderr, "Unknown command '%s'\n\n", argv[optind]);
	print_help(argv[0], 0);
out:
	return !!rv;
}
