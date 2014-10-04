/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB billboard definitions.
 */

#ifndef USB_BB_H
#define USB_BB_H

/* per Billboard Device Class Spec Revision 1.0 */

/* device descriptor fields */
#define USB_BB_BCDUSB_MIN 0x0201 /* v2.01 minimum */
#define USB_BB_SUBCLASS 0x00
#define USB_BB_PROTOCOL 0x00
#define USB_BB_EP0_PACKET_SIZE 8
#define USB_BB_CAP_DESC_TYPE 0x0d
#endif /* USB_BB_H */

