
#ifdef SECTION_IS_RW


/* Universal Serial Bus Device Class Specification for Device Firmware Upgrade
 * Version 1.1
 * https://www.usb.org/sites/default/files/DFU_1.1.pdf
 */
#include "common.h"
#include "usb_descriptor.h"
#include "usb_hw.h"
#include "registers.h"


#include "dfu_bootmanager.h"
#include "system.h"

#define USB_SUBCLASS_DFU          (0x01)
#define USB_PROTOCOL_DFU_RUNTIME  (0x01)

#define USB_DFU_DESC_ATTRIBUTES_CAN_DOWNLOAD       (1 << 0)
#define USB_DFU_DESC_ATTRIBUTES_CAN_UPLOAD         (1 << 1)
#define USB_DFU_DESC_ATTRIBUTES_MANIFEST_TOLERANT  (1 << 2)
#define USB_DFU_DESC_ATTRIBUTES_WILL_DETACH        (1 << 3)

#define USB_DFU_DESC_ATTRIBUTES \
	(USB_DFU_DESC_ATTRIBUTES_CAN_DOWNLOAD | \
	USB_DFU_DESC_ATTRIBUTES_CAN_UPLOAD | \
	USB_DFU_DESC_ATTRIBUTES_WILL_DETACH)

#define USB_DFU_DESC_SIZE           (9)
#define USB_DFU_DESC_FUNCTIONAL     (0x21)
#define USB_DFU_DESC_DETACH_TIMEOUT (0xffff)
#define USB_DFU_DESC_TRANSFER_SIZE  (64)
#define USB_DFU_DESC_DFU_VERSION    (0x0022)

/* DFU states */
#define DFU_STATE_APP_IDLE   (0)
#define DFU_STATE_APP_DETACH (1)

/* DFU status */
#define DFU_STATUS_OK        (0)

/* DFU Request types */
#define DFU_DETACH     (0)
#define DFU_DNLOAD     (1)
#define DFU_UPLOAD     (2)
#define DFU_GET_STATUS (3)
#define DFU_CLR_STATUS (5)
#define DFU_GET_STATE  (5)
#define DFU_ABORT      (6)

/* DFU Functional Descriptor  */
struct usb_dfu_functional_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bmAttributes;
	uint16_t wDetachTimeOut;
	uint16_t wTransferSize;
	uint16_t bcdDFUVersion;
} __packed;

/* DFU response packets */
struct dfu_get_status_response {
	uint8_t bStatus;
	uint8_t bwPollTimeout[3];
	uint8_t bState;
	uint8_t iString;
} __packed;

struct dfu_get_state_response {
	uint8_t bState;
} __packed;

/* DFU Run-Time Descriptor Set. */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_DFU) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = USB_IFACE_DFU,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_APP_SPEC,
	.bInterfaceSubClass = USB_SUBCLASS_DFU,
	.bInterfaceProtocol = USB_PROTOCOL_DFU_RUNTIME,
	.iInterface = USB_STR_DFU_NAME,
};

/* DFU Functional Descriptor. */
const struct usb_dfu_functional_descriptor USB_CUSTOM_DESC_VAR(USB_IFACE_DFU,
						dfu, dfu_func_desc) = {
	.bLength = USB_DFU_DESC_SIZE,
	.bDescriptorType = USB_DFU_DESC_FUNCTIONAL,
	.bmAttributes = USB_DFU_DESC_ATTRIBUTES,
	.wDetachTimeOut = USB_DFU_DESC_DETACH_TIMEOUT,
	.wTransferSize = USB_DFU_DESC_TRANSFER_SIZE,
	.bcdDFUVersion = USB_DFU_DESC_DFU_VERSION,
};

static int dfu_runtime_request(usb_uint *ep0_buf_rx, usb_uint *ep0_buf_tx) {
	struct usb_setup_packet packet;
	usb_read_setup_packet(ep0_buf_rx, &packet);
	btable_ep[0].tx_count = 0;
	if ((packet.bmRequestType == (USB_DIR_OUT | USB_TYPE_STANDARD | \
			USB_RECIP_INTERFACE)) && \
			(packet.bRequest == USB_REQ_SET_INTERFACE)) {
		/* ACK the change alternative mode request. */
		STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
		return 0;
	}
	if ((packet.bmRequestType == (USB_DIR_OUT | USB_TYPE_CLASS | \
				USB_RECIP_INTERFACE)) && \
		(packet.bRequest == DFU_DETACH)) {
		/*
		 * Host is requesting a jump from application to DFU mode.
		 * We'll acknowlege the packet and reset.
		 */
		STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
		usleep(MSEC);
		return dfu_bootmanager_enter_dfu();
	}
	if (packet.bmRequestType == (USB_DIR_IN | USB_TYPE_CLASS | \
				USB_RECIP_INTERFACE)) {
		if (packet.bRequest == DFU_GET_STATUS) {
			/* Return the Get Status response. */
			struct dfu_get_status_response response = {
				.bStatus = DFU_STATUS_OK,
				.bState = DFU_STATE_APP_IDLE,
			};
			memcpy_to_usbram((void *) usb_sram_addr(ep0_buf_tx),
				&response, sizeof(response));
			btable_ep[0].tx_count = sizeof(response);
			STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
			return 0;
		}
		if (packet.bRequest == DFU_GET_STATE) {
			/* Return the Get State response. */
			struct dfu_get_state_response response = {
				.bState = DFU_STATE_APP_IDLE,
			};
			memcpy_to_usbram((void *) usb_sram_addr(ep0_buf_tx),
				&response, sizeof(response));
			btable_ep[0].tx_count = sizeof(response);
			STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
			return 0;
		}
	}
	/* Return a stall response for any unhandled packets. */
	STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_RX_VALID | EP_TX_STALL, 0);
	return 0;
}

USB_DECLARE_IFACE(USB_IFACE_DFU, dfu_runtime_request)

#endif /* SECTION_IS_RW */
