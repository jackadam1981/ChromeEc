/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/usb_stream.h"
#include "usbd_init.h"

#include <zephyr/logging/log.h>

#include <usb_descriptor.h>
LOG_MODULE_REGISTER(usbd_google_update, LOG_LEVEL_INF);

UDC_BUF_POOL_DEFINE(gupdate_pool, 32, GOOGLE_EP_FS_MPS,
		    sizeof(struct udc_buf_info), NULL);

K_MSGQ_DEFINE(gupdate_evt_msgq, sizeof(struct gvendor_event), 2,
	      sizeof(uint32_t));

static struct usbd_class_data *gupdate_c_data;

static void gupdate_event_submit(struct usbd_class_data *const c_data,
				 struct net_buf *buf,
				 const enum gvendor_event_type event)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct gvendor_event evt;

	evt.buf = buf;
	evt.event = event;
	k_msgq_put(&gupdate_evt_msgq, &evt, K_NO_WAIT);
	k_work_reschedule(&data->event_work, K_NO_WAIT);
}

static struct net_buf *gupdate_buf_alloc(const uint8_t ep)
{
	struct net_buf *buf = NULL;
	struct udc_buf_info *bi;

	buf = net_buf_alloc(&gupdate_pool, K_NO_WAIT);
	if (!buf) {
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

	if (atomic_test_and_set_bit(&data->state, GVENDOR_DEV_OUT_BUSY)) {
		return -EBUSY;
	}

	buf = gupdate_buf_alloc(google_get_out_ep(c_data));
	if (!buf) {
		LOG_ERR("%s failed to allocate out buffer", c_data->name);
		return -ENOMEM;
	}

	ret = usbd_ep_enqueue(c_data, buf);
	if (ret) {
		LOG_ERR("%s failed to enqueue net_buf for endpoint 0x%02x",
			c_data->name, ep);
		atomic_clear_bit(&data->state, GVENDOR_DEV_OUT_BUSY);
		net_buf_unref(buf);
	}

	return ret;
}

static void gupdate_event_handler(struct k_work *item)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(item);
	struct google_data *data =
		CONTAINER_OF(dwork, struct google_data, event_work);
	struct usbd_class_data *c_data = data->c_data;

	struct gvendor_event evt;
	int ret;

	k_msgq_get(&gupdate_evt_msgq, &evt, K_FOREVER);

	if (!atomic_test_bit(&data->state, GVENDOR_DEV_ENABLED)) {
		return;
	}

	switch (evt.event) {
	case GVENDOR_RX_EVT:
		const struct queue *usb_to_update = usb_update.producer.queue;

		if (evt.buf->len > queue_space(usb_to_update)) {
			LOG_ERR("%s queue is full", c_data->name);
			break;
		}
		queue_add_units(usb_to_update, evt.buf->data, evt.buf->len);
		LOG_HEXDUMP_DBG(evt.buf->data, evt.buf->len, "Gupdate Rx:");
		net_buf_unref(evt.buf);
		ret = gupdate_out_start(c_data);
		if (ret) {
			LOG_ERR("%s failed to start out transfer, err %d",
				c_data->name, ret);
		}
		break;
	case GVENDOR_TX_EVT:
		LOG_HEXDUMP_DBG(evt.buf->data, evt.buf->len, "Gupdate Tx:");
		ret = usbd_ep_enqueue(c_data, evt.buf);
		if (ret) {
			LOG_ERR("%s failed to send data, ret %d", c_data->name,
				ret);
			net_buf_unref(evt.buf);
			break;
		}
		k_sem_take(&data->sync_sem, K_FOREVER);
		break;
	default:
		LOG_ERR("%s Unknown Gvendor event %d", c_data->name, evt.event);
		break;
	}
}

static ALWAYS_INLINE void gupdate_out_cb(struct usbd_class_data *const c_data,
					 struct net_buf *const buf)
{
	struct net_buf *out_buf;

	out_buf = gupdate_buf_alloc(google_get_out_ep(c_data));
	if (!out_buf) {
		LOG_ERR("%s failed to allocate rx memory", c_data->name);
		return;
	}

	if (net_buf_tailroom(out_buf) < buf->len) {
		LOG_ERR("%s buffer size is too small", c_data->name);
		return;
	}

	net_buf_add_mem(out_buf, buf->data, buf->len);
	gupdate_event_submit(c_data, out_buf, GVENDOR_RX_EVT);
	return;
}

