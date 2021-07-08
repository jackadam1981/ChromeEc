/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SUBSYS_CHRE_INCLUDE_CHRE_TARGET_PLATFORM_LOG_H_
#define ZEPHYR_SUBSYS_CHRE_INCLUDE_CHRE_TARGET_PLATFORM_LOG_H_

#include <logging/log.h>

#if CHRE_MINIMUM_LOG_LEVEL <= CHRE_LOG_LEVEL_ERROR
#define LOGE(format, ...) LOG_ERR(format, __VA_ARGS__)
#else
#define LOGE(...)
#endif

#if CHRE_MINIMUM_LOG_LEVEL <= CHRE_LOG_LEVEL_WARN
#define LOGW(format, ...) LOG_WRN(format, __VA_ARGS__)
#else
#define LOGW(...)
#endif

#if CHRE_MINIMUM_LOG_LEVEL <= CHRE_LOG_LEVEL_INFO
#define LOGI(format, ...) LOG_INF(format, __VA_ARGS__)
#else
#define LOGI(...)
#endif

#if CHRE_MINIMUM_LOG_LEVEL <= CHRE_LOG_LEVEL_DEBUG
#define LOGD(format, ...) LOG_DBG(format, __VA_ARGS__)
#else
#define LOGD(...)
#endif

#endif /* ZEPHYR_SUBSYS_CHRE_INCLUDE_CHRE_TARGET_PLATFORM_LOG_H_ */
