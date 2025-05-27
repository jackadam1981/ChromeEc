/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __FT9865_BIO_ALG_H__
#define __FT9865_BIO_ALG_H__

#include <fingerprint/fingerprint_alg.h>
#include <stdint.h>

#define FF_ALGO_SIZE           		64 * 1024
#define FINGER_TEMPLATE_SIZE   		CONFIG_FP_ALGORITHM_TEMPLATE_SIZE //32 * 1024

#define SINGLE_FINGER_ENROLL_NUM    8
#define MAX_TEMPLATE_ID             50

#define FT_TPL_HEAD_SIZE			2048
#define FT_TPL_SUBTPL_SIZE			2048


/* Focal LIBFP algorithm private data. */
struct ft_libfp_data {
	uint16_t cols;
	uint16_t rows;
	uint32_t max_enroll_samples;
	uint8_t remain; //the left enroll count
	uint32_t tpl_finger_size;		// tpl_finger_size = tpl_head_size + tpl_subtemplate_size * n
	uint32_t tpl_head_size;			
	uint32_t tpl_subtemplate_size;	
};

#endif