static int usbd_gupdate_request(struct usbd_class_data *const c_data,
				struct net_buf *const buf, const int err)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
	struct udc_buf_info *bi;

	bi = udc_get_buf_info(buf);

	if (err) {
		if (err == -ECONNABORTED) {
			LOG_DBG("request ep 0x%02x, len %u cancelled", bi->ep,
				buf->len);
		} else {
			LOG_ERR("%s request ep 0x%02x, len %u failed, err %d",
				c_data->name, bi->ep, buf->len, err);
		}

		if (bi->ep == google_get_out_ep(c_data)) {
			atomic_clear_bit(&data->state, GVENDOR_DEV_OUT_BUSY);
		}

		goto ep_request_error;
	}

	if (bi->ep == google_get_out_ep(c_data)) {
		atomic_clear_bit(&data->state, GVENDOR_DEV_OUT_BUSY);
		gupdate_out_cb(c_data, buf);
	}

	if (bi->ep == google_get_in_ep(c_data)) {
		/* Finish Tx */
		k_sem_give(&data->sync_sem);
	}

ep_request_error:
	return usbd_ep_buf_free(uds_ctx, buf);
}

static void usbd_gupdate_enable(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);
	int err;

	atomic_set_bit(&data->state, GVENDOR_DEV_ENABLED);
	err = gupdate_out_start(c_data);
	if (err) {
		LOG_ERR("failed to enable %s, err %d", c_data->name, err);
		atomic_clear_bit(&data->state, GVENDOR_DEV_ENABLED);
		return;
	}

	LOG_DBG("%s configuration enabled", c_data->name);
}

static void usbd_gupdate_disable(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);

	atomic_clear_bit(&data->state, GVENDOR_DEV_ENABLED);

	LOG_DBG("%s configuration disabled", c_data->name);
}

static void *usbd_gupdate_get_desc(struct usbd_class_data *const c_data,
				   const enum usbd_speed speed)
{
	struct google_data *data = usbd_class_get_private(c_data);

	if (speed == USBD_SPEED_FS) {
		return data->fs_desc;
	}

	LOG_ERR("%s only supports full-speed", c_data->name);

	return NULL;
}

static int usbd_gupdate_init(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);

	data->c_data = c_data;
	gupdate_c_data = c_data;
	k_work_init_delayable(&data->event_work, gupdate_event_handler);

	LOG_DBG("Google class %s init", c_data->name);

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
	static struct google_desc gupdate_desc_##n = {                     \
		.if0 = INITIALIZER_IF(2, USB_BCC_VENDOR,                   \
				      USB_SUBCLASS_GOOGLE_UPDATE,          \
				      USB_PROTOCOL_GOOGLE_UPDATE),         \
		.out_ep = INITIALIZER_IF_EP(AUTO_EP_OUT, USB_EP_TYPE_BULK, \
					    GOOGLE_EP_FS_MPS),             \
		.in_ep = INITIALIZER_IF_EP(AUTO_EP_IN, USB_EP_TYPE_BULK,   \
					   GOOGLE_EP_FS_MPS),              \
	};                                                                 \
	const static struct usb_desc_header *gupdate_fs_desc_##n[] = {     \
		(struct usb_desc_header *)&gupdate_desc_##n.if0,           \
		(struct usb_desc_header *)&gupdate_desc_##n.in_ep,         \
		(struct usb_desc_header *)&gupdate_desc_##n.out_ep,        \
		NULL,                                                      \
	};

/* Coreboot only parses the first interface descriptor for boot keyboard
 * detection. The section name format (full-speed) in RAM is
 * "._usbd_class_fs.static.<class>_<instance>_fs" and the USB descriptors
 * are sorted by name in the linker scripts. The string "vendor_gupdate_0"
 * is set to ensure that the Google update descriptor is placed after the
 * HID class.
 */
#define DEFINE_GUPDATE_CLASS_DATA(n)                                           \
	static struct google_data gupdate_data_##n = {                         \
		.sync_sem =                                                    \
			Z_SEM_INITIALIZER(gupdate_data_##n.sync_sem, 0, 1),    \
		.desc = &gupdate_desc_##n,                                     \
		.fs_desc = gupdate_fs_desc_##n,                                \
	};                                                                     \
                                                                               \
	USBD_DEFINE_CLASS(vendor_gupdate_##n, &gupdate_api, &gupdate_data_##n, \
			  NULL);

/*
 * Google update subsystem does not support multiple instances.
 */
DEFINE_GUPDATE_DESCRIPTOR(0)
DEFINE_GUPDATE_CLASS_DATA(0)

void usb_update_stream_written(struct consumer const *consumer, size_t count)
{
	struct net_buf *buf;

	if (queue_is_empty(consumer->queue)) {
		LOG_ERR("usb_update consumer queue is empty");
		return;
	}

	do {
		count = (count > GOOGLE_EP_FS_MPS) ? GOOGLE_EP_FS_MPS : count;
		buf = gupdate_buf_alloc(google_get_in_ep(&vendor_gupdate_0));
		if (!buf) {
			LOG_ERR("usb_update failed to allocate tx buffer");
			return;
		}

		buf->len = count;
		queue_remove_units(consumer->queue, buf->data, count);
		gupdate_event_submit(gupdate_c_data, buf, GVENDOR_TX_EVT);
		count = queue_count(consumer->queue);
	} while (count != 0);
}
