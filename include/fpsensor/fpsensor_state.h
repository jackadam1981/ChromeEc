/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_FPSENSOR_STATE_H
#define __CROS_EC_FPSENSOR_FPSENSOR_STATE_H

#include "atomic.h"
#include "common.h"
#include "ec_commands.h"
#include "fpsensor_driver.h"
#include "fpsensor_matcher.h"
#include "fpsensor_state_driver.h"
#include "fpsensor_state_without_driver_info.h"
#include "link_defs.h"
#include "timer.h"

#include <stdbool.h>
#include <stdint.h>

#include <span>

/* if no special memory regions are defined, fallback on regular SRAM */
#ifndef FP_FRAME_SECTION
#define FP_FRAME_SECTION
#endif
#ifndef FP_TEMPLATE_SECTION
#define FP_TEMPLATE_SECTION
#endif

#define FP_ALGORITHM_ENCRYPTED_TEMPLATE_SIZE                         \
	(FP_ALGORITHM_TEMPLATE_SIZE + FP_POSITIVE_MATCH_SALT_BYTES + \
	 sizeof(struct ec_fp_template_encryption_metadata))

/* --- Global variables. --- */

/* Last acquired frame (aligned as it is used by arbitrary binary libraries) */
extern "C" {
inline uint8_t fp_buffer[FP_SENSOR_IMAGE_SIZE] FP_FRAME_SECTION __aligned(4);
}

/* Fingers templates for the current user */
test_mockable inline uint8_t
	fp_template[FP_MAX_FINGER_COUNT]
		   [FP_ALGORITHM_TEMPLATE_SIZE] FP_TEMPLATE_SECTION
			   __aligned(4);
static_assert(
	sizeof(fp_template[0]) % 4 == 0,
	"The size of each template must be a multiple of 4 to ensure that the next "
	"template will still be aligned by 4.");

/* Encryption/decryption buffer */
/* TODO: On-the-fly encryption/decryption without a dedicated buffer */
/*
 * Store the encryption metadata at the beginning of the buffer containing the
 * ciphered data.
 */
inline std::array<uint8_t, FP_ALGORITHM_ENCRYPTED_TEMPLATE_SIZE> fp_enc_buffer
	FP_TEMPLATE_SECTION;

/* Salt used in derivation of positive match secret. */
inline uint8_t fp_positive_match_salt[FP_MAX_FINGER_COUNT]
				     [FP_POSITIVE_MATCH_SALT_BYTES];

/* Simulation for unit tests. */
#ifdef __cplusplus
extern "C" {
#endif
__test_only void fp_task_simulate(void);
#ifdef __cplusplus
}
#endif

/*
 * Clear one fingerprint template.
 *
 * @param idx the index of the template to clear.
 */
void fp_clear_finger_context(uint16_t idx);

/**
 * Reset the current user id associated.
 */
void fp_reset_context(void);

/**
 * Init the decrypted template state with the current user_id.
 */
void fp_init_decrypted_template_state_with_user_id(uint16_t idx);

/**
 * Clear all fingerprint templates associated with the current user id and
 * reset the sensor.
 */
void fp_reset_and_clear_context(void);

/*
 * Get the next FP event.
 *
 * @param out the pointer to the output event.
 */
#ifdef __cplusplus
extern "C" {
#endif
int fp_get_next_event(uint8_t *out);
#ifdef __cplusplus
}
#endif

/**
 * Change the sensor mode.
 *
 * @param mode          new mode to change to
 * @param mode_output   resulting mode
 * @return EC_RES_SUCCESS on success. Error code on failure.
 */
enum ec_status fp_set_sensor_mode(uint32_t mode, uint32_t *mode_output);

/**
 * Allow reading positive match secret for |fgr| in the next 5 seconds.
 *
 * @param fgr the index of template to enable positive match secret.
 * @param state the state of positive match secret, e.g. readable or not.
 * @return EC_SUCCESS if the request is valid, error code otherwise.
 */
int fp_enable_positive_match_secret(uint16_t fgr,
				    struct positive_match_secret_state *state);

/**
 * Disallow positive match secret for any finger to be read.
 *
 * @param state the state of positive match secret, e.g. readable or not.
 */
void fp_disable_positive_match_secret(struct positive_match_secret_state *state);

/**
 * Read the match secret from the positive match salt.
 *
 * @param fgr the index of positive match salt.
 * @param positive_match_secret the match secret that derived from the salt.
 */
enum ec_status
fp_read_match_secret(int8_t fgr,
		     std::span<uint8_t, FP_POSITIVE_MATCH_SECRET_BYTES>
			     positive_match_secret);


#endif /* __CROS_EC_FPSENSOR_FPSENSOR_STATE_H */
