/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Private sensor interface */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_FT98xx_PRIVATE_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_FT98xx_PRIVATE_H_

#include <stddef.h>
#include <stdint.h>

typedef enum {
	FOCAL_SENSOR_MODE_IDLE = 0,
	FOCAL_SENSOR_MODE_LOW_POWER = 1,
	FOCAL_SENSOR_MODE_DETECT = 2,
} focal_sensor_mode_t;

typedef int (*SENSOR_HW_RESET_FUNC)(void);
typedef int (*SPI_WRITE_FUNC)(uint8_t *tx_buf, uint32_t tx_len);
typedef int (*SPI_WRITE_READ_FUNC)(uint8_t *tx_buf, uint32_t tx_len,
				   uint8_t *rx_buf, uint32_t rx_len);
typedef void (*DELAY_MS_FUNC)(uint32_t ms);

typedef struct {
	uint8_t *raw_buf;
	SENSOR_HW_RESET_FUNC hw_rst_func_impl;
	SPI_WRITE_FUNC spi_write_func_impl;
	SPI_WRITE_READ_FUNC spi_write_read_func_impl;
	DELAY_MS_FUNC delay_ms_func_impl;
} sensor_param_t;

/**
 * @brief init fingerprint sensor
 *
 * @param sensor_params 	sensor init params
 * @return 0:           success
 *         others: 		fail.
 */
int ft_sensor_init(sensor_param_t sensor_params);

/**
 * @brief query if finger is on or off
 *
 * @return 1:			finger on sensor
 *         0:			finger not on sensor
 */
int ft_sensor_query_finger_status_simple(void);

/**
 * @brief capture sensor data
 *
 * @param[out] img 		the captured image data
 * @return 1:			finger touch, success to capture image
 *         2:			finger leave, fail to capture image
 *         others:		fail
 */
int ft_sensor_capture_process(unsigned char *img);

/**
 * @brief return sensor chipid
 *
 * @return sensor chipid
 */
uint16_t ft_sensor_query_chipid(void);

/**
 * @brief return sensor cols
 *
 * @return sensor cols
 */
uint16_t ft_sensor_query_cols(void);

/**
 * @brief return sensor rows
 *
 * @return sensor rows
 */
uint16_t ft_sensor_query_rows(void);

/**
 * @brief set sensor mode
 *
 * @param[in] mode 		enum focal_sensor_mode_t
 * @return 0:			success
 *         others:		fail
 */
int ft_sensor_set_mode(int mode);

/**
 * @brief acquire image with mode
 *
 * @param[out] img 		the captured image data
 * @param[in] mode 		enum fingerprint_capture_type
 * @return 0:			success
 *         others:		fail
 */
int ft_sensor_acquire_image_with_mode(uint8_t *img, int mode);

#define LIBFP_API_LOCKER_VERSION "v3.0.28"

typedef void (*PRINT_FUNC)(const char *tag, int level, const char *file,
			   int line, const char *format, ...);
typedef void (*FLASH_COPY_FUNC)(uint8_t *ram_addr, uint8_t *flash_addr,
				int size);
typedef void (*HARDWARE_ACC_FUNC)(uint8_t *samp_data, uint8_t *temp_data,
				  uint8_t *out_data, uint16_t *out_num,
				  uint32_t v1, uint32_t v2, uint32_t v3,
				  uint32_t config, uint32_t config2);

typedef struct {
	/*==================== Common Variables ====================*/
	uint32_t rows;
	uint32_t cols;
	uint32_t algo_size_limit; // max algo ram size (bytes) (minimum. 140 *
				  // 1024)
	uint32_t flash_size_limit; // flash size for algo
	uint32_t sub_tpl_size; // expected sub template size // add in v3.0.27

	uint8_t max_finger_num;
	uint8_t enroll_template_num; // enroll_template_num <= max_template_num

	uint8_t enroll_similarity_enable; // n = 0: disable,n >= 1: enable ,
					  // check n previous images  ; check
					  // duplicated regions for enrolling
	uint8_t enroll_duplicated_finger_enable; // n = 0: disable,n >= 1:
						 // enable , check n images in
						 // fingers
	uint8_t image_quality_enable; // 0: disable, 1: enable
	uint8_t update_template_enable; // 0: disable, 1: enable

	uint8_t log_level; // 0: all, 1: vbs, 2: dbg, 3: info, 4: warn, 5:
			   // error, 6: disable

	PRINT_FUNC print_func_impl;

	// if finger size is too large , the following parameters need to be set
	// to dive the fingerprint template into smaller sections
	uint8_t finger_template_read_type; // 0 = without segmented execution ,
					   // 1 = with segmented execution
	uint8_t *ram_interim_addr; // ram address
	int ram_interim_size; // ram size
	FLASH_COPY_FUNC flash_copy_impl; // callback function ,implementing the
					 // process of copying the
					 // finger_template from flash to ram

	// hardware_acc
	uint8_t use_harware_acc;
	HARDWARE_ACC_FUNC hardware_acc_impl;
	/*==================== ZB Variables ====================*/
	uint8_t enroll_reject_thr; // total reject numbers
	uint8_t enroll_continue_fail_thr;
	uint8_t gen_feat_quality_thr;
	uint8_t gen_feat_area_thr;
	uint8_t enroll_non_overlap_th; // The contribution area threshold for
				       // enrollment samples, 0~100(percent), 0
				       // is disable
} algo_param_locker_t;

