/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <endian.h>
#include <errno.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm-host.h"
#include "comm-usb.h"
#include "cros_ec_dev.h"
#include "ec_commands.h"
#include "misc_util.h"
#include "update_fw.h"
#include "usb_descriptor.h"

#define USB_ERROR(m, r) \
	fprintf(stderr, "%s:%d, %s returned %d (%s)\n", __FILE__, __LINE__, \
		m, r, libusb_strerror(r))

#ifdef DEBUG
#define debug(format, arg...) printf("%s:%d: " format, __FILE__, __LINE__, ##arg)
#else
#define debug(...)
#endif

struct usb_endpoint {
	struct libusb_device_handle *devh;
	int iface_num;
	uint8_t ep_num;
	int     chunk_len;
};

struct transfer_descriptor {
	/*
	 * offsets of section available for update (not currently active).
	 */
	uint32_t offset;

	struct usb_endpoint uep;
};

static struct transfer_descriptor td;

void comm_usb_exit(void)
{
	debug("Exit\n");
	if (td.uep.iface_num)
		libusb_release_interface(td.uep.devh, td.uep.iface_num);
	if (td.uep.devh)
		libusb_close(td.uep.devh);
	libusb_exit(NULL);
}

/*
 * Actual USB transfer function, the 'allow_less' flag indicates that the
 * valid response could be shorter than allotted memory.
 *
 * Returns enum libusb_error (< 0) or 1 on success.
 */
static int do_xfer(struct usb_endpoint *uep, void *outbuf, int outlen,
		   void *inbuf, int inlen, int allow_less)
{

	int r, actual;

	/* Send data out */
	if (outbuf && outlen) {
		actual = 0;
		r = libusb_bulk_transfer(uep->devh, uep->ep_num,
					 outbuf, outlen,
					 &actual, 2000);
		if (r < 0) {
			USB_ERROR("libusb_bulk_transfer", r);
			return -EC_RES_ERROR;
		}
		if (actual != outlen) {
			fprintf(stderr, "%s:%d, only sent %d/%d bytes\n",
				__FILE__, __LINE__, actual, outlen);
			return -EC_RES_ERROR;
		}
	}
	debug("Sent %d bytes, expecting %d bytes\n", outlen, inlen);

	/* Read reply back */
	if (inbuf && inlen) {
		actual = 0;
		r = libusb_bulk_transfer(uep->devh, uep->ep_num | 0x80,
					 inbuf, inlen,
					 &actual, 5000);
		if (r < 0) {
			USB_ERROR("libusb_bulk_transfer", r);
			return -EC_RES_ERROR;
		}
		if ((actual != inlen) && !allow_less) {
			fprintf(stderr, "%s:%d, only received %d/%d bytes\n",
				__FILE__, __LINE__, actual, inlen);
			return -EC_RES_ERROR;
		}
	}

	debug("Received %d bytes\n", inlen);
	return outlen;
}

/* Return iface # or enum libusb_error (< 0) if not found. */
static int find_interface_with_endpoint(struct usb_endpoint *uep)
{
	int iface_num = -1;
	int r, i, j, k;
	struct libusb_device *dev;
	struct libusb_config_descriptor *conf = 0;
	const struct libusb_interface *iface0;
	const struct libusb_interface_descriptor *iface;
	const struct libusb_endpoint_descriptor *ep;

	dev = libusb_get_device(uep->devh);
	r = libusb_get_active_config_descriptor(dev, &conf);
	if (r < 0) {
		USB_ERROR("Failed to get_active_config", r);
		return -EC_RES_ERROR;
	}

	for (i = 0; i < conf->bNumInterfaces; i++) {
		iface0 = &conf->interface[i];
		for (j = 0; j < iface0->num_altsetting; j++) {
			iface = &iface0->altsetting[j];
			for (k = 0; k < iface->bNumEndpoints; k++) {
				ep = &iface->endpoint[k];
				if (ep->bEndpointAddress == uep->ep_num) {
					uep->chunk_len = ep->wMaxPacketSize;
					iface_num = i;
					break;
				}
			}
		}
	}

	libusb_free_config_descriptor(conf);
	return iface_num;
}

int parse_vidpid(const char *input, uint16_t *vid_ptr, uint16_t *pid_ptr)
{
	char *copy, *s, *e = 0;

	copy = strdup(input);

	s = strchr(copy, ':');
	if (!s)
		return 0;
	*s++ = '\0';

	*vid_ptr = (uint16_t) strtoull(copy, &e, 16);
	if (e && *e)
		return 0;

	*pid_ptr = (uint16_t) strtoull(s, &e, 16);
	if (e && *e)
		return 0;

	return 1;
}

