/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPU R19ME4070 configuration */

#ifndef __CROS_EC_R19ME4070_H
#define __CROS_EC_R19ME4070_H

/* Baseboard features */
#include "baseboard.h"

/* GPU features */
#define GPU_CONFIG
#define R19M14017_LOCAL                 0
/* GPU I2C address */
#define GPU_ADDR_FLAGS                  0x82
/* GPU Temperature functions */

/*
 * Tell SMBus slave which register to read before GPU read
 * temperature, call it "GPU INIT".
 */
#define GPU_INIT_OFFSET                 0x01
#define GPU_TEMPERATURE_OFFSET          0x03
#define GPU_INIT_WRITE_VALUE            0x0F01665A
#define I2C_PORT_GPU                    NPCX_I2C_PORT4_1

/*
 * get GPU temperature value and move to *tem_ptr
 * One second trigger ,Use I2C read GPU's Die temperature.
 */
int get_temp_R19M14017(int idx, int *temp_ptr);

/*
 * Use I2C Write GPU register,do this function before
 * read GPU temperature .
 */
void gpu_init(void);

#endif /* __CROS_EC_AMD_R19ME4070_H */
