/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_IOEXPANDER_PS8815_H_
#define __CROS_EC_DRIVER_IOEXPANDER_PS8815_H_

#define PS8815_IO_FLAG_VER_MASK GENMASK(2, 1)
#define PS8815_IO_FLAG_VER_OFFSET 0

#define PS8815_IO_REG_OUT_SEL (0xf0)
#define PS8815_IO_REG_OEB_SEL (0xf2)
#define PS8815_IO_REG_LEVEL (0xf4)
/* Input - 1, output - 0 */
#define PS8815_IO_REG_DIRECTION (0xf6)

#define PS8815_IOEX_PORT0 (0)
#define PS8815_IOEX_PORT1 (1)
#define PS8815_IOEX_PORT0_MASK (0xff)
#define PS8815_IOEX_PORT1_MASK (0x3)


extern const struct ioexpander_drv ps8815_ioexpander_drv;

#endif /* __CROS_EC_DRIVER_IOEXPANDER_PS8815_H_ */
