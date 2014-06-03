/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB definitions.
 */

#ifndef USB_H
#define USB_H

#include <stddef.h> /* for wchar_t */

/* USB 2.0 chapter 9 definitions */
#define USB_DT_DEVICE                     0x01
#define USB_DT_CONFIGURATION              0x02
#define USB_DT_STRING                     0x03
#define USB_DT_INTERFACE                  0x04
#define USB_DT_ENDPOINT                   0x05

/* USB Device Descriptor */
struct usb_device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} __packed;
#define USB_DT_DEVICE_SIZE                18

/* Configuration Descriptor */
struct usb_config_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t wTotalLength;
	uint8_t  bNumInterfaces;
	uint8_t  bConfigurationValue;
	uint8_t  iConfiguration;
	uint8_t  bmAttributes;
	uint8_t  bMaxPower;
} __packed;
#define USB_DT_CONFIG_SIZE                9

/* String Descriptor */
struct usb_string_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t wData[1];
} __packed;

/* Interface Descriptor */
struct usb_interface_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bInterfaceNumber;
	uint8_t  bAlternateSetting;
	uint8_t  bNumEndpoints;
	uint8_t  bInterfaceClass;
	uint8_t  bInterfaceSubClass;
	uint8_t  bInterfaceProtocol;
	uint8_t  iInterface;
} __packed;
#define USB_DT_INTERFACE_SIZE           9

/* Endpoint Descriptor */
struct usb_endpoint_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint8_t  bEndpointAddress;
	uint8_t  bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t  bInterval;
} __packed;
#define USB_DT_ENDPOINT_SIZE            7

/* USB Class codes */
#define USB_CLASS_PER_INTERFACE           0x00
#define USB_CLASS_AUDIO                   0x01
#define USB_CLASS_COMM                    0x02
#define USB_CLASS_HID                     0x03
#define USB_CLASS_PHYSICAL                0x05
#define USB_CLASS_STILL_IMAGE             0x06
#define USB_CLASS_PRINTER                 0x07
#define USB_CLASS_MASS_STORAGE            0x08
#define USB_CLASS_HUB                     0x09
#define USB_CLASS_CDC_DATA                0x0a
#define USB_CLASS_CSCID                   0x0b
#define USB_CLASS_CONTENT_SEC             0x0d
#define USB_CLASS_VIDEO                   0x0e
#define USB_CLASS_WIRELESS_CONTROLLER     0xe0
#define USB_CLASS_MISC                    0xef
#define USB_CLASS_APP_SPEC                0xfe
#define USB_CLASS_VENDOR_SPEC             0xff

/* USB Types */
#define USB_TYPE_CLASS                    0x20

/* USB Vendor ID assigned to Google Inc. */
#define USB_VID_GOOGLE 0x18d1

/* Helpers for descriptors */

#define WIDESTR(quote) WIDESTR2(quote)
#define WIDESTR2(quote) L##quote

#define USB_STRING_DESC(str) \
	(const void *)&(const struct { \
		uint8_t _len; \
		uint8_t _type; \
		wchar_t _data[sizeof(str)]; \
	}) { \
		sizeof(WIDESTR(str)) + 2 - 2, \
		USB_DT_STRING, \
		WIDESTR(str) \
	}

#define USB_CONF_DESC(name) CONCAT2(usb_desc_,name) \
	__attribute__((section(".rodata.usb_desc_" STRINGIFY(name))))
#define USB_IFACE_DESC(num) USB_CONF_DESC(CONCAT3(iface,num,_0iface))
#define USB_EP_DESC(i,num) USB_CONF_DESC(CONCAT4(iface,i,_1ep,num))
#define USB_CUSTOM_DESC(i,name) USB_CONF_DESC(CONCAT4(iface,i,_2,name))

/* String descriptors are defined in the board code */
extern const void * const usb_strings[];
extern const uint8_t usb_string_desc[];

/* USB HID definitions */

#define USB_HID_SUBCLASS_BOOT     1
#define USB_HID_PROTOCOL_KEYBOARD 1
#define USB_HID_PROTOCOL_MOUSE    2

/* USB HID Class requests */
#define USB_HID_REQ_GET_REPORT    0x01
#define USB_HID_REQ_GET_IDLE      0x02
#define USB_HID_REQ_GET_PROTOCOL  0x03
#define USB_HID_REQ_SET_REPORT    0x09
#define USB_HID_REQ_SET_IDLE      0x0A
#define USB_HID_REQ_SET_PROTOCOL  0x0B

/* USB HID class descriptor types */
#define USB_HID_DT_HID            (USB_TYPE_CLASS | 0x01)
#define USB_HID_DT_REPORT         (USB_TYPE_CLASS | 0x02)
#define USB_HID_DT_PHYSICAL       (USB_TYPE_CLASS | 0x03)

struct usb_hid_class_descriptor {
	uint8_t  bDescriptorType;
	uint16_t wDescriptorLength;
} __packed;

struct usb_hid_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdHID;
	uint8_t  bCountryCode;
	uint8_t  bNumDescriptors;
	struct usb_hid_class_descriptor desc[1];
} __packed;

/* class implementation interfaces */
void set_keyboard_report(uint64_t rpt);

#endif /* USB_H */
