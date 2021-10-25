/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_USBC_PPC_H
#define ZEPHYR_CHROME_USBC_PPC_H

#include <device.h>
#include <devicetree.h>

#define PPC_ID(id) id
#define PPC_ID_WITH_COMMA(id) PPC_ID(id),
#define PPC_COMPAT ppc_chip

enum ppc_chips_id {
	DT_FOREACH_STATUS_OKAY(PPC_COMPAT, PPC_ID_WITH_COMMA)
	PPC_CHIP_COUNT
};

#endif /* ZEPHYR_CHROME_USBC_PPC_H */
