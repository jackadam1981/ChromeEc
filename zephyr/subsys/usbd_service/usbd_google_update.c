/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/usb_stream.h"
#include "google_vendor.h"

#include <zephyr/drivers/usb/udc.h>
#include <zephyr/logging/log.h>

#include <usb_descriptor.h>
LOG_MODULE_REGISTER(usbd_google_update, LOG_LEVEL_INF);

#define GOOGLE_UPDATE_EP_FS_MPS 64

static K_KERNEL_STACK_DEFINE(rx_thread_stack,
			     CONFIG_GOOGLE_UPDATE_RX_STACK_SIZE);
static struct k_thread rx_thread_data;
static K_KERNEL_STACK_DEFINE(tx_thread_stack,
			     CONFIG_GOOGLE_UPDATE_TX_STACK_SIZE);
static struct k_thread tx_thread_data;

// TODO: Think
NET_BUF_POOL_FIXED_DEFINE(gupdate_pool, 10, GOOGLE_UPDATE_EP_FS_MPS, 0, NULL);

static K_FIFO_DEFINE(rx_queue);
static K_FIFO_DEFINE(tx_queue);

static struct net_buf *gupdate_buf_alloc(const uint8_t ep)
{
	struct net_buf *buf = NULL;
	struct udc_buf_info *bi;

	buf = net_buf_alloc(&gupdate_pool, K_NO_WAIT);
	if (!buf) {
		LOG_ERR("failed to allocate memory");
		return NULL;
	}

	bi = udc_get_buf_info(buf);
	memset(bi, 0, sizeof(struct udc_buf_info));
	bi->ep = ep;

	return buf;
}

static int gupdate_out_start(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct net_buf *buf;
	uint8_t ep;
	int ret;

	if (!atomic_test_bit(&data->state, GUPDATE_DEV_CLASS_ENABLED)) {
		return -EPERM;
	}

	buf = gupdate_buf_alloc(google_get_out_ep(c_data));
	if (!buf) {
		LOG_ERR("Failed to allocate rx buffer");
		return -ENOMEM;
	}

	ret = usbd_ep_enqueue(c_data, buf);
	if (ret) {
		LOG_ERR("Failed to enqueue net_buf for endpoint 0x%02x", ep);
		net_buf_unref(buf);
	}

	return ret;
}

static void gupdate_rx_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		struct net_buf *buf;
		const struct queue *usb_to_update = usb_update.producer.queue;

		buf = net_buf_get(&rx_queue, K_FOREVER);
		if (buf->len > queue_space(usb_to_update)) {
			LOG_ERR("queue is full");
			continue;
		}
		queue_add_units(usb_to_update, buf->data, buf->len);
		LOG_HEXDUMP_DBG(buf->data, buf->len, "Gupdate Rx:");
		net_buf_unref(buf);
	}
}

static void gupdate_tx_thread(void *arg1, void *arg2, void *arg3)
{
	struct usbd_class_data *const c_data = arg1;
	struct google_data *data = usbd_class_get_private(c_data);

	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		struct net_buf *buf;
		int ret;

		buf = net_buf_get(&tx_queue, K_FOREVER);
		LOG_HEXDUMP_DBG(buf->data, buf->len, "Gupdate Tx:");

		ret = usbd_ep_enqueue(c_data, buf);
		if (ret) {
			LOG_ERR("failed to send data");
			net_buf_unref(buf);
			continue;
		}

		k_sem_take(&data->sync_sem, K_FOREVER);
		// TODO: need to free?
		//  net_buf_unref(buf);
	}
}

static int usbd_gupdate_request(struct usbd_class_data *const c_data,
				struct net_buf *const buf, const int err)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct udc_buf_info *bi;
	struct net_buf *out_buf;

	bi = udc_get_buf_info(buf);

	if (bi->ep == google_get_out_ep(c_data)) {
		/* TODO: need to check */
		out_buf = net_buf_alloc(&gupdate_pool, K_NO_WAIT);
		if (!out_buf) {
			LOG_ERR("failed to allocate rx memory");
			return -ENOMEM;
		}
		net_buf_add_mem(out_buf, buf->data, buf->len);
		net_buf_put(&rx_queue, out_buf);
		gupdate_out_start(c_data);
	}

	if (bi->ep == google_get_in_ep(c_data)) {
		/* Finish Tx */
		k_sem_give(&data->sync_sem);
	}

	return usbd_ep_buf_free(uds_ctx, buf);
}

static void usbd_gupdate_enable(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);

	atomic_set_bit(&data->state, GUPDATE_DEV_CLASS_ENABLED);

	if (gupdate_out_start(c_data)) {
		LOG_ERR("failed to start gupdate out transfer");
	}

	LOG_DBG("configuration enabled");
}

static void usbd_gupdate_disable(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);

	atomic_clear_bit(&data->state, GUPDATE_DEV_CLASS_ENABLED);

	LOG_DBG("configuration disabled");
}

static void *usbd_gupdate_get_desc(struct usbd_class_data *const c_data,
				   const enum usbd_speed speed)
{
	struct google_data *data = usbd_class_get_private(c_data);

	if (speed == USBD_SPEED_FS) {
		return data->fs_desc;
	}

	return NULL;
}