/**
 * @brief init fingerprint algorithm parameter
 *
 * @param[in] algo_param
 * @return 0:           ok
 *         -1:          memory can't not allocation
 *         -2:          flash or sram not enough to calculator algorithm
 *         -3:          error row or col value
 *         -4:          config max_tpl_num > default max_tpl_num
 */
int focal_algo_init(algo_param_locker_t algo_param);

/**
 * @brief set the starting address of the available memory space
 *
 * @param[int] algo_buf 	algo_buf:90K
 * @return 0:           ok
 *         others:		failed
 */
int focal_algo_set_buffer(uint8_t *algo_buf);

/**
 * @brief get algorithm version
 *
 * @param[out] version_buf 	the algorithm version data
 */
void focal_algo_get_version(uint8_t *version_buf);

/**
 * @brief get finger data features
 *
 * @param[in] image 			8bit bmp
 * @param[out] feature 			finger feature
 * @param[out] feature_size 	feature data size
 * @return 0: 			ok
 *         -1:      	memory error
 *         -3:      	image quality low
 *         -4:      	valid area low
 */
int focal_algo_get_feature(uint8_t *image, uint8_t *feature, int *feature_size);

/**
 * @brief finger enroll
 *
 * @param[in] feature 				finger feature ready to enroll
 * @param[in] enroll_num 			current enroll index
 * @param[out] finger_template 		the generated finger template
 * @return 0:           ok
 *         -1:          memory error
 */
int focal_algo_enroll_by_feature(uint8_t *feature, uint8_t enroll_num,
				 uint8_t *finger_template);

/**
 * @brief finger verify
 *
 * @param[in] feature 				finger feature ready to verify
 * @param[in] finger_template 		finger template
 * @param[out] update_flag 			the flag indicate that need to
 * update template or not, 1: need update, 0: no need update
 * @return 0:           ok
 *         -1:          memory error
 *         -2:          verify failed
 *         -3:          finger is null
 *         -4:          finger_template is not valid
 *         -5:          bcc check error
 *         -6:          access an unused store template
 */
int focal_algo_verify_by_feature(uint8_t *feature, uint8_t *finger_template,
				 uint8_t *update_flag);

/**
 * @brief template update
 *
 * @param[in] feature 				the feature data need to update
 * @param[in] finger_template	the template need to be update to
 * @return 0:           ok
 *         -1:			memory error
 *         -2:			not reach the threshold of update
 *         -3:			update function not enabled
 *         -4:			the template read error
 *         -5:			the template not study fully
 */
int focal_algo_update_template_by_feature(uint8_t *feature,
					  uint8_t *finger_template);

/**
 * @brief do isp for raw data
 *
 * @param[out] p_dst 		isp result, 8bit bmp data
 * @param[in] p_src 		input image raw data
 * @param[in] rows 			image height
 * @param[in] cols 			image width
 * @param[in] isp_type 		0: coating, 1: cover-glass
 * @param[in] radius 		radius of suace, default: 3
 * @param[in] dintance 		dynamic range of suace, default: 255
 * @return 0:           ok
 *         others:		 fail
 */
int focal_algo_image_isp(uint8_t *p_dst, uint16_t *p_src, int rows, int cols,
			 uint8_t isp_type, uint16_t radius, uint16_t dintance);

/**
 * @brief get the quality score and effective area of the image
 *
 * @param[in] p_src 				input image data
 * @param[out] quality_score 		image quality, 0 ~ 100
 * @param[out] valid_area 			effective area of the image, 0 ~
 * 100
 * @return 0:           ok
 *         others:		fail
 */
int focal_algo_get_image_quality_area(uint8_t *p_src, uint8_t *quality_score,
				      uint8_t *valid_area);

/**
 * @brief get the finger and the template size
 *
 * @param[out] finger_size 			The size of the finger data.
 * @param[out] sub_tpl_size 		The size of the subtemplate data.
 * @param[out] header_size 			The size of the header data.
 * @return 0:           ok
 *         others:		fail
 */
int focal_algo_get_finger_detailed_info(int *finger_size, int *sub_tpl_size,
					int *header_size);

/**
 * @brief check two features if similar
 *
 * @param feature 				1st finger feature
 * @param feature_prev 			2nd finger feature
 * @param area 					overlap area, 0 ~ 100
 * @param delta_x 				affine matrix dx offset
 * @param delta_y 				affine matrix dy offset
 * @param delta_theta 			affine matrix theta
 * @return 0:           ok
 *         -1:			memory can't not allocation
 *         -2:			template not matching
 */
int focal_algo_similar(uint8_t *feature, uint8_t *feature_prev, int *area,
		       int *delta_x, int *delta_y, float *delta_theta);

#endif
