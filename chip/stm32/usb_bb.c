/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"
#include "usb_bb.h"

/*
 * Implements the USB Billboard Class specification using the
 * ...
 */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

/* USB descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_BILLBOARD) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = USB_IFACE_BILLBOARD,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_BILLBOARD,
	.bInterfaceSubClass = USB_BB_SUBCLASS,
	.bInterfaceProtocol = USB_BB_PROTOCOL,
	.iInterface = 0,
};
