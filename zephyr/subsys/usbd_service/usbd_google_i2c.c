/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/usb_stream.h"
#include "usbd_init.h"

#include <zephyr/logging/log.h>

#include <usb_descriptor.h>
LOG_MODULE_REGISTER(usbd_google_i2c, LOG_LEVEL_INF);

static K_KERNEL_STACK_DEFINE(rx_thread_stack, CONFIG_GOOGLE_I2C_RX_STACK_SIZE);
static struct k_thread rx_thread_data;
static K_KERNEL_STACK_DEFINE(tx_thread_stack, CONFIG_GOOGLE_I2C_TX_STACK_SIZE);
static struct k_thread tx_thread_data;

// TODO: Think
NET_BUF_POOL_FIXED_DEFINE(gi2c_pool, 10, GOOGLE_EP_FS_MPS,
			  sizeof(struct udc_buf_info), NULL);

static K_FIFO_DEFINE(rx_queue);
static K_FIFO_DEFINE(tx_queue);

static struct net_buf *gi2c_buf_alloc(const uint8_t ep)
{
	struct net_buf *buf = NULL;
	struct udc_buf_info *bi;

	buf = net_buf_alloc(&gi2c_pool, K_NO_WAIT);
	if (!buf) {
		LOG_ERR("failed to allocate memory");
		return NULL;
	}

	bi = udc_get_buf_info(buf);
	memset(bi, 0, sizeof(struct udc_buf_info));
	bi->ep = ep;

	return buf;
}

static int gi2c_out_start(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct net_buf *buf;
	uint8_t ep;
	int ret;

	if (!atomic_test_bit(&data->state, GVENDOR_DEV_ENABLED)) {
		return -EPERM;
	}

	if (atomic_test_and_set_bit(&data->state, GVENDOR_DEV_OUT_BUSY)) {
		return -EBUSY;
	}

	buf = gi2c_buf_alloc(google_get_out_ep(c_data));
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

static void gi2c_rx_thread(void *arg1, void *arg2, void *arg3)
{
	struct usbd_class_data *const c_data = arg1;
	struct google_data *data = usbd_class_get_private(c_data);

	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		struct net_buf *buf;
		const struct queue *usb_to_i2c = i2c_usb_.producer.queue;

		buf = k_fifo_get(&rx_queue, K_FOREVER);

		if (!atomic_test_bit(&data->state, GVENDOR_DEV_ENABLED)) {
			continue;
		}

		if (buf->len > queue_space(usb_to_i2c)) {
			LOG_ERR("queue is full");
			continue;
		}
		queue_add_units(usb_to_i2c, buf->data, buf->len);
		LOG_HEXDUMP_DBG(buf->data, buf->len, "Gi2c Rx:");
		net_buf_unref(buf);

		atomic_clear_bit(&data->state, GVENDOR_DEV_OUT_BUSY);
		gi2c_out_start(c_data);
	}
}

static void gi2c_tx_thread(void *arg1, void *arg2, void *arg3)
{
	struct usbd_class_data *const c_data = arg1;
	struct google_data *data = usbd_class_get_private(c_data);

	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		struct net_buf *buf;
		int ret;

		buf = k_fifo_get(&tx_queue, K_FOREVER);

		if (!atomic_test_bit(&data->state, GVENDOR_DEV_ENABLED)) {
			continue;
		}

		LOG_HEXDUMP_DBG(buf->data, buf->len, "Gi2c Tx:");

		ret = usbd_ep_enqueue(c_data, buf);
		if (ret) {
			LOG_ERR("failed to send Gi2c data, ret %d", ret);
			net_buf_unref(buf);
			continue;
		}

		k_sem_take(&data->sync_sem, K_FOREVER);
	}
}

static int gi2c_out_cb(struct usbd_class_data *const c_data,
		       struct net_buf *const buf, const int err)
{
	struct net_buf *out_buf;

	/* TODO: need to check */
	out_buf = net_buf_alloc(&gi2c_pool, K_NO_WAIT);
	if (!out_buf) {
		LOG_ERR("failed to allocate rx memory");
		return -ENOMEM;
	}

	if (net_buf_tailroom(out_buf) < buf->len) {
		LOG_ERR("%s ITE Debug %d", __func__, __LINE__);
		return -ENOMEM;
	}

	net_buf_add_mem(out_buf, buf->data, buf->len);
	k_fifo_put(&rx_queue, out_buf);
	return 0;
}

static int usbd_gi2c_request(struct usbd_class_data *const c_data,
			     struct net_buf *const buf, const int err)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct udc_buf_info *bi;

	bi = udc_get_buf_info(buf);

	if (err) {
		if (err == -ECONNABORTED) {
			LOG_DBG("request ep 0x%02x, len %u cancelled",
				bi->ep, buf->len);
		} else {
			LOG_ERR("request ep 0x%02x, len %u failed, err %d",
				bi->ep, buf->len, err);
		}

		if (bi->ep == google_get_out_ep(c_data)) {
			atomic_clear_bit(&data->state, GVENDOR_DEV_OUT_BUSY);
		}

		goto ep_request_error;
	}

	if (bi->ep == google_get_out_ep(c_data)) {
		gi2c_out_cb(c_data, buf, err);
	}

	if (bi->ep == google_get_in_ep(c_data)) {
		/* Finish Tx */
		k_sem_give(&data->sync_sem);
	}

