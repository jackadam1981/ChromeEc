/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_GPIO_MAP_H
#define __ZEPHYR_GPIO_MAP_H

#define GPIO_ENTERING_RW		GPIO_UNIMPLEMENTED
#define GPIO_WP_L			GPIO_UNIMPLEMENTED

#ifdef CONFIG_PLATFORM_EC_GMR_TABLET_MODE
#define GMR_TABLET_MODE_GPIO_L GPIO_TABLET_MODE_L
#endif

#define GPIO_EN_PP5000 GPIO_EN_PP5000_A

#endif /* __ZEPHYR_GPIO_MAP_H */
