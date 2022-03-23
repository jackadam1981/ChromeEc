/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Primus board-specific PS2 configuration */

#define TP_READ_ID		0xE1	/* Sent for device identification */
#define TP_COMMAND		0xE2	/* Commands start with this */

/*
 * Toggling Flag bits
 */
#define TP_TOGGLE		0x47	/* Toggle command */

/*
 * Valid first byte responses to the "Read Secondary ID" (0xE1) command.
 * 0x01 was the original IBM trackpoint, others implement very limited
 * subset of trackpoint features.
 */
#define TP_VARIANT_ELAN			0x03
#define TP_VARIANT_SYNAPTICS		0x06
