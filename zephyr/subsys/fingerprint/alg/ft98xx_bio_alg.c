/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ft98xx_bio_alg.h"

#include <stdio.h>

#include <zephyr/drivers/fingerprint/fingerprint_ft98xx_private.h>
#include <zephyr/logging/log.h>

#include <fingerprint/fingerprint_alg.h>

LOG_MODULE_REGISTER(ft98xx_bio_alg, LOG_LEVEL_INF);

/* algo buffer */
static uint8_t g_algo_buf[FT_ALGO_SIZE] __attribute__((aligned(4)));
/* feature data for one image */
static uint8_t g_feature[FT_TPL_SUBTPL_SIZE];
/* template data for one */
static uint8_t g_finger_template_data[FT_TPL_ENROLL_SIZE]
	__attribute__((aligned(4)));

static int ft98xx_algorithm_init(const struct fingerprint_algorithm *const alg)
{
	int ret = 0;
	char alg_version[64];
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	if (!IS_ENABLED(CONFIG_HAVE_FT_LOCKER_PRIVATE_ALGORITHM)) {
		return 0;
	}

	algo_param_locker_t algo_params = {
		.rows = ft_sensor_query_rows(),
		.cols = ft_sensor_query_cols(),
		.algo_size_limit = FT_ALGO_SIZE,
		.flash_size_limit = 0,
		.max_finger_num = MAX_TEMPLATE_ID,
		.enroll_template_num = SINGLE_FINGER_ENROLL_NUM,
		.image_quality_enable = 1,
		.update_template_enable = 1,
		.log_level = 1,
		.enroll_reject_thr = 30,
		.enroll_continue_fail_thr = 1,
		.gen_feat_quality_thr = 30,
		.gen_feat_area_thr = 75,
		.print_func_impl = NULL,
		.finger_template_read_type = 0,
		.use_harware_acc = 0,
	};

	data->cols = algo_params.cols;
	data->rows = algo_params.rows;
	data->max_enroll_samples = algo_params.enroll_template_num;
	data->algo_buf = g_algo_buf;
	data->feature_buf = g_feature;
	data->finger_template_buf = g_finger_template_data;

	ret = focal_algo_set_buffer(data->algo_buf);
	if (ret != 0) {
		LOG_ERR("memory must be 4-byte aligned(ret = %d)", ret);
	}

	ret = focal_algo_init(algo_params);
	if (ret != 0) {
		LOG_ERR("algorithm initial failed, ret = %d", ret);
	}

	focal_algo_get_finger_detailed_info(&data->tpl_finger_size,
					    &data->tpl_subtemplate_size,
					    &data->tpl_head_size);
	LOG_INF("algo:finger_size = %d, head_size = %d, sub_tpl_size= %d\n",
		data->tpl_finger_size, data->tpl_head_size,
		data->tpl_subtemplate_size);

	sprintf(alg_version, "api_%s_core_", LIBFP_API_LOCKER_VERSION);
	focal_algo_get_version((uint8_t *)&alg_version[strlen(alg_version)]);
	LOG_INF("algo ver: %s", alg_version);

	return 0;
}

static int ft98xx_algorithm_exit(const struct fingerprint_algorithm *const alg)
{
	return 0;
}

static int ft98xx_enroll_start(const struct fingerprint_algorithm *const alg)
{
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	if (!IS_ENABLED(CONFIG_HAVE_FT_LOCKER_PRIVATE_ALGORITHM) ||
	    (data->finger_template_buf == NULL)) {
		return -ENOTSUP;
	}

	data->remain = data->max_enroll_samples;
	memset(data->finger_template_buf, 0, FT_TPL_ENROLL_SIZE);

	return 0;
}

