/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT google_update_device

#include "drivers/usb_stream.h"

#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/usb/udc.h>
LOG_MODULE_REGISTER(usbd_google_update, LOG_LEVEL_INF);

// TODO: Think
NET_BUF_POOL_FIXED_DEFINE(update_rx_pool, 2, 128, 64, NULL);

struct gupdate_desc {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor out_ep;
	struct usb_ep_descriptor in_ep;
} __packed;

struct gupdate_data {
	struct gupdate_desc *const desc;
	struct usbd_class_data *c_data;
	const struct usb_desc_header **const fs_desc;
	atomic_t state;
};

enum {
	GUPDATE_DEV_CLASS_ENABLED,
};

static inline uint8_t gupdate_get_in_ep(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gupdate_data *ddata = dev->data;
	struct gupdate_desc *desc = ddata->desc;

	return desc->in_ep.bEndpointAddress;
}

static inline uint8_t gupdate_get_out_ep(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gupdate_data *ddata = dev->data;
	struct gupdate_desc *desc = ddata->desc;

	return desc->out_ep.bEndpointAddress;
}

static struct net_buf *gupdate_buf_alloc(struct gupdate_data *const ddata,
				     const uint8_t ep)
{
	struct net_buf *buf = NULL;
	struct udc_buf_info *bi;

	buf = net_buf_alloc(&update_rx_pool, K_NO_WAIT);
	if (!buf) {
		LOG_ERR("failed to allocate buffer");
		return NULL;
	}

	bi = udc_get_buf_info(buf);
	memset(bi, 0, sizeof(struct udc_buf_info));
	bi->ep = ep;

	return buf;
}

struct usbd_class_data *temp_c_data = NULL;
void updater_stream_written(struct consumer const *consumer, size_t count)
{
	static uint8_t data[64];

	while (!queue_is_empty(consumer->queue)) {
		if (count > 64) {
			LOG_ERR("invaild data count");
			return;
		}

		queue_peek_units(consumer->queue, data, 0, count);

		/* TODO: GOTO thread */
		struct net_buf *buf;
		const struct device *dev = usbd_class_get_private(temp_c_data);
		struct gupdate_data *ddata = dev->data;

		buf = gupdate_buf_alloc(ddata, gupdate_get_in_ep(temp_c_data));
		if (buf == NULL) {
			LOG_ERR("Failed to allocate buffer");
			return;
		}

		net_buf_add_mem(buf, data, count);
		usbd_ep_enqueue(temp_c_data, buf);
		LOG_HEXDUMP_DBG(buf->data, buf->len, "Tx:");


		queue_advance_head(consumer->queue, count);
	}
}

static int gupdate_out_start(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gupdate_data *ddata = dev->data;
	struct net_buf *buf;
	uint8_t ep;
	int ret;

	if (!atomic_test_bit(&ddata->state, GUPDATE_DEV_CLASS_ENABLED)) {
		return -EPERM;
	}

	buf = gupdate_buf_alloc(ddata, gupdate_get_out_ep(c_data));
	if (!buf) {
		return -ENOMEM;
	}

	ret = usbd_ep_enqueue(c_data, buf);
	if (ret) {
		LOG_ERR("Failed to enqueue net_buf for endpoint 0x%02x", ep);
		net_buf_unref(buf);
	}

	return  ret;
}

static int usbd_gupdate_request(struct usbd_class_data *const c_data,
				struct net_buf *const buf, const int err)
{
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct udc_buf_info *bi;

	bi = udc_get_buf_info(buf);

	if (bi->ep == gupdate_get_out_ep(c_data)) {
		const struct queue *usb_to_update = usb_update.producer.queue;

		LOG_HEXDUMP_DBG(buf->data, buf->len, "Rx:");
		queue_add_units(usb_to_update, buf->data, buf->len);

		net_buf_unref(buf);
		return gupdate_out_start(c_data);
	}

	if (bi->ep == gupdate_get_in_ep(c_data)) {
		/* Finish Tx */
	}

	return usbd_ep_buf_free(uds_ctx, buf);
}

static void usbd_gupdate_enable(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gupdate_data *ddata = dev->data;

	atomic_set_bit(&ddata->state, GUPDATE_DEV_CLASS_ENABLED);

	if (gupdate_out_start(c_data)) {
		LOG_ERR("failed to start gupdate out transfer");
	}

	LOG_DBG("configuration enabled");
}

static void usbd_gupdate_disable(struct usbd_class_data *const c_data)
{
	const struct device *dev = usbd_class_get_private(c_data);
	struct gupdate_data *ddata = dev->data;

	atomic_clear_bit(&ddata->state, GUPDATE_DEV_CLASS_ENABLED);

	LOG_DBG("configuration disabled");
}

static void usbd_gupdate_suspended(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void usbd_gupdate_resumed(struct usbd_class_data *const c_data)
{
	LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
}

static void *usbd_gupdate_get_desc(struct usbd_class_data *const c_data,
				   const enum usbd_speed speed)
{
	LOG_ERR("%s ITE Debug %d speed %d", __func__, __LINE__, speed);

	const struct device *dev = usbd_class_get_private(c_data);
	struct gupdate_data *ddata = dev->data;

	/* TODO */
	// if (speed != USBD_SPEED_FS) {

	// }

	return ddata->fs_desc;
}

static int usbd_gupdate_init(struct usbd_class_data *const c_data)
{
	LOG_ERR("Google update class %s init", c_data->name);
	temp_c_data = c_data;

	return 0;
}

static void usbd_gupdate_shutdown(struct usbd_class_data *const c_data)
{
	LOG_ERR("Google update %s shutdown", c_data->name);
}

struct usbd_class_api gupdate_api = {
	.request = usbd_gupdate_request,
	.update = NULL,
	.enable = usbd_gupdate_enable,
	.disable = usbd_gupdate_disable,
	.suspended = usbd_gupdate_suspended,
	.resumed = usbd_gupdate_resumed,
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
	.out_ep = {						\
		.bLength = sizeof(struct usb_ep_descriptor),	\
		.bDescriptorType = USB_DESC_ENDPOINT,		\
		.bEndpointAddress = 0x02,			\
		.bmAttributes = USB_EP_TYPE_BULK,		\
		.wMaxPacketSize = sys_cpu_to_le16(64),				\
		.bInterval = 0x00,				\
	},							\
								\
	/* Data Endpoint IN */					\
	.in_ep = {						\
		.bLength = sizeof(struct usb_ep_descriptor),	\
		.bDescriptorType = USB_DESC_ENDPOINT,		\
		.bEndpointAddress = 0x81,			\
		.bmAttributes = USB_EP_TYPE_BULK,		\
		.wMaxPacketSize = sys_cpu_to_le16(64),				\
		.bInterval = 0x00,				\
	},							\
};             \
	const static struct usb_desc_header *gupdate_fs_desc_##n[] = {  \
		(struct usb_desc_header *)&gupdate_desc_##n.if0,        \
		(struct usb_desc_header *)&gupdate_desc_##n.in_ep,  \
		(struct usb_desc_header *)&gupdate_desc_##n.out_ep, \
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