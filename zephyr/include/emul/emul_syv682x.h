/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for SYV682X emulator
 */

#ifndef __EMUL_SYV682X_H
#define __EMUL_SYV682X_H

#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

/**
 * @brief Get pointer to SYV682x emulator using device tree order number.
 *
 * @param ord Device tree order number obtained from DT_DEP_ORD macro
 *
 * @return Pointer to smart battery emulator
 */
struct i2c_emul *syv682x_emul_get_ptr(int ord);

#endif /* __EMUL_SYV682X_H */
