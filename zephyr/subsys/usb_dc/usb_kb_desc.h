/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Keyboard protocol interface
 */

#ifndef __USB_KB_DESC_H
#define __USB_KB_DESC_H

#define HID_KEYBOARD_MODIFIER_LOW 0xE0
#define HID_KEYBOARD_MODIFIER_HIGH 0xE7
#define HID_KEYBOARD_ASSISTANT_KEY 0xF0

#define CONFIG_USB_HID_KB_NUM_TOP_ROW_KEYS 10

#define KEYBOARD_BASE_DESC                                                 \
	0x05, 0x01, /* Usage Page (Generic Desktop) */                     \
		0x09, 0x06, /* Usage (Keyboard) */                         \
		0xA1, 0x01, /* Collection (Application) */                 \
                                                                           \
		/* Modifiers */                                            \
		0x05, 0x07, /* Usage Page (Key Codes) */                   \
		0x19, HID_KEYBOARD_MODIFIER_LOW, /* Usage Minimum */       \
		0x29, HID_KEYBOARD_MODIFIER_HIGH, /* Usage Maximum */      \
		0x15, 0x00, /* Logical Minimum (0) */                      \
		0x25, 0x01, /* Logical Maximum (1) */                      \
		0x75, 0x01, /* Report Size (1) */                          \
		0x95, 0x08, /* Report Count (8) */                         \
		0x81, 0x02, /* Input (Data, Variable, Absolute), ;Modifier \
			       byte */                                     \
                                                                           \
		0x95, 0x01, /* Report Count (1) */                         \
		0x75, 0x08, /* Report Size (8) */                          \
		0x81, 0x01, /* Input (Constant), ;Reserved byte */         \
                                                                           \
		/* Normal keys */                                          \
		0x95, 0x06, /* Report Count (6) */                         \
		0x75, 0x08, /* Report Size (8) */                          \
		0x15, 0x00, /* Logical Minimum (0) */                      \
		0x25, 0xa4, /* Logical Maximum (164) */                    \
		0x05, 0x07, /* Usage Page (Key Codes) */                   \
		0x19, 0x00, /* Usage Minimum (0) */                        \
		0x29, 0xa4, /* Usage Maximum (164) */                      \
		0x81, 0x00, /* Input (Data, Array), ;Key arrays (6 bytes) */

#define KEYBOARD_TOP_ROW_DESC                                                 \
	/* Modifiers */                                                       \
	0x05, 0x0C, /* Consumer Page */                                       \
		0x0A, 0x24, 0x02, /* AC Back (0x224) */                       \
		0x0A, 0x25, 0x02, /* AC Forward (0x225) */                    \
		0x0A, 0x27, 0x02, /* AC Refresh (0x227) */                    \
		0x0A, 0x32, 0x02, /* AC View Toggle (0x232) */                \
		0x0A, 0x9F, 0x02, /* AC Desktop Show All windows (0x29F) */   \
		0x09, 0x70, /* Display Brightness Decrement (0x70) */         \
		0x09, 0x6F, /* Display Brightness Increment (0x6F) */         \
		0x09, 0xE2, /* Mute (0xE2) */                                 \
		0x09, 0xEA, /* Volume Decrement (0xEA) */                     \
		0x09, 0xE9, /* Volume Increment (0xE9) */                     \
		0x0B, 0x46, 0x00, 0x07, 0x00, /* PrintScreen (Page 0x7, Usage \
						 0x46) */                     \
		0x0A, 0xD0, 0x02, /* Privacy Screen Toggle (0x2D0) */         \
		0x09, 0x7A, /* Keyboard Brightness Decrement (0x7A) */        \
		0x09, 0x79, /* Keyboard Brightness Increment (0x79)*/         \
		0x09, 0xCD, /* Play / Pause (0xCD) */                         \
		0x09, 0xB5, /* Scan Next Track (0xB5) */                      \
		0x09, 0xB6, /* Scan Previous Track (0xB6) */                  \
		0x09, 0x7C, /* Keyboard Backlight OOC (0x7C) */               \
		0x0B, 0x2F, 0x00, 0x0B, 0x00, /* Phone Mute (Page 0xB, Usage  \
						 0x2F) */                     \
		0x09, 0x32, /* Sleep (0x32) */                                \
		0x15, 0x00, /* Logical Minimum (0) */                         \
		0x25, 0x01, /* Logical Maximum (1) */                         \
		0x75, 0x01, /* Report Size (1) */                             \
		0x95, 0x14, /* Report Count (20) */                           \
		0x81, 0x02, /* Input (Data, Variable, Absolute), ;Modifier    \
			       byte */                                        \
                                                                              \
		/* 12-bit padding */                                          \
		0x95, 0x0C, /* Report Count (12) */                           \
		0x75, 0x01, /* Report Size (1) */                             \
		0x81, 0x01, /* Input (Constant), ;1-bit padding */

