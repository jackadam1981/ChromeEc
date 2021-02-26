/* -*- mode:c -*-
 *
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare FW_CONFIG fields used by board */

/*
 * USB Daughter Board Mux and Retimer (2 bits)
 */
FW_CONFIG_FIELD(USB_DB, 0, 2, A1_PS8811_C1_PS8818, A1_ANX7491_C1_ANX7451)

/*
 * Form Factor (1 bit)
 */
FW_CONFIG_FIELD(FORM_FACTOR, 2, 1, CLAMSHELL, CONVERTIBLE)

/*
 * Keyboard Back Light (1 bit)
 */
FW_CONFIG_FIELD(KB_BL, 3, 1, ENABLED, DISABLED)