static libusb_device_handle *check_device(libusb_device *dev,
					  uint16_t vid, uint16_t pid,
					  char *serialno)
{
	struct libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	char sn[256];
	int ret;
	int match = 1;
	int snvalid = 0;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0)
		return NULL;

	ret = libusb_open(dev, &handle);

	if (ret != LIBUSB_SUCCESS)
		return NULL;

	if (desc.iSerialNumber) {
		ret = libusb_get_string_descriptor_ascii(handle,
			desc.iSerialNumber, (unsigned char *)sn, sizeof(sn));
		if (ret > 0)
			snvalid = 1;
	}

	if (vid != 0 && vid != desc.idVendor)
		match = 0;
	if (pid != 0 && pid != desc.idProduct)
		match = 0;
	if (serialno != NULL && (!snvalid || strstr(sn, serialno) == NULL))
		match = 0;

	if (match)
		return handle;

	libusb_close(handle);
	return NULL;
}

static int usb_findit(uint16_t vid, uint16_t pid,
		      char *serialno, struct usb_endpoint *uep)
{
	int iface_num, r, i;
	libusb_device **devs;
	libusb_device_handle *devh = NULL;

	memset(uep, 0, sizeof(*uep));

	r = libusb_init(NULL);
	if (r < 0) {
		USB_ERROR("libusb_init", r);
		return -EC_RES_ERROR;
	}

	r = libusb_get_device_list(NULL, &devs);
	if (r < 0) {
		USB_ERROR("No device is found.\n", r);
		return -EC_RES_ERROR;
	}

	for (i = 0; devs[i]; i++) {
		devh = check_device(devs[i], vid, pid, serialno);
		if (devh) {
			debug("Found device.\n");
			break;
		}
	}

	libusb_free_device_list(devs, 1);

	if (!devh) {
		fprintf(stderr, "Can't find device\n");
		return -EC_RES_ERROR;
	}

	uep->devh = devh;
	uep->ep_num = 2; /* USB_EP_HOSTCMD */

	iface_num = find_interface_with_endpoint(uep);
	if (iface_num < 0) {
		fprintf(stderr, "USB HOSTCMD not supported by that device\n");
		return -EC_RES_ERROR;
	}

	if (!uep->chunk_len) {
		fprintf(stderr, "wMaxPacketSize isn't valid\n");
		return -EC_RES_ERROR;
	}

	debug("Found interface %d endpoint=%d, chunk_len=%d\n",
	      iface_num, uep->ep_num, uep->chunk_len);

	libusb_set_auto_detach_kernel_driver(uep->devh, 1);
	r = libusb_claim_interface(uep->devh, iface_num);
	if (r < 0) {
		USB_ERROR("libusb_claim_interface", r);
		return -EC_RES_ERROR;
	}
	uep->iface_num = iface_num;

	debug("READY\n-------\n");
	return 1;
}

static int sum_bytes(const void *data, int length)
{
	const uint8_t *bytes = (const uint8_t *)data;
	int sum = 0;
	int i;

	for (i = 0; i < length; i++)
		sum += bytes[i];
	return sum;
}

static int ec_command_usb(int command, int version,
			  const void *outdata, int outsize,
			  void *indata, int insize)
{
	struct ec_host_request *req;
	struct ec_host_response *res;
	size_t req_len, res_len;
	int rv = -EC_RES_ERROR;

	assert(outsize == 0 || outdata != NULL);
	assert(insize == 0 || indata != NULL);

	req_len = sizeof(*req) + outsize;
	req = malloc(req_len);
	res_len = sizeof(*res) + insize;
	res = malloc(res_len);
	if (req == NULL || res == NULL)
		goto out;

	req->struct_version = EC_HOST_REQUEST_VERSION;  /* 3 */
	req->checksum = 0;
	req->command = command;
	req->command_version = version;
	req->reserved = 0;
	req->data_len = outsize;
	if (outdata)
		memcpy(&req[1], outdata, outsize);
	req->checksum = (uint8_t)(-sum_bytes(req, req_len));

	memset(res, 0, res_len);

	debug("Running command 0x%04x\n", command);
	rv = do_xfer(&td.uep, req, req_len, res, res_len, 1);
	if (rv < 0) {
		comm_usb_exit();
		goto out;
	}

	if (indata)
		memcpy(indata, &res[1], insize);

out:
	if (req)
		free(req);
	if (res)
		free(res);

	debug("Command 0x%04x %s\n", command, (rv < 0) ? "failed" : "succeeded");
	return rv;
}

int comm_init_usb(uint16_t vid, uint16_t pid)
{
	memset(&td, 0, sizeof(td));

	if (usb_findit(vid, pid, NULL, &td.uep) < 0) {
		comm_usb_exit();
		return -1;
	}

	ec_command_proto = ec_command_usb;

	/* Set large size temporarily, will be updated (reduced) later. */
	ec_max_outsize = 0x400;
	ec_max_insize = 0x400;

	return 0;
}