#define KEYBOARD_TOP_ROW_FEATURE_DESC                                         \
	0x06, 0xd1, 0xff, /* Usage Page (Google) */                           \
		0x09, 0x01, /* Usage (Top Row List) */                        \
		0xa1, 0x02, /* Collection (Logical) */                        \
		0x05, 0x0a, /*   Usage Page (Ordinal) */                      \
		0x19, 0x01, /*   Usage Minimum (1) */                         \
		0x29, CONFIG_USB_HID_KB_NUM_TOP_ROW_KEYS, /* Usage Maximum */ \
		0x95, CONFIG_USB_HID_KB_NUM_TOP_ROW_KEYS, /* Report Count */  \
		0x75, 0x20, /*   Report Size (32) */                          \
		0xb1, 0x03, /*   Feature (Cnst,Var,Abs) */                    \
		0xc0, /* End Collection */

/*
 * Vendor-defined Usage Page 0xffd1:
 *  - 0x18: Assistant key
 *  - 0x19: Tablet mode switch
 */
#ifdef HID_KEYBOARD_EXTRA_FIELD
#ifdef CONFIG_KEYBOARD_ASSISTANT_KEY
#define KEYBOARD_ASSISTANT_KEY_DESC                                        \
	0x19, 0x18, /* Usage Minimum */                                    \
		0x29, 0x18, /* Usage Maximum */                            \
		0x15, 0x00, /* Logical Minimum (0) */                      \
		0x25, 0x01, /* Logical Maximum (1) */                      \
		0x75, 0x01, /* Report Size (1) */                          \
		0x95, 0x01, /* Report Count (1) */                         \
		0x81, 0x02, /* Input (Data, Variable, Absolute), ;Modifier \
			       byte */
#else
/* No assistant key: just pad 1 bit. */
#define KEYBOARD_ASSISTANT_KEY_DESC               \
	0x95, 0x01, /* Report Count (1) */        \
		0x75, 0x01, /* Report Size (1) */ \
		0x81, 0x01, /* Input (Constant), ;1-bit padding */
#endif /* !CONFIG_KEYBOARD_ASSISTANT_KEY */

#ifdef CONFIG_KEYBOARD_TABLET_MODE_SWITCH
#define KEYBOARD_TABLET_MODE_SWITCH_DESC                                   \
	0x19, 0x19, /* Usage Minimum */                                    \
		0x29, 0x19, /* Usage Maximum */                            \
		0x15, 0x00, /* Logical Minimum (0) */                      \
		0x25, 0x01, /* Logical Maximum (1) */                      \
		0x75, 0x01, /* Report Size (1) */                          \
		0x95, 0x01, /* Report Count (1) */                         \
		0x81, 0x02, /* Input (Data, Variable, Absolute), ;Modifier \
			       byte */
#else
/* No tablet mode swtch: just pad 1 bit. */
#define KEYBOARD_TABLET_MODE_SWITCH_DESC          \
	0x95, 0x01, /* Report Count (1) */        \
		0x75, 0x01, /* Report Size (1) */ \
		0x81, 0x01, /* Input (Constant), ;1-bit padding */
#endif /* CONFIG_KEYBOARD_TABLET_MODE_SWITCH */

#define KEYBOARD_VENDOR_DESC                                                 \
	0x06, 0xd1, 0xff, /* Usage Page (Vendor-defined 0xffd1) */           \
                                                                             \
		KEYBOARD_ASSISTANT_KEY_DESC KEYBOARD_TABLET_MODE_SWITCH_DESC \
                                                                             \
		0x95,                                                        \
		0x01, /* Report Count (1) */                                 \
		0x75, 0x06, /* Report Size (6) */                            \
		0x81, 0x01, /* Input (Constant), ;6-bit padding */
#endif /* HID_KEYBOARD_EXTRA_FIELD */

#define KEYBOARD_BACKLIGHT_DESC                                       \
	0xA1, 0x02, /* Collection (Logical) */                        \
		0x05, 0x14, /*   Usage Page (Alphanumeric Display) */ \
		0x09, 0x46, /*   Usage (Display Brightness) */        \
		0x95, 0x01, /*   Report Count (1) */                  \
		0x75, 0x08, /*   Report Size (8) */                   \
		0x15, 0x00, /*   Logical Minimum (0) */               \
		0x25, 0x64, /*   Logical Maximum (100) */             \
		0x91, 0x02, /*   Output (Data, Variable, Absolute) */ \
		0xC0, /* End Collection */

#endif /* __USB_KB_DESC_H */