static int usbd_gupdate_init(struct usbd_class_data *const c_data)
{
	LOG_DBG("HID class %s init", c_data->name);

	return 0;
}

struct usbd_class_api gupdate_api = {
	.request = usbd_gupdate_request,
	.enable = usbd_gupdate_enable,
	.disable = usbd_gupdate_disable,
	.get_desc = usbd_gupdate_get_desc,
	.init = usbd_gupdate_init,
};

#define DEFINE_GUPDATE_DESCRIPTOR(n)                                       \
	static struct google_desc gupdate_desc_##n = {                    \
		.if0 = INITIALIZER_IF(2, USB_BCC_VENDOR,                   \
				      USB_SUBCLASS_GOOGLE_UPDATE,          \
				      USB_PROTOCOL_GOOGLE_UPDATE),         \
		.out_ep = INITIALIZER_IF_EP(AUTO_EP_OUT, USB_EP_TYPE_BULK, \
					    GOOGLE_UPDATE_EP_FS_MPS),      \
		.in_ep = INITIALIZER_IF_EP(AUTO_EP_IN, USB_EP_TYPE_BULK,   \
					   GOOGLE_UPDATE_EP_FS_MPS),       \
	};                                                                 \
	const static struct usb_desc_header *gupdate_fs_desc_##n[] = {     \
		(struct usb_desc_header *)&gupdate_desc_##n.if0,           \
		(struct usb_desc_header *)&gupdate_desc_##n.in_ep,         \
		(struct usb_desc_header *)&gupdate_desc_##n.out_ep,        \
		NULL,                                                      \
	};

#ifdef TEST_SHARED_FIFO_RACE
/* Need to disable Google I2C */
#define DEFINE_GUPDATE_CLASS_DATA(n)                             \
	static struct google_data gupdate_data_##n = {   \
		.sync_sem = Z_SEM_INITIALIZER(                   \
			gupdate_data_##n.sync_sem, 0, 1), \
		.desc = &gupdate_desc_##n,                       \
		.fs_desc = gupdate_fs_desc_##n,                  \
	};                                                       \
                                                                 \
	USBD_DEFINE_CLASS(gupdate_##n, &gupdate_api,      \
			  &gupdate_data_##n, NULL);
#else
/* Coreboot only parses the first interface descriptor for boot keyboard
 * detection. The section name format (full-speed) in RAM is
 * "._usbd_class_fs.static.<class>_<instance>_fs" and the USB descriptors
 * are sorted by name in the linker scripts. The string "vendor_gupdate_0"
 * is set to ensure that the Google update descriptor is placed after the
 * HID class.
 */
#define DEFINE_GUPDATE_CLASS_DATA(n)                             \
	static struct google_data gupdate_data_##n = {   \
		.sync_sem = Z_SEM_INITIALIZER(                   \
			gupdate_data_##n.sync_sem, 0, 1), \
		.desc = &gupdate_desc_##n,                       \
		.fs_desc = gupdate_fs_desc_##n,                  \
	};                                                       \
                                                                 \
	USBD_DEFINE_CLASS(vendor_gupdate_##n, &gupdate_api,      \
			  &gupdate_data_##n, NULL);
#endif

/*
 * Google update subsystem does not support multiple instances.
 */
DEFINE_GUPDATE_DESCRIPTOR(0)
DEFINE_GUPDATE_CLASS_DATA(0)

void updater_stream_written(struct consumer const *consumer, size_t count)
{
	static uint8_t data[GOOGLE_UPDATE_EP_FS_MPS];
	struct net_buf *buf;

	// TODO
	// if (!atomic_test_bit(&data->state, GUPDATE_DEV_CLASS_ENABLED)) {
	// 	return;
	// }

	if (queue_is_empty(consumer->queue)) {
		LOG_ERR("consumer queue is empty");
		return;
	}

	do {
		count = (count > GOOGLE_UPDATE_EP_FS_MPS) ? GOOGLE_UPDATE_EP_FS_MPS : count;
		queue_peek_units(consumer->queue, data, 0, count);
		buf = gupdate_buf_alloc(google_get_in_ep(&vendor_gupdate_0));
		if (!buf) {
			LOG_ERR("Failed to allocate tx buffer");
			return;
		}

		net_buf_add_mem(buf, data, count);
		net_buf_put(&tx_queue, buf);
		queue_advance_head(consumer->queue, count);
		count = queue_count(consumer->queue);
	} while (count != 0);
}

static int gupdate_preinit(void)
{
	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_KERNEL_STACK_SIZEOF(rx_thread_stack),
			gupdate_rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_GOOGLE_UPDATE_RX_THREAD_PRIORTY), 0,
			K_NO_WAIT);

	k_thread_name_set(&rx_thread_data, "gupdate_rx");

	k_thread_create(&tx_thread_data, tx_thread_stack,
			K_KERNEL_STACK_SIZEOF(tx_thread_stack),
			gupdate_tx_thread, (void *)&vendor_gupdate_0, NULL,
			NULL,
			K_PRIO_COOP(CONFIG_GOOGLE_UPDATE_TX_THREAD_PRIORTY), 0,
			K_NO_WAIT);

	k_thread_name_set(&tx_thread_data, "gupdate_tx");

	return 0;
}

SYS_INIT(gupdate_preinit, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
