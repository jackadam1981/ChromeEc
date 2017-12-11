/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "stddef.h"
#include "common.h"
#include "config.h"
#include "link_defs.h"
#include "registers.h"
#include "util.h"
#include "usb_api.h"
#include "usb_hw.h"
#include "usb-isochronous.h"


/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)


/* Currently, we only support TX direction for USB isochronous transfer.
 *
 * According to RM0091, isochronous transfer is always double buffered.
 * Addresses of buffers are pointed by `btable_ep[<endpoint>].tx_addr` and
 * `btable_ep[<endpoint>].rx_addr`.
 *
 * DTOG | USB Buffer | App Buffer
 * -----+------------+-----------
 *   0  | tx_addr    | rx_addr
 *   1  | rx_addr    | tx_addr
 *
 * That is, when DTOG bit is 0 (see `GET_TX_DTOG()`), USB hardware will read
 * from `tx_addr`, and our application can write new data to `rx_addr` at the
 * same time.
 *
 * Number of bytes in each buffer shall be tracked by `tx_count` and `rx_count`
 * respectively.
 *
 * `APP_ADDR()`, `APP_COUNT()`, `SET_APP_ADDR()`, `SET_APP_COUNT()` help you to
 * to select the correct variable to use by given DTOG value, which is available
 * by `GET_TX_DTOG()`.
 */
#define GET_TX_DTOG(USB_ISOCHRONOUS_CONFIG_PTR) \
	((STM32_USB_EP((USB_ISOCHRONOUS_CONFIG_PTR)->endpoint) & EP_TX_DTOG))

#define APP_ADDR(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE) \
	(DTOG_VALUE ?			\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_addr :	\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_addr)
#define APP_COUNT(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE) \
	(DTOG_VALUE ?			\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_count :	\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_count)

#define SET_APP_ADDR(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE, V) \
	(DTOG_VALUE ?			\
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_addr = (V)) : \
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_addr = (V)))
#define SET_APP_COUNT(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE, V) \
	(DTOG_VALUE ?			\
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_count = (V)) : \
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_count = (V)))

#if 0
/* Write 1 @object into @config queue.
 *
 * If there is one *entire* object in the queue, that object will be removed and
 * replaced by new object.
 * If current object in queue is partially transmitted, the new object cannot be
 * pushed into queue.
 * */
size_t usb_isochronous_write_queue(struct usb_isochronous_config const *config,
				   void *object)
{
	if (queue_count(config->queue) == config->object_size) {
		// entire object is in the queue, we can just drop it.
		CPRINTF("%s: pop old object\n", __func__);
		queue_advance_head(config->queue, config->object_size);
	} else if (queue_count(config->queue) > 0) {
		CPRINTF("%s: ongoing transmit, cannot write queue\n",
			__func__);
		// there are some bytes left in the queue, we need to wait until
		// they got transmitted.
		return 0;
	}

	queue_add_units(config->queue, object, config->object_size);
	config->state->object_offset = 0;
	return 1;
}
#endif

void usb_isochronous_init(struct usb_isochronous_config const *config)
{
	int ep = config->endpoint;

	SET_APP_ADDR(config, 0, usb_sram_addr(config->tx_ram_1));
	SET_APP_COUNT(config, 0, 0);
	SET_APP_ADDR(config, 1, usb_sram_addr(config->tx_ram_0));
	SET_APP_COUNT(config, 1, 0);

	STM32_USB_EP(ep) = ((ep <<  0) | /* Endpoint Addr */
			    EP_TX_VALID | /* start transmit */
			    (2 <<  9) | /* ISO EP */
			    EP_RX_DISAB);
}


void usb_isochronous_event(struct usb_isochronous_config const *config,
			   enum usb_ep_event evt)
{
	if (evt == USB_EVENT_RESET)
		usb_isochronous_init(config);
}


void usb_isochronous_tx(struct usb_isochronous_config const *config)
{
	/* Clear CTR_TX, note that EP_TX_VALID will *NOT* be cleared by
	 * hardware, so we don't need to toggle it.
	 */
	STM32_TOGGLE_EP(config->endpoint, 0, 0, 0);
	/* Clear buffer count */
	SET_APP_COUNT(config, GET_TX_DTOG(config), 0);

	hook_call_deferred(config->deferred, 0);
}


void usb_isochronous_deferred(struct usb_isochronous_config const *config)
{
	const int dtog_value = GET_TX_DTOG(config);
	uintptr_t app_addr = APP_ADDR(config, dtog_value);
	size_t count = config->tx_callback(app_addr, config->tx_size);
#if 0
	/* Transfer object_offset, so client can reconstruct the object. */
	memcpy_to_usbram((void *) app_addr, &config->state->object_offset, 1);

	count = queue_remove_memcpy(config->queue,
				    (void *) (app_addr + 1),
				    MIN(config->object_size,
					config->tx_size - 1),
				    memcpy_to_usbram);

	config->state->object_offset += count;
	app_count += count + 1;
#endif
	SET_APP_COUNT(config, dtog_value, count);
}