static int ft98xx_enroll_step(const struct fingerprint_algorithm *const alg,
			      const uint8_t *const image, int *completion)
{
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	int32_t ret = 0;
	int32_t feature_size = 0;
	uint8_t enroll_index = 0;

	if ((!IS_ENABLED(CONFIG_HAVE_FT_LOCKER_PRIVATE_ALGORITHM)) ||
	    (data->feature_buf == NULL) ||
	    (data->finger_template_buf == NULL)) {
		return -ENOTSUP;
	}

	if (data->remain == 0) {
		data->remain = data->max_enroll_samples;
	}

	enroll_index = data->max_enroll_samples - data->remain;

	memset(data->feature_buf, 0, FT_TPL_SUBTPL_SIZE);
	ret = focal_algo_get_feature((uint8_t *)image, data->feature_buf,
				     &feature_size);

	if (ret != 0) {
		LOG_ERR("focal_getfeature failed: ret = %d", ret);
		*completion = (data->max_enroll_samples - data->remain) * 100 /
			      data->max_enroll_samples;
		return FP_ENROLLMENT_RESULT_LOW_QUALITY;
	}

	ret = focal_algo_enroll_by_feature(data->feature_buf, enroll_index,
					   data->finger_template_buf);
	if (ret == 0) {
		LOG_DBG("enroll success: %d", enroll_index);
		data->remain -= 1;
		*completion = (data->max_enroll_samples - data->remain) * 100 /
			      data->max_enroll_samples;
		memcpy(data->finger_template_buf + data->tpl_head_size +
			       data->tpl_subtemplate_size * enroll_index,
		       data->feature_buf, data->tpl_subtemplate_size);
		return FP_ENROLLMENT_RESULT_OK;
	} else {
		LOG_ERR("enroll failed: %d", enroll_index);
		*completion = (data->max_enroll_samples - data->remain) * 100 /
			      data->max_enroll_samples;
		return FP_ENROLLMENT_RESULT_LOW_QUALITY;
	}

	return FP_ENROLLMENT_RESULT_INTERNAL_ERROR;
}

static int ft98xx_enroll_finish(const struct fingerprint_algorithm *const alg,
				void *templ)
{
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	if ((!IS_ENABLED(CONFIG_HAVE_FT_LOCKER_PRIVATE_ALGORITHM)) ||
	    (data->finger_template_buf == NULL)) {
		return -ENOTSUP;
	}

	if (templ)
		memcpy(templ, data->finger_template_buf, FT_TPL_ENROLL_SIZE);

	if (data)
		data->remain = 0;

	return FP_ENROLLMENT_RESULT_OK;
}

static int ft98xx_match(const struct fingerprint_algorithm *const alg,
			void *templ, uint32_t templ_count,
			const uint8_t *const image, int32_t *match_index,
			uint32_t *update_bitmap)
{
	int32_t ret = 0;
	int32_t feature_size = 0;
	uint8_t update_flag = 0;
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	if ((!IS_ENABLED(CONFIG_HAVE_FT_LOCKER_PRIVATE_ALGORITHM)) ||
	    (data->feature_buf == NULL)) {
		return -ENOTSUP;
	}

	memset(data->feature_buf, 0, FT_TPL_SUBTPL_SIZE);
	ret = focal_algo_get_feature((uint8_t *)image, data->feature_buf,
				     &feature_size);

	if (ret != 0) {
		LOG_ERR("focal_getfeature failed: ret = %d", ret);
		return FP_MATCH_RESULT_NO_MATCH;
	}

	for (int i = 0; i < templ_count; i++) {
		ret = focal_algo_verify_by_feature(
			data->feature_buf, templ + i * FINGER_TEMPLATE_SIZE,
			&update_flag);
		LOG_DBG("identify : %d %d", ret, update_flag);
		if (ret == 0) {
			/*match*/
			*match_index = i;

			if (update_flag) {
				/*update template*/
				ret = focal_algo_update_template_by_feature(
					data->feature_buf,
					templ + i * FINGER_TEMPLATE_SIZE);
				if (ret == 0) {
					*update_bitmap = (0x01 << i);
					return FP_MATCH_RESULT_MATCH_UPDATED;
				} else {
					return FP_MATCH_RESULT_MATCH_UPDATE_FAILED;
				}
			} else {
				return FP_MATCH_RESULT_MATCH;
			}
		}
	}

	return FP_MATCH_RESULT_NO_MATCH;
}

const struct fingerprint_algorithm_api ft98xx_api = {
	.init = ft98xx_algorithm_init,
	.exit = ft98xx_algorithm_exit,
	.enroll_start = ft98xx_enroll_start,
	.enroll_step = ft98xx_enroll_step,
	.enroll_finish = ft98xx_enroll_finish,
	.match = ft98xx_match,
};

static struct ft_libfp_data ft98xx_libfp_data;

FINGERPRINT_ALGORITHM_DEFINE(ft98xx_algorithm, &ft98xx_libfp_data, &ft98xx_api);
