/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef UM_PPM_PPM_UTILS_H_
#define UM_PPM_PPM_UTILS_H_

#include <stddef.h>

#include <drivers/ucsi_v3.h>

#define PPM_MAXIMUM_ALT_MODES 16

/**
 * @brief Issue GET_ALTERNATE_MODES to the LPM.
 *
 * @param ppm_dev: PPM driver instance
 * @param connector_num: LPM connector number to issue the GET_ALTERNATES_MODES
 * command to. 1 based indexing.
 * @param recipient: Recipient for the altnerate modes report.
 * @param max_modes: Maximum number of modes to read
 * @param ucsi_alt_modes: Output parameter, array of alternate modes read
 * @param num_alt_modes: Output parameter, number of alternate modes written to
 * \p ucsi_alt_modes.
 * @returns 0 on success, <0 on error.
 */
int ppm_get_alternate_modes(const struct device *ppm_dev, uint8_t connector_num,
			    enum alt_modes_recipient recipient,
			    size_t max_modes,
			    struct ucsi_altmode_field *ucsi_alt_modes,
			    size_t *num_alt_modes);

/**
 * @brief Issue GET_ALTERNATE_MODES to the LPM.
 *
 * @param ppm_dev: PPM driver instance
 * @param connector_num: LPM connector number to issue the GET_ALTERNATES_MODES
 * @param mask_size: Size of the /p altmode_supported_mask array in bytes
 * @param altmode_supported_mask: Output parameter, array of alternate modes
 * supported, one bit per alternate mode.
 * @param resp_size: Output parameter, number of bytes written to /p
 * altmode_supported_mask
 * @returns 0 on success, <0 on error.
 */
int ppm_get_cam_supported(const struct device *ppm_dev, uint8_t connector_num,
			  size_t mask_size, uint8_t *altmode_supported_mask,
			  size_t *resp_size);

/**
 * @brief Issue GET_CURRENT_CAM to the LPM.
 *
 * @param ppm_dev: PPM driver instance
 * @param connector_num: LPM connector number to issue the GET_ALTERNATES_MODES
 * @param altmodes_size: Size of the /p altmodes array in bytes
 * @param altmodes: Output parameter, array of alternate modes, one byte per
 * alternate mode.
 * @param num_altmodes: Output parameter, number of bytes written to the /p
 * altmodes array.
 * @returns 0 on success, <0 on error.
 */
int ppm_get_current_cam(const struct device *ppm_dev, uint8_t connector_num,
			size_t altmodes_size, uint8_t *altmodes,
			size_t *num_altmodes);

#endif /* UM_PPM_PPM_UTILS_H */
