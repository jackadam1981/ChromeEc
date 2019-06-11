/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor crypto operations */

#ifndef __CROS_EC_FPSENSOR_CRYPTO_H
#define __CROS_EC_FPSENSOR_CRYPTO_H

/**
 * Generate random finger id.
 *
 * @param new_finger_id buffer to hold output finger id, must have size of at
 * least FP_FINGER_ID_BYTES.
 * @return EC_RES_SUCCESS on success and EC_RES_TIMEOUT if timeout.
 */
int fp_rand_finger_id(uint8_t *new_finger_id);

/**
 * Derive hardware encryption key from rollback secret and |salt|.
 *
 * @param outkey the pointer to buffer holding the output key.
 * @param salt the salt to use in HKDF.
 * @return EC_RES_SUCCESS on success and EC_RES_ERROR otherwise.
 */
int derive_encryption_key(uint8_t *out_key, const uint8_t *salt);

/**
 * Derive new positive match secret for an enrolled finger.
 *
 * @param output buffer to store positive match secret.
 * @return EC_RES_SUCCESS on success and EC_RES_ERROR otherwise.
 */
int derive_new_pos_match_secret(uint8_t *output);

/**
 * Derive positive match secret from |input_pos_match_salt| and
 * |input_finger_id|.
 *
 * @param output buffer to store positive match secret.
 * @param input_pos_match_salt the salt for deriving secret.
 * @param input_finger_id the finger id tied to the secret.
 * @return EC_RES_SUCCESS on success and EC_RES_ERROR otherwise.
 */
int derive_pos_match_secret(uint8_t *output, uint8_t *input_pos_match_salt,
			    uint8_t *input_finger_id);

/**
 * Encrypt |plaintext| using AES-GCM128.
 *
 * @param key the key to use in AES.
 * @param key_size the size of |key| in bytes.
 * @param plaintext the plain text to encrypt.
 * @param ciphertext buffer to hold encryption result.
 * @param text_size size of both |plaintext| and output ciphertext in bytes.
 * @param nonce the nonce value to use in GCM128.
 * @param nonce_size the size of |nonce| in bytes.
 * @param tag the tag to hold the authenticator after encryption.
 * @param tag_size the size of |tag|.
 * @return EC_RES_SUCCESS on success and EC_RES_ERROR otherwise.
 */
int aes_gcm_encrypt(const uint8_t *key, int key_size,
		    const uint8_t *plaintext,
		    uint8_t *ciphertext, int text_size,
		    const uint8_t *nonce, int nonce_size,
		    uint8_t *tag, int tag_size);

/**
 * Decrypt |plaintext| using AES-GCM128.
 *
 * @param key the key to use in AES.
 * @param key_size the size of |key| in bytes.
 * @param ciphertext the cipher text to decrypt.
 * @param plaintext buffer to hold decryption result.
 * @param text_size size of both |ciphertext| and output plaintext in bytes.
 * @param nonce the nonce value to use in GCM128.
 * @param nonce_size the size of |nonce| in bytes.
 * @param tag the tag to compare against when decryption finishes.
 * @param tag_size the length of tag to compare against.
 * @return EC_RES_SUCCESS on success and EC_RES_ERROR otherwise.
 */
int aes_gcm_decrypt(const uint8_t *key, int key_size, uint8_t *plaintext,
		    const uint8_t *ciphertext, int text_size,
		    const uint8_t *nonce, int nonce_size,
		    const uint8_t *tag, int tag_size);

#endif /* __CROS_EC_FPSENSOR_CRYPTO_H */
