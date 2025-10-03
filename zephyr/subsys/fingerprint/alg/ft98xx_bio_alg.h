/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __FT98xx_BIO_ALG_H__
#define __FT98xx_BIO_ALG_H__

#include <stdint.h>

#include <fingerprint/fingerprint_alg.h>

#define FINGER_TEMPLATE_SIZE CONFIG_FP_ALGORITHM_TEMPLATE_SIZE

#define SINGLE_FINGER_ENROLL_NUM 10

/*temporary use template size to separate sensors, the next version of algo will
 * no need to separate them*/
#if (FINGER_TEMPLATE_SIZE == 31744) // CONFIG_FINGERPRINT_SENSOR_FT9849
#define MAX_TEMPLATE_ID 50
#define FT_TPL_HEAD_SIZE 1024
#define FT_TPL_SUBTPL_SIZE 2048
#define FT_ALGO_SIZE 94 * 1024
#else
#define MAX_TEMPLATE_ID 3
#define FT_TPL_HEAD_SIZE 2048
#define FT_TPL_SUBTPL_SIZE 2048
#define FT_ALGO_SIZE 68 * 1024
#endif

#define FT_TPL_ENROLL_SIZE \
	(FT_TPL_HEAD_SIZE + FT_TPL_SUBTPL_SIZE * SINGLE_FINGER_ENROLL_NUM)

typedef enum {
	MSG_LVL_ALL = 0,
	MSG_LVL_VBS = 1,
	MSG_LVL_DBG = 2,
	MSG_LVL_INF = 3,
	MSG_LVL_WRN = 4,
	MSG_LVL_ERR = 5,
	MSG_LVL_DIS = 6,
} log_level_t;

/* Focal LIBFP algorithm private data. */
struct ft_libfp_data {
	uint16_t cols;
	uint16_t rows;
	uint32_t max_enroll_samples;
	uint8_t remain; // the left enroll count
	uint32_t tpl_finger_size; // tpl_finger_size = tpl_head_size +
				  // tpl_subtemplate_size * n
	uint32_t tpl_head_size;
	uint32_t tpl_subtemplate_size;
	uint8_t *algo_buf;
	uint8_t *feature_buf;
	uint8_t *finger_template_buf;
};

#endif
