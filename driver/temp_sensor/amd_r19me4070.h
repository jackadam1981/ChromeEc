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
#define R19ME4070_LOCAL                 0
#define GPU_PORT                        4
/*
 *default is PORT4, If you want to use I2C_PORT_GPU,
 *must assign I2C channel in board.h
 */
#ifndef I2C_PORT_GPU
#define I2C_PORT_GPU                    GPU_PORT
#endif

/*
 * get GPU temperature value and move to *tem_ptr
 * One second trigger ,Use I2C read GPU's Die temperature.
 */
int get_temp_R19ME4070(int idx, int *temp_ptr);

#endif /* __CROS_EC_AMD_R19ME4070_H */
