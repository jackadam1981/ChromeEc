/*
 * Copyright (C) 2016 Google, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/kernel.h>
#include <asm/byteorder.h>
#include <openssl/sha.h>

#ifdef DEBUG
#define debug printf
#else
#define debug(fmt, args...)
#endif

#define __packed	__attribute__((packed))

struct upgrade_pkt {
	__be16	tag;
	__be32	length;
	__be32	ordinal;
	__be16	subcmd;
	__be32	digest;
	__be32	address;
	char data[0];
} __packed;

#define BLK_SIZE	1024
#define MAX_BUF_SIZE	(BLK_SIZE + sizeof(struct upgrade_pkt))

#define EXT_CMD		0xbaccd00a
#define FW_UPGRADE	4
#define FLASH_BASE	0x40000
#define CONFIG_RW_SIZE	0x3c000

static char outbuf[MAX_BUF_SIZE];
static char inbuf[MAX_BUF_SIZE];

static int tpm_send_pkt(int fd, unsigned int digest, unsigned int addr,
			const char *data, int size)
{
	struct upgrade_pkt *out = (struct upgrade_pkt *)outbuf;
	struct upgrade_pkt *in = (struct upgrade_pkt *)inbuf;
	unsigned int base;
	int len, done;

	len = size + sizeof(struct upgrade_pkt);

	out->tag = __cpu_to_be16(0x8001);
	out->length = __cpu_to_be32(len);
	out->ordinal = __cpu_to_be32(EXT_CMD);
	out->subcmd = __cpu_to_be16(FW_UPGRADE);
	out->digest = digest;
	out->address = __cpu_to_be32(addr);
	memcpy(out->data, data, size);
	debug("Writing %x bytes to TPM at %x\n", len, addr);
	done = write(fd, out, len);
	if (done < 0) {
		perror("Could not write to TPM");
		return -1;
	} else if (done != len) {
		fprintf(stderr, "Error: Wrote %x bytes, expected to write %x\n",
			done, len);
		return -1;
	}

	len = read(fd, in, sizeof(inbuf));
	if (len < 0) {
		perror("Could not read from TPM");
		return -1;
	}
	debug("Read %x bytes from TPM\n", len);

	base = __be32_to_cpu(in->digest);

	return base;
}

static int update_cr50(int tpm, int fd)
{
	unsigned char digest[SHA_DIGEST_LENGTH];
	uint32_t offset, data_len;
	struct timeval start, end;
	const char *image_name;
	char buf[BLK_SIZE];
	long secs, usecs;
	uint32_t addr;
	int tx_size;
	float utime;
	SHA_CTX c;
	int upto;
	int ret;

	ret = tpm_send_pkt(tpm, 0, 0, NULL, 0);
	if (ret < 0)
		return ret;
	addr = ret;
	switch (addr) {
	case 0x44000:
		image_name = "A";
		break;
	case 0x84000:
		image_name = "B";
		break;
	default:
		fprintf(stderr, "Unrecognised image address %x\n", addr);
		return -1;
	}

	offset = addr - FLASH_BASE;
	data_len = CONFIG_RW_SIZE;
	printf("Updating image %s at offset %08x, size %x\n", image_name,
	       offset, data_len);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		perror("Unable to seek to image offset");
		return -1;
	}

	gettimeofday(&start, NULL);
	upto = 0;
	while (upto < data_len) {
		uint32_t digest_val;
		uint32_t addr_be = __cpu_to_be32(addr);
		int toread;

		toread = data_len - upto;
		if (toread > BLK_SIZE)
			toread = BLK_SIZE;
		tx_size = read(fd, buf, toread);
		if (tx_size < 0) {
			perror("Unable to read from file");
			return -1;
		} else if (tx_size != toread) {
			fprintf(stderr, "Out of data at offset %d, got %d\n",
				upto, tx_size);
			return -1;
		}
		if (!SHA1_Init(&c)) {
			fprintf(stderr, "Unable to init SHA1\n");
			return -1;
		}
		if (!SHA1_Update(&c, &addr_be, sizeof(addr_be))) {
			fprintf(stderr, "Unable to update SHA1 cmd\n");
			return -1;
		}

		if (!SHA1_Update(&c, buf, tx_size)) {
			fprintf(stderr, "Unable to update SHA1 buf\n");
			return -1;
		}
		if (!SHA1_Final(digest, &c)) {
			fprintf(stderr, "Unable to get SHA1 digest\n");
			return -1;
		}

		digest_val = *(uint32_t *)digest;
		ret = tpm_send_pkt(tpm, digest_val, addr, buf, tx_size) &
				0xff;
		if (ret) {
			fprintf(stderr, "Update error code %d\n", ret);
			return -1;
		}
		addr += tx_size;
		upto += tx_size;
		printf("\r%x / %x", upto, data_len);
		fflush(stdout);
	}
	gettimeofday(&end, NULL);
	secs = end.tv_sec - start.tv_sec;
	usecs = end.tv_usec - start.tv_usec;
	utime = secs * 1000000 + usecs;

	printf("\rUpgrade completed, %d packets, %1.1fs\n",
	       (data_len + BLK_SIZE - 1) / BLK_SIZE, utime / 1000000);

	return 0;
}

static int run_update(const char *fname)
{
	int tpm;
	int ret = -1;
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("Could not open RW_A file");
		goto err;
	}

	tpm = open("/dev/tpm0", O_RDWR);
	if (tpm < 0) {
		perror("Could not open TPM");
		goto no_tpm;
	}
	debug("Opened TPM as %d\n", tpm);

	ret = update_cr50(tpm, fd);

	close(tpm);
	ret = 0;

no_tpm:
	close(fd);
err:
	return ret;
}

static int usage(const char *progname, const char *err_msg)
{
	fprintf(stderr, "Error: %s\n", err_msg);
	fprintf(stderr, "Usage: %s ec.bin\n", progname);

	return -1;
}

int main(int argc, char *argv[])
{
	int opt;

	printf("CR50 update tool\n");
	while ((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		/* Put option code here */
		default:
			return usage(argv[0], "Unknown option");
		}
	}

	if (argc - optind < 1)
		return usage(argv[0], "Not enough parameters");
	if (run_update(argv[optind]))
		return 1;

	return 0;
}
