/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <dt-bindings/pinctrl/it8xxx2-pinctrl.h>
#include <soc.h>

static int it8xxx2_pinmux_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}
SYS_INIT(it8xxx2_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);

