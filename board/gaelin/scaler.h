/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SCALER_H
#define __CROS_EC_SCALER_H

//extern int8_t scaler_en;  //for scaler test //raymondchung: ???
extern int scaler_[3];  //for scaler test //raymondchung: ???

void scaler_send_scan_code(uint8_t action_keys);

void scaler_test(void);  //for scaler test //raymondchung: ???

#endif /* __CROS_EC_SCALER_H */
