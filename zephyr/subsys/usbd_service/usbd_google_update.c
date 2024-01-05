/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT google_update_device

#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>
LOG_MODULE_REGISTER(usbd_google_update, LOG_LEVEL_DBG);

struct gupdate_desc {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
	struct usb_desc_header nil_desc;
} __packed;

struct gupdate_data {
	struct gupdate_desc *const desc;
	struct usbd_class_data *c_data;
	const struct usb_desc_header **const fs_desc;
};

static int usbd_gupdate_request(struct usbd_class_data *const c_data,
				struct net_buf *const buf, const int err)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
	return 0;
}

static void usbd_gupdate_sof(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void usbd_gupdate_enable(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void usbd_gupdate_disable(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void usbd_gupdate_suspended(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void usbd_gupdate_resumed(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void usbd_gupdate_reset(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void *usbd_gupdate_get_desc(struct usbd_class_data *const c_data,
				   const enum usbd_speed speed)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);

	const struct device *dev = usbd_class_get_private(c_data);
	struct gupdate_data *ddata = dev->data;

	return ddata->fs_desc;
}

static int usbd_gupdate_init(struct usbd_class_data *const c_data)
{
	LOG_ERR("Google update class %s init", c_data->name);

	return 0;
}

static void usbd_gupdate_shutdown(struct usbd_class_data *const c_data)
{
	LOG_ERR("Google update %s shutdown", c_data->name);
}

struct usbd_class_api gupdate_api = {
	.request = usbd_gupdate_request,
	.update = NULL,
	.sof = usbd_gupdate_sof,
	.enable = usbd_gupdate_enable,
	.disable = usbd_gupdate_disable,
	.suspended = usbd_gupdate_suspended,
	.resumed = usbd_gupdate_resumed,
	.reset = usbd_gupdate_reset,
	.control_to_dev = NULL,
	.control_to_host = NULL,
	.get_desc = usbd_gupdate_get_desc,
	.init = usbd_gupdate_init,
	.shutdown = usbd_gupdate_shutdown,
};

static int gupdate_device_init(const struct device *dev)
{
	LOG_ERR("Google update device %s init", dev->name);

	return 0;
}

#define DEFINE_GUPDATE_DESCRIPTOR(n)                                    \
	static struct gupdate_desc gupdate_desc_##n = {			\
	.if0 = {						\
		.bLength = sizeof(struct usb_if_descriptor),	\
		.bDescriptorType = USB_DESC_INTERFACE,		\
		.bInterfaceNumber = 0,				\
		.bAlternateSetting = 0,				\
		.bNumEndpoints = 2,				\
		.bInterfaceClass = USB_BCC_VENDOR,		\
		.bInterfaceSubClass = 0x53,			\
		.bInterfaceProtocol = 0xff,			\
		.iInterface = 0,				\
	},							\
								\
	/* Data Endpoint OUT */					\
	.if0_out_ep = {						\
		.bLength = sizeof(struct usb_ep_descriptor),	\
		.bDescriptorType = USB_DESC_ENDPOINT,		\
		.bEndpointAddress = 0x81,			\
		.bmAttributes = USB_EP_TYPE_BULK,		\
		.wMaxPacketSize = 0,				\
		.bInterval = 0x00,				\
	},							\
								\
	/* Data Endpoint IN */					\
	.if0_in_ep = {						\
		.bLength = sizeof(struct usb_ep_descriptor),	\
		.bDescriptorType = USB_DESC_ENDPOINT,		\
		.bEndpointAddress = 0x02,			\
		.bmAttributes = USB_EP_TYPE_BULK,		\
		.wMaxPacketSize = 0,				\
		.bInterval = 0x00,				\
	},							\
};             \
	const static struct usb_desc_header *gupdate_fs_desc_##n[] = {  \
		(struct usb_desc_header *)&gupdate_desc_##n.if0,        \
		(struct usb_desc_header *)&gupdate_desc_##n.if0_in_ep,  \
		(struct usb_desc_header *)&gupdate_desc_##n.if0_out_ep, \
	};

#define DEFINE_GUPDATE_CLASS_DATA(n)                    \
	static struct gupdate_data gupdate_data_##n = { \
		.desc = &gupdate_desc_##n,              \
		.fs_desc = gupdate_fs_desc_##n,         \
	};                                              \
                                                        \
	USBD_DEFINE_CLASS(gupdate_##n, &gupdate_api,    \
			  (void *)DEVICE_DT_GET(DT_DRV_INST(n)), NULL);

#define USBD_GUPDATE_INSTANCE_DEFINE(n)                                        \
	DEFINE_GUPDATE_DESCRIPTOR(n)                                           \
	DEFINE_GUPDATE_CLASS_DATA(n)                                           \
                                                                               \
	DEVICE_DT_INST_DEFINE(n, gupdate_device_init, NULL, &gupdate_data_##n, \
			      NULL, POST_KERNEL, 8, NULL);

DT_INST_FOREACH_STATUS_OKAY(USBD_GUPDATE_INSTANCE_DEFINE);