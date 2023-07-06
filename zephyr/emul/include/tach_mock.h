/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_INCLUDE_TACH_MOCK_H
#define __EMUL_INCLUDE_TACH_MOCK_H

#include <zephyr/device.h>

/*
 * Set the mocked tachometer RPM to read
 *
 * @param dev		pointer to hte pwm device
 * @param rpm		RPM value the mock should report
 */
void tach_mock_set_rpm(const struct device *dev, int32_t rpm);

#endif /*__EMUL_INCLUDE_TACH_MOCK_H */
