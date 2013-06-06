/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB definitions.
 */

#ifndef USB_H
#define USB_H

/* USB 2.0 chapter 9 definitions */
#define USB_DEVICE_DESCRIPTOR_TYPE 0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE 0x02
#define USB_STRING_DESCRIPTOR_TYPE 0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE 0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE 0x05

#define WIDESTR(quote) WIDESTR2(quote)
#define WIDESTR2(quote) L##quote

void set_keyboard_report(uint64_t rpt);

#define USB_STRING_DESC(varname, str) \
	struct { \
		uint8_t _len; \
		uint8_t _type; \
		wchar_t _data[sizeof(str)]; \
	} varname = { \
		sizeof(WIDESTR(str)) + 2 - 2, \
		USB_STRING_DESCRIPTOR_TYPE, \
		WIDESTR(str) \
	}


#endif /* USB_H */
