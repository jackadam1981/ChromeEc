/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <endian.h>
#include <getopt.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "usb_upgrade.h"

/* Google Cr50 */
#define VID 0x18d1
#define PID 0x5014
#define SUBCLASS UNOFFICIAL_USB_SUBCLASS_GOOGLE_CR50
#define PROTOCOL 0xff

/* Globals */
static char *progname;
static char *short_opts = ":d:h";
static const struct option long_opts[] = {
	/* name    hasarg *flag val */
	{"device",   1,   NULL, 'd'},
	{"help",     0,   NULL, 'h'},
	{NULL,       0,   NULL, 0},
};

static void usage(int errs)
{
	printf("\nUsage: %s [options] ec.bin\n"
	       "\n"
	       "This updates the Cr50 RW firmware over USB.\n"
	       "The required argument is the full RO+RW image.\n"
	       "\n"
	       "Options:\n"
	       "\n"
	       "  -d,--device  VID:PID     USB device (default %04x:%04x)\n"
	       "  -h,--help                Show this message\n"
	       "\n", progname, VID, PID);

	exit(!!errs);
}

/* Read file into buffer */
static uint8_t *get_file_or_die(const char *filename, uint32_t *len_ptr)
{
	FILE *fp;
	struct stat st;
	uint8_t *data;
	uint32_t len;

	fp = fopen(filename, "rb");
	if (!fp) {
		perror(filename);
		exit(1);
	}
	if (fstat(fileno(fp), &st)) {
		perror("stat");
		exit(1);
	}

	len = st.st_size;

	data = malloc(len);
	if (!data) {
		perror("malloc");
		exit(1);
	}

	if (1 != fread(data, st.st_size, 1, fp)) {
		perror("fread");
		exit(1);
	}

	fclose(fp);

	*len_ptr = len;
	return data;
}

#define USB_ERROR(m, r) \
	fprintf(stderr, "%s:%d, %s returned %d (%s)\n", __FILE__, __LINE__, \
		m, r, libusb_strerror(r))

static void xfer(struct libusb_device_handle *devh, uint8_t ep_num,
		 void *outbuf, int outlen, void *inbuf, int inlen) {

	int r, actual;

	/* Send data out */
	if (outbuf && outlen) {

		actual = 0;
		r = libusb_bulk_transfer(devh, ep_num,
					 outbuf, outlen,
					 &actual, 1000);
		if (r < 0) {
			USB_ERROR("libusb_bulk_transfer", r);
			exit(1);
		}
		if (actual != outlen) {
			fprintf(stderr, "%s:%d, only sent %d/%d bytes\n",
				__FILE__, __LINE__, actual, outlen);
			exit(1);
		}
	}

	/* Read reply back */
	if (inbuf && inlen) {

		actual = 0;
		r = libusb_bulk_transfer(devh, ep_num | 0x80,
					 inbuf, inlen,
					 &actual, 1000);
		if (r < 0) {
			USB_ERROR("libusb_bulk_transfer", r);
			exit(1);
		}
		if (actual != inlen) {
			fprintf(stderr, "%s:%d, only received %d/%d bytes\n",
				__FILE__, __LINE__, actual, inlen);
			exit(1);
		}
	}
}


/* Return 0 on error, since it's never gonna be EP 0 */
static int find_endpoint(const struct libusb_interface_descriptor *iface,
			 uint8_t *ep_num_ptr, int *chunk_len_ptr)
{
	const struct libusb_endpoint_descriptor *ep;

	if (iface->bInterfaceClass == 255 &&
	    iface->bInterfaceSubClass == SUBCLASS &&
	    iface->bInterfaceProtocol == PROTOCOL &&
	    iface->bNumEndpoints) {
		ep = &iface->endpoint[0];
		*ep_num_ptr = (ep->bEndpointAddress & 0x7f);
		*chunk_len_ptr = ep->wMaxPacketSize;
		return 1;
	}

	return 0;
}

/* Return -1 on error */
static int find_interface(struct libusb_device_handle *devh,
			  uint8_t *ep_num_ptr, int *chunk_len_ptr)
{
	int iface_num = -1;
	int r, i, j;
	struct libusb_device *dev;
	struct libusb_config_descriptor *conf = 0;
	const struct libusb_interface *iface0;
	const struct libusb_interface_descriptor *iface;

	dev = libusb_get_device(devh);
	r = libusb_get_active_config_descriptor(dev, &conf);
	if (r < 0) {
		USB_ERROR("libusb_get_active_config_descriptor", r);
		goto out;
	}

	for (i = 0; i < conf->bNumInterfaces; i++) {
		iface0 = &conf->interface[i];
		for (j = 0; j < iface0->num_altsetting; j++) {
			iface = &iface0->altsetting[j];
			if (find_endpoint(iface, ep_num_ptr, chunk_len_ptr)) {
				iface_num = i;
				goto out;
			}
		}
	}

out:
	libusb_free_config_descriptor(conf);
	return iface_num;
}

