/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <asm/byteorder.h>
#include <endian.h>
#include <fcntl.h>
#include <getopt.h>
#include <libusb.h>
#include <openssl/sha.h>
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

/*
 * This file contains the source code of a Linux application used to read
 * board ID from or write board ID to a CR50 device. The communication
 * with CR50 is implemented over /dev/tpm0.
 */

/* Structure holding Board ID */
struct board_id {
	uint32_t type;          /* Board type */
	uint32_t type_inv;      /* Board type (inverted) */
	uint32_t flags;         /* Flags */
};

enum exit_values {
	BID_SUCCESS = 0,  /* Board ID operation succeeded. */
	BID_ERROR = 1  /* Something went wrong. */
};

/*
 * Need to create an entire TPM PDU when upgrading over /dev/tpm0 and need to
 * have space to prepare the entire PDU.
 */
struct upgrade_pkt {
	__be16	tag;
	__be32	length;
	__be32	ordinal;
	__be16	subcmd;
	__be32	digest;
	__be32	address;
	char data[0];
} __packed;

#define MAX_BUF_SIZE	(SIGNED_TRANSFER_SIZE + sizeof(struct upgrade_pkt))

static char *progname;
static char *short_opts = "f:hrt:w";
static const struct option long_opts[] = {
	/* name    hasarg *flag val */
	{"flags",	1,   NULL, 'f'},
	{"help",	0,   NULL, 'h'},
	{"read_bid",	0,   NULL, 'r'},
	{"type",	1,   NULL, 't'},
	{"write_bid",	0,   NULL, 'w'},
	{},
};

