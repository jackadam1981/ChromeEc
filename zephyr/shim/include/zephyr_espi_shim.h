/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ZEPHYR_ESPI_SHIM_H
#define __CROS_EC_ZEPHYR_ESPI_SHIM_H

#define EC_SC_SCI_EVT 5
#define EC_SC_SMI_EVT 6

/**
 * zephyr_shim_setup_espi() - initialize eSPI device
 *
 * Return: 0 upon success, or <0 upon failure.
 */
int zephyr_shim_setup_espi(void);

#endif /* __CROS_EC_ZEPHYR_ESPI_SHIM_H */
