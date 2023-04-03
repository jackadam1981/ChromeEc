/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_GPIO_H__
#define __CROS_EC_STARFISH_GPIO_H__

#include <zephyr/drivers/gpio.h>

enum GPIO_LABEL {
	GPIO_LABEL_BUTTON_MODE,
	GPIO_LABEL_BUTTON_NEXT,
	GPIO_LABEL_BUTTON_PREV,
	GPIO_LABEL_HOST_DET_CTRL,
	GPIO_LABEL_LED_MODE,
	GPIO_LABEL_LED_SLOT,
	GPIO_LABEL_SIM_HOST_EN,
	GPIO_LABEL_SIM_MUX_ADDR,
	GPIO_LABEL_SIM_MUX_EN,
	GPIO_LABEL_SIM_CD,
	GPIO_LABEL_SIM_VCC_SEL,
};

/*
 * Defines a GPIO context state which
 */
struct gpio_ctx {
	/* GPIO pin spec */
	struct gpio_dt_spec spec;
	/* GPIO group label */
	enum GPIO_LABEL label;
	/* GPIO group index */
	int idx;
};

#define _GPIO_CTX(node, y, idx_)                                              \
	{                                                                     \
		.gpio = {                                                     \
			.spec = GPIO_DT_SPEC_GET_BY_IDX(node, gpios, idx_),   \
			.label = _CONCAT(GPIO_LABEL_,                         \
					 DT_STRING_UPPER_TOKEN(node, label)), \
			.idx = idx_,                                          \
		}                                                             \
	}

#define COMMA (, )
#define _GPIO_CB(node) DT_FOREACH_PROP_ELEM_SEP(node, gpios, _GPIO_CTX, COMMA)

#define GPIO_LIST_CTX(node) \
	DT_FOREACH_CHILD_SEP(DT_PATH(gpio, node), _GPIO_CB, COMMA)

#endif /* __CROS_EC_STARFISH_GPIO_H__ */