/* Prepare and transfer a block to /dev/tpm0, get a reply. */
static int tpm_send_pkt(int fd, unsigned int digest, unsigned int addr,
			const void *data, int size,
			void *response, size_t *response_size,
			uint16_t subcmd)
{
	/* Used by transfer to /dev/tpm0 */
	static uint8_t outbuf[MAX_BUF_SIZE];

	struct upgrade_pkt *out = (struct upgrade_pkt *)outbuf;
	/* Use the same structure, it will not be filled completely. */
	int len, done;
	int response_offset = offsetof(struct upgrade_pkt, digest);

	debug("%s: sending to %#x %d bytes\n", __func__, addr, size);

	len = size + sizeof(struct upgrade_pkt);

	out->tag = htobe16(0x8001);
	out->length = htobe32(len);
	if (subcmd <= LAST_EXTENSION_COMMAND)
		out->ordinal = htobe32(CONFIG_EXTENSION_COMMAND);
	else
		out->ordinal = htobe32(TPM_CC_VENDOR_BIT_MASK);
	out->subcmd = htobe16(subcmd);
	out->digest = digest;
	out->address = htobe32(addr);
	memcpy(out->data, data, size);
#ifdef DEBUG
	{
		int i;

		debug("Writing %d bytes to TPM at %x\n", len, addr);
		for (i = 0; i < 20; i++)
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

	/*
	 * Let's reuse the output buffer as the receve buffer; the combined
	 * size of the two structures below is sure enough for any expected
	 * response size.
	 */
	len = read(fd, outbuf, sizeof(struct upgrade_pkt) +
		   sizeof(struct first_response_pdu));
#ifdef DEBUG
	debug("Read %d bytes from TPM\n", len);
	if (len > 0) {
		int i;

		for (i = 0; i < len; i++)
			debug("%2.2x ", outbuf[i]);
		debug("\n");
	}
#endif
	len = len - response_offset;
	if (len < 0) {
		fprintf(stderr, "Problems reading from TPM, got %d bytes\n",
			len + response_offset);
		return -1;
	}

	len = MIN(len, *response_size);
	memcpy(response, outbuf + response_offset, len);
	*response_size = len;
	return 0;
}

static void usage(int errs)
{
	printf("\nUsage: %s [options]\n"
	       "\n"
	       "This utility can read/write board ID from/to Cr50 image.\n"
	       "\n"
	       "Options:\n"
	       "\n"
	       " -f,--flags  FLAGS    The 32-bit flags field of board id\n"
	       " -h,--help            Show this message\n"
	       " -r,--read            Read board id\n"
	       " -t,--type   TYPE     The 32-bit type field of board id\n"
	       " -w,--write           Write board id, -f and -t are required\n"
	       "\n", progname);

	exit(errs ? BID_ERROR : BID_SUCCESS);
}

static void read_board_id(int tpm_fd)
{
	struct board_id id;
	size_t bid_size = sizeof(id);

	memset(&id, 0, bid_size);
	if (tpm_send_pkt(tpm_fd, 0, 0,
			 NULL, 0,
			 &id, &bid_size,
			 VENDOR_CC_GET_BOARD_ID) < 0) {
		fprintf(stderr, "Failed to read board ID\n");
		exit(BID_ERROR);
	}

	if (bid_size != sizeof(id)) {
		fprintf(stderr, "Board ID length is incorrect\n");
		exit(BID_ERROR);
	}

	printf("Board ID is read as: 0x%08x 0x%08x 0x%08x\n",
	       id.type, id.type_inv, id.flags);
}


static void write_board_id(int tpm_fd, int type, int flags)
{
	int data[2];
	int data_bytes = sizeof(data);

	data[0] = type;
	data[1] = flags;
	if (tpm_send_pkt(tpm_fd, 0, 0,
			 data, data_bytes,
			 NULL, 0,
			 VENDOR_CC_SET_BOARD_ID) < 0) {
		fprintf(stderr, "Failed to write board ID\n");
		exit(BID_ERROR);
	}
	printf("New Board ID is written successfully:\n");
	printf("type=0x%08x, flags=0x%08x\n", type, flags);
}

int main(int argc, char *argv[])
{
	int tpm_fd;
	int errorcnt;

	int board_id_type = 0xFFFFFFFF;
	int board_id_flags = 0xFFFFFFFF;
	char *e;
	int read_bid = 0;
	int write_bid = 0;
	int i;

	progname = strrchr(argv[0], '/');
	if (progname)
		progname++;
	else
		progname = argv[0];

	errorcnt = 0;
	opterr = 0;				/* quiet, you */
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'f':
			board_id_flags = strtol(optarg, &e, 0);
			if (*e) {
				printf("Invalid argument: \"%s\"\n", optarg);
				errorcnt++;
			}
			break;
		case 'h':
			usage(errorcnt);
			break;
		case 'r':
			read_bid = 1;
			break;
		case 't':
			board_id_type = strtol(optarg, &e, 0);
			if (*e) {
				printf("Invalid argument: \"%s\"\n", optarg);
				errorcnt++;
			}
			break;
		case 'w':
			write_bid = 1;
			break;
		case 0:				/* auto-handled option */
			break;
		case '?':
			if (optopt)
				printf("Unrecognized option: -%c\n", optopt);
			else
				printf("Unrecognized option: %s\n",
				       argv[optind - 1]);
			errorcnt++;
			break;
		case ':':
			printf("Missing argument to %s\n", argv[optind - 1]);
			errorcnt++;
			break;
		default:
			printf("Internal error at %s:%d\n", __FILE__, __LINE__);
			exit(BID_ERROR);
		}
	}

	if (errorcnt)
		usage(errorcnt);

	if (!read_bid && !write_bid) {
		printf("Please specify you want to read or write board id.");
		exit(BID_ERROR);
	}

	tpm_fd = open("/dev/tpm0", O_RDWR);
	if (tpm_fd < 0) {
		perror("Could not open TPM");
		exit(BID_ERROR);
	}

	if (write_bid)
		write_board_id(tpm_fd, board_id_type, board_id_flags);

	if (read_bid)
		read_board_id(tpm_fd);

	return BID_SUCCESS;
}
