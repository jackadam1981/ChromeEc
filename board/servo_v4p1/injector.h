/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_INJECTOR_H
#define __CROS_EC_INJECTOR_H

enum inj_res {
	INJ_RES_NONE = 0,
	INJ_RES_RA = 1,
	INJ_RES_RD = 2,
	INJ_RES_RPUSB = 3,
	INJ_RES_RP1A5 = 4,
	INJ_RES_RP3A0 = 5,
};

#endif /* __CROS_EC_INJECTOR_H */
