/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_GPIO_SIGNAL_H
#define __CROS_EC_GPIO_SIGNAL_H

#include <devicetree.h>

#define GPIO_SIGNAL(id) DT_CAT(GPIO_, id)
#define GPIO_SIGNAL_WITH_COMMA(id) GPIO_SIGNAL(id),
enum gpio_signal {
#if DT_NODE_EXISTS(DT_PATH(named_gpios))
	DT_FOREACH_CHILD(DT_PATH(named_gpios), GPIO_SIGNAL_WITH_COMMA)
#endif
	GPIO_COUNT,
	GPIO_LIMIT = 0x0FFF,
};
#undef GPIO_SIGNAL_WITH_COMMA

enum ioex_signal {
	IOEX_SIGNAL_START = GPIO_LIMIT + 1,
	__IOEX_PLACEHOLDER = GPIO_LIMIT,
	IOEX_SIGNAL_END,
	IOEX_LIMIT = 0x1FFF,
};

/** @brief Converts a node identifier under named gpios to enum
 *
 * Converts the specified node identifier name, which should be nested under
 * the named_gpios node, into the correct enum gpio_signal that can be used
 * with platform/ec gpio API
 */
#define NAMED_GPIO(name) GPIO_SIGNAL(DT_PATH(named_gpios, name))

#endif  /* __CROS_EC_GPIO_SIGNAL_H */
