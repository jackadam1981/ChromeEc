/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_BOARD_CONFIG_H
#define __CROS_EC_BOARD_CONFIG_H

#include "common.h"

#ifdef CONFIG_BOARD_PRE_INIT
/**
 * Configure board before any inits are called.
 *
 * Note that this is in general a hacky place to do configuration.  Most config
 * is actually chip-specific or module-specific and not board-specific, so
 * putting it here hides dependencies between module inits and board init.
 * Think very hard before putting code here.
 */
void board_config_pre_init(void);
#endif

#ifdef CONFIG_BOARD_POST_GPIO_INIT
/**
 * Configure board after GPIOs are initialized.
 *
 * Note that this is in general a hacky place to do configuration.  Most config
 * is actually chip-specific or module-specific and not board-specific, so
 * putting it here hides dependencies between module inits and board init.
 * Think very hard before putting code here.
 */
void board_config_post_gpio_init(void);
#endif

#ifdef CONFIG_EC_FEATURE_BOARD_OVERRIDE
/* function for board specific overrides to default feature flags */
uint32_t board_override_feature_flags0(uint32_t flags0);
uint32_t board_override_feature_flags1(uint32_t flags1);
#endif

#endif /* __CROS_EC_BOARD_CONFIG_H */
