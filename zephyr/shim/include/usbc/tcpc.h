/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_TCPC_H
#define __CROS_EC_TCPC_H

/**
 * @brief Get the TCPC device from the TCPC port enumeration
 *
 * @param port The enumeration of TCPC port
 *
 * @return NULL if failed, otherwise a pointer to device
 */
const struct device *tcpc_get_device_from_port(const int port);

#endif /* __CROS_EC_TCPC_H */
