/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>

#include <fingerprint/fingerprint_alg.h>

#include <stdio.h>

#include "ft9865_bio_alg.h"

#include "focal_algolib_for_locker.h"

LOG_MODULE_REGISTER(ft9865_bio_alg, LOG_LEVEL_INF);

uint8_t ff_algo_buf[FF_ALGO_SIZE] __attribute__ ((aligned(4))); // Algo buffer
static uint8_t g_feature[FT_TPL_SUBTPL_SIZE]; //feature data for one image
static uint8_t g_finger_template_data[FT_TPL_HEAD_SIZE + FT_TPL_SUBTPL_SIZE * SINGLE_FINGER_ENROLL_NUM] __attribute__ ((aligned(4))); //template data for one finger


static int ft9865_algorithm_init(const struct fingerprint_algorithm *const alg)
{
	LOG_INF("%s", __func__);

	int ret = 0;
    char alg_version[64];
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	algo_param_locker_t algo_params = {
			.rows = 80, 
			.cols = 64,
			.algo_size_limit = FF_ALGO_SIZE,
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
		};

	data->cols = algo_params.cols;
	data->rows = algo_params.rows;
	data->max_enroll_samples = algo_params.enroll_template_num;
	
    ret = focal_algo_set_buffer(ff_algo_buf); 
	if (ret != 0) 
    {
		LOG_ERR("memory must be 4-byte aligned(ret = %d)", ret);
	}
    
	ret = focal_algo_init(algo_params);
	LOG_INF("focal_algo_init ret: %d", ret);
	if (ret != 0) 
    {
		LOG_ERR("algorithm initial failed, ret = %d", ret);
	}

	focal_algo_get_finger_detailed_info(&data->tpl_finger_size, &data->tpl_subtemplate_size, &data->tpl_head_size);
	LOG_INF("algo:finger_size = %d, head_size = %d, sub_tpl_size= %d\n", data->tpl_finger_size, data->tpl_head_size, data->tpl_subtemplate_size);

	sprintf(alg_version, "api_%s_core_", LIBFP_API_LOCKER_VERSION);
	focal_algo_get_version((uint8_t*)&alg_version[strlen(alg_version)]);
	LOG_INF("algo ver: %s", alg_version);
	
	return 0;
}

static int ft9865_algorithm_exit(const struct fingerprint_algorithm *const alg)
{
	LOG_INF("%s", __func__);
	return 0;
}

static int ft9865_enroll_start(const struct fingerprint_algorithm *const alg)
{
	LOG_INF("%s", __func__);
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	data->remain = data->max_enroll_samples;
	memset(g_finger_template_data, 0, sizeof(g_finger_template_data));
	
	return 0;
}

static int ft9865_enroll_step(const struct fingerprint_algorithm *const alg,
			       const uint8_t *const image, int *completion)
{
	LOG_INF("%s", __func__);
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;

	int32_t ret = 0;
	int32_t feature_size = 0;
	uint8_t enroll_index = 0;
	
	if(data->remain == 0)
	{
		data->remain = data->max_enroll_samples;
	}

	enroll_index = data->max_enroll_samples - data->remain;

	memset(g_feature, 0, sizeof(g_feature));
	ret =  focal_algo_get_feature((uint8_t*)image, g_feature, &feature_size);
	if (ret == 0)
	{
		ret = focal_algo_enroll_by_feature(g_feature, enroll_index, g_finger_template_data);
		if(ret == 0)
		{
			LOG_INF("enroll success: %d", enroll_index);
			data->remain -= 1;
			*completion = (data->max_enroll_samples - data->remain) * 100 / data->max_enroll_samples;
			memcpy(g_finger_template_data + data->tpl_head_size + data->tpl_subtemplate_size * enroll_index,
					g_feature, data->tpl_subtemplate_size);
			return FP_ENROLLMENT_RESULT_OK;
		}
		else
		{
			LOG_ERR("enroll failed: %d", enroll_index);
			return FP_ENROLLMENT_RESULT_LOW_QUALITY;
		}
	}
	else
	{
		LOG_ERR("focal_getfeature failed: ret = %d", ret);
	}
	
	return FP_ENROLLMENT_RESULT_INTERNAL_ERROR;
}

static int ft9865_enroll_finish(const struct fingerprint_algorithm *const alg,
				 void *templ)
{
	LOG_INF("%s", __func__);
	struct ft_libfp_data *data = (struct ft_libfp_data *)alg->data;
	
	memcpy(templ, g_finger_template_data, sizeof(g_finger_template_data));
	data->remain = 0;
	
	return FP_ENROLLMENT_RESULT_OK;
}

static int ft9865_match(const struct fingerprint_algorithm *const alg,
			 void *templ, uint32_t templ_count,
			 const uint8_t *const image, int32_t *match_index,
			 uint32_t *update_bitmap)
{
	LOG_INF("%s", __func__);

	int32_t ret = 0;   
	int32_t feature_size = 0;
	uint8_t update_flag = 0;

	memset(g_feature, 0, sizeof(g_feature));
	ret = focal_algo_get_feature((uint8_t*)image, g_feature, &feature_size);
	if (ret == 0)
	{
		for (int i = 0; i < templ_count ; i++)
		{
			ret = focal_algo_verify_by_feature(g_feature, templ + i * FINGER_TEMPLATE_SIZE, &update_flag);
			LOG_INF("identify : %d %d", ret, update_flag);
			if (ret == 0)
			{
				/*match*/ 
				*match_index = i;
				
				if (update_flag)
				{
					/*update template*/
					ret = focal_algo_update_template_by_feature(g_feature, templ + i * FINGER_TEMPLATE_SIZE);
					if (ret == 0)
					{
						*update_bitmap = (0x01 << i);
						return FP_MATCH_RESULT_MATCH_UPDATED;
					}
					else
					{
						return FP_MATCH_RESULT_MATCH_UPDATE_FAILED;
					}
				}
				else
				{
					return FP_MATCH_RESULT_MATCH;
				}
			}
		}
	}
	else
	{
		LOG_ERR("focal_getfeature failed: ret = %d", ret);
	}
	
	return FP_MATCH_RESULT_NO_MATCH;
}

const struct fingerprint_algorithm_api ft9865_api = {
	.init = ft9865_algorithm_init,
	.exit = ft9865_algorithm_exit,
	.enroll_start = ft9865_enroll_start,
	.enroll_step = ft9865_enroll_step,
	.enroll_finish = ft9865_enroll_finish,
	.match = ft9865_match,
};

static struct ft_libfp_data ft9865_libfp_data;

FINGERPRINT_ALGORITHM_DEFINE(ft9865_algorithm, &ft9865_libfp_data, &ft9865_api);
