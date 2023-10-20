/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SHIM_INCLUDE_ERRNO_MAP_H_
#define ZEPHYR_SHIM_INCLUDE_ERRNO_MAP_H_

#ifdef __cplusplus
extern "C" {
#endif

int errno_to_ec(int ret);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SHIM_INCLUDE_ERRNO_MAP_H_ */
