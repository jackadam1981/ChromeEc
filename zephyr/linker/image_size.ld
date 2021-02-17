/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef CONFIG_CROS_EC_RO
__image_size = CONFIG_CROS_EC_RO_SIZE + SIZEOF(datas);
#else
__image_size = CONFIG_CROS_EC_RW_SIZE + SIZEOF(datas);
#endif