ep_request_error:
	return usbd_ep_buf_free(uds_ctx, buf);
}

static void usbd_gi2c_enable(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);

	atomic_set_bit(&data->state, GVENDOR_DEV_ENABLED);

	if (gi2c_out_start(c_data)) {
		LOG_ERR("failed to start gi2c out transfer");
	}

	LOG_DBG("configuration enabled");
}

static void usbd_gi2c_disable(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);

	atomic_clear_bit(&data->state, GVENDOR_DEV_ENABLED);

	LOG_DBG("configuration disabled");
}

static void *usbd_gi2c_get_desc(struct usbd_class_data *const c_data,
				const enum usbd_speed speed)
{
	struct google_data *data = usbd_class_get_private(c_data);

	if (speed == USBD_SPEED_FS) {
		return data->fs_desc;
	}

	LOG_ERR("Gi2c only supports full-speed");

	return NULL;
}

static int usbd_gi2c_init(struct usbd_class_data *const c_data)
{
	LOG_DBG("Google class %s init", c_data->name);

	return 0;
}

struct usbd_class_api gi2c_api = {
	.request = usbd_gi2c_request,
	.enable = usbd_gi2c_enable,
	.disable = usbd_gi2c_disable,
	.get_desc = usbd_gi2c_get_desc,
	.init = usbd_gi2c_init,
};

#define DEFINE_GI2C_DESCRIPTOR(n)                                          \
	static struct google_desc gi2c_desc_##n = {                        \
		.if0 = INITIALIZER_IF(2, USB_BCC_VENDOR,                   \
				      USB_SUBCLASS_GOOGLE_I2C,             \
				      USB_PROTOCOL_GOOGLE_I2C),            \
		.out_ep = INITIALIZER_IF_EP(AUTO_EP_OUT, USB_EP_TYPE_BULK, \
					    GOOGLE_EP_FS_MPS),             \
		.in_ep = INITIALIZER_IF_EP(AUTO_EP_IN, USB_EP_TYPE_BULK,   \
					   GOOGLE_EP_FS_MPS),              \
	};                                                                 \
	const static struct usb_desc_header *gi2c_fs_desc_##n[] = {        \
		(struct usb_desc_header *)&gi2c_desc_##n.if0,              \
		(struct usb_desc_header *)&gi2c_desc_##n.in_ep,            \
		(struct usb_desc_header *)&gi2c_desc_##n.out_ep,           \
		NULL,                                                      \
	};

/* Coreboot only parses the first interface descriptor for boot keyboard
 * detection. The section name format (full-speed) in RAM is
 * "._usbd_class_fs.static.<class>_<instance>_fs" and the USB descriptors
 * are sorted by name in the linker scripts. The string "vendor_gi2c_0"
 * is set to ensure that the Google i2c descriptor is placed after the
 * HID class.
 */
#define DEFINE_GI2C_CLASS_DATA(n)                                            \
	static struct google_data gi2c_data_##n = {                          \
		.sync_sem = Z_SEM_INITIALIZER(gi2c_data_##n.sync_sem, 0, 1), \
		.desc = &gi2c_desc_##n,                                      \
		.fs_desc = gi2c_fs_desc_##n,                                 \
	};                                                                   \
                                                                             \
	USBD_DEFINE_CLASS(vendor_gi2c_##n, &gi2c_api, &gi2c_data_##n, NULL);

/*
 * Google I2C subsystem does not support multiple instances.
 */
DEFINE_GI2C_DESCRIPTOR(0)
DEFINE_GI2C_CLASS_DATA(0)

void i2c_usb__stream_written(struct consumer const *consumer, size_t count)
{
	static uint8_t data[GOOGLE_EP_FS_MPS];
	struct net_buf *buf;

	if (queue_is_empty(consumer->queue)) {
		LOG_ERR("consumer queue is empty");
		return;
	}

	do {
		count = (count > GOOGLE_EP_FS_MPS) ? GOOGLE_EP_FS_MPS : count;
		queue_peek_units(consumer->queue, data, 0, count);
		buf = gi2c_buf_alloc(google_get_in_ep(&vendor_gi2c_0));
		if (!buf) {
			LOG_ERR("Failed to allocate tx buffer");
			return;
		}

		net_buf_add_mem(buf, data, count);
		k_fifo_put(&tx_queue, buf);
		queue_advance_head(consumer->queue, count);
		count = queue_count(consumer->queue);
	} while (count != 0);
}

static int gi2c_preinit(void)
{
	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_KERNEL_STACK_SIZEOF(rx_thread_stack), gi2c_rx_thread,
			(void *)&vendor_gi2c_0, NULL, NULL,
			K_PRIO_COOP(CONFIG_GOOGLE_I2C_RX_THREAD_PRIORTY), 0,
			K_NO_WAIT);

	k_thread_name_set(&rx_thread_data, "gi2c_rx");

	k_thread_create(&tx_thread_data, tx_thread_stack,
			K_KERNEL_STACK_SIZEOF(tx_thread_stack), gi2c_tx_thread,
			(void *)&vendor_gi2c_0, NULL, NULL,
			K_PRIO_COOP(CONFIG_GOOGLE_I2C_TX_THREAD_PRIORTY), 0,
			K_NO_WAIT);

	k_thread_name_set(&tx_thread_data, "gi2c_tx");
	return 0;
}

SYS_INIT(gi2c_preinit, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
