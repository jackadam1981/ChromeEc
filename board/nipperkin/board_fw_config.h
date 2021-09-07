/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _GUYBRUSH_BOARD_FW_CONFIG__H_
#define _GUYBRUSH_BOARD_FW_CONFIG__H_

/****************************************************************************
 * Guybrush CBI FW Configuration
 */

/*
 * Keyboard Backlight (1 bit)
 */
#define FW_CONFIG_KBLIGHT_OFFSET		0
#define FW_CONFIG_KBLIGHT_WIDTH			1
#define FW_CONFIG_KBLIGHT_NO			0
#define FW_CONFIG_KBLIGHT_YES			1

/*
 * Finger Printer (1 bits)
 */
#define FW_CONFIG_FINGER_PRINTER_OFFSET		1
#define FW_CONFIG_FINGER_PRINTER_WIDTH		1

/*
 * Wireless Lan (2 bits)
 */
#define FW_CONFIG_WLAN_OFFSET		2
#define FW_CONFIG_WLAN_WIDTH		2

/*
 * WWAN (2 bits)
 */
#define FW_CONFIG_WWAN_OFFSET		4
#define FW_CONFIG_WWAN_WIDTH		2

/*
 * Storage Type (1 bits)
 */
#define FW_CONFIG_STORAGE_OFFSET	6
#define FW_CONFIG_STORAGE_WIDTH		1

/*
 * Keyboard (1 bits)
 */
#define FW_CONFIG_KEYBOARD_OFFSET		7
#define FW_CONFIG_KEYBOARD_WIDTH		1
#define FW_CONFIG_KEYBOARD_PRIVACY_YES		0
#define FW_CONFIG_KEYBOARD_PRIVACY_NO		1


#endif /* _GUYBRUSH_CBI_FW_CONFIG__H_ */
