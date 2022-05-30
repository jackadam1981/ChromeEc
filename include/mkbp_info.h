/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MKBP info host command for Chrome EC */

#ifndef __CROS_EC_MKBP_INFO_H
#define __CROS_EC_MKBP_INFO_H

#ifdef CONFIG_VOLUME_BUTTONS
/**
 * Board specific function to disable setup for volume buttons
 *
 * Although we're able to define CONFIG_VOLUME_BUTTONS for ec volume buttons,
 * some board might need to configure this settings at run time by several
 * cases such as sharing same firmware with different designed.
 *
 * @return true if want to enable volume buttons setup else false
 */
__override_proto bool mkbp_enable_vol_button_via_custom(void);
#endif

#endif /* __CROS_EC_MKBP_INFO_H */