/* Returns true if parsed. */
static int parse_vidpid(const char *input, uint16_t *vid_ptr, uint16_t *pid_ptr)
{
	char *copy, *s, *e = 0;

	copy = strdup(input);

	s = strchr(copy, ':');
	if (!s)
		return 0;
	*s++ = '\0';

	*vid_ptr = (uint16_t) strtoul(copy, &e, 16);
	if (!*optarg || (e && *e))
		return 0;

	*pid_ptr = (uint16_t) strtoul(s, &e, 16);
	if (!*optarg || (e && *e))
		return 0;

	return 1;
}


int main(int argc, char *argv[])
{
	struct libusb_device_handle *devh;
	uint32_t out;
	struct usb_upgrade_reply in;
	int iface_num, r, errorcnt, i;
	uint8_t *data = 0;
	uint32_t data_len = 0;
	uint8_t *data_ptr = 0;
	uint8_t ep_num = 0;
	int chunk_len = 0;
	uint16_t vid = VID, pid = PID;

	progname = strrchr(argv[0], '/');
	if (progname)
		progname++;
	else
		progname = argv[0];

	errorcnt = 0;
	opterr = 0;				/* quiet, you */
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'd':
			if (!parse_vidpid(optarg, &vid, &pid)) {
				printf("Invalid argument: \"%s\"\n", optarg);
				errorcnt++;
			}
			break;
		case 'h':
			usage(errorcnt);
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
			exit(1);
		}
	}

	if (errorcnt)
		usage(errorcnt);

	if (optind >= argc) {
		fprintf(stderr, "\nERROR: Missing required ec.bin file\n\n");
		usage(1);
	}

	data = get_file_or_die(argv[optind], &data_len);
	printf("read 0x%x bytes from %s\n", data_len, argv[optind]);
	if (data_len != FW_IMAGE_SIZE) {
		fprintf(stderr, "Image file is not %d bytes\n",
			FW_IMAGE_SIZE);
		exit(1);
	}

	r = libusb_init(NULL);
	if (r < 0) {
		USB_ERROR("libusb_init", r);
		exit(1);
	}

	printf("open_device %04x:%04x\n", vid, pid);
	/* NOTE: This doesn't handle multiple matches! */
	devh = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if (!devh) {
		fprintf(stderr, "can't find device\n");
		exit(1);
	}

	iface_num = find_interface(devh, &ep_num, &chunk_len);
	if (iface_num < 0) {
		fprintf(stderr, "USB FW update not supported by that device\n");
		exit(1);
	}
	if (!chunk_len) {
		fprintf(stderr, "wMaxPacketSize isn't valid\n");
		exit(1);
	}

	printf("found interface %d endpoint %d, chunk_len %d\n",
	       iface_num, ep_num, chunk_len);

	libusb_set_auto_detach_kernel_driver(devh, 1);
	r = libusb_claim_interface(devh, iface_num);
	if (r < 0) {
		USB_ERROR("libusb_claim_interface", r);
		exit(1);
	}

	printf("READY\n-------\n");

	/* Send start/erase request */
	printf("erase\n");
	out = UPGRADE_START;
	xfer(devh, ep_num, &out, sizeof(out), &in, sizeof(in));
	printf("status 0x%08x offset 0x%08x\n", in.status, in.offset);

	/* Okay? */
	if (!IS_EXPECT_RW(in.status)) {
		fprintf(stderr, "error: unexpected reply\n");
		exit(1);
	}

	printf("updating RW %c\n",
	       'A' + WHICH_IMAGE(in.status) - 0xA);
	data_ptr = data + in.offset;
	data_len = RW_SIZE;

	/* Actually, we can skip trailing chunks of 0xff */
	for (i = data_len - 1; i && data_ptr[i] == 0xff; i--)
		;
	data_len = (i + chunk_len - 1) & ~(chunk_len - 1);
	printf("sending 0x%x/0x%x bytes\n", data_len, RW_SIZE);

	/* Send the firmware image */
	for (; data_len; data_ptr += chunk_len, data_len -= chunk_len) {

		/* Show status occasionally */
		if (!(in.offset & 0x00003FFF))
			printf("offset 0x%x\n", in.offset);

		xfer(devh, ep_num, data_ptr, chunk_len, &in, sizeof(in));

		if (!IS_EXPECT_RW(in.status)) {
			fprintf(stderr, "error: status 0x%08x offset 0x%08x\n",
				in.status, in.offset);
			exit(1);
		}
	}

	printf("-------\nupdate complete\n");

	/* Send stop request */
	out = UPGRADE_DONE;
	xfer(devh, ep_num, &out, sizeof(out), &in, sizeof(in));

	/* Reply doesn't matter */
	printf("status 0x%08x offset 0x%08x\n", in.status, in.offset);

	printf("reboot\n");

	/* Send a second stop request, which should reboot without replying */
	out = UPGRADE_DONE;
	xfer(devh, ep_num, &out, sizeof(out), 0, 0);

	printf("bye\n");

	if (data)
		free(data);

	if (devh)
		libusb_close(devh);

	libusb_exit(NULL);

	return 0;
}
