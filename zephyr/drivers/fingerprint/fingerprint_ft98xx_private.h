/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Private sensor interface */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_FT98xx_PRIVATE_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_FT98xx_PRIVATE_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Sensor operation mode enumeration
 *
 */
typedef enum {
	FOCAL_SENSOR_MODE_IDLE = 0, /**< Idle mode */
	FOCAL_SENSOR_MODE_LOW_POWER = 1, /**< Low power mode */
	FOCAL_SENSOR_MODE_DETECT = 2, /**< Finger detection mode */
} focal_sensor_mode_t;

/**
 * @brief Hardware reset function pointer type
 *
 * @return 0 on success, non-zero on failure
 */
typedef int (*SENSOR_HW_RESET_FUNC)(void);

/**
 * @brief SPI write function pointer type
 *
 * @param[in] tx_buf Transmit buffer
 * @param[in] tx_len Transmit buffer length
 *
 * @return 0 on success, non-zero on failure
 */
typedef int (*SPI_WRITE_FUNC)(uint8_t *tx_buf, uint32_t tx_len);

/**
 * @brief SPI write and read function pointer type
 *
 * @param[in]  tx_buf Transmit buffer
 * @param[in]  tx_len Transmit buffer length
 * @param[out] rx_buf Receive buffer
 * @param[in]  rx_len Receive buffer length
 *
 * @return 0 on success, non-zero on failure
 */
typedef int (*SPI_WRITE_READ_FUNC)(uint8_t *tx_buf, uint32_t tx_len,
				   uint8_t *rx_buf, uint32_t rx_len);

/**
 * @brief Delay function pointer type
 *
 * @param[in] ms Delay time in milliseconds
 */
typedef void (*DELAY_MS_FUNC)(uint32_t ms);

/**
 * @brief Sensor initialization parameters structure
 */
typedef struct {
	uint8_t *raw_buf; /**< Raw image buffer */
	SENSOR_HW_RESET_FUNC hw_rst_func_impl; /**< Hardware reset callback */
	SPI_WRITE_FUNC spi_write_func_impl; /**< SPI write callback */
	SPI_WRITE_READ_FUNC spi_write_read_func_impl; /**< SPI write/read
							 callback */
	DELAY_MS_FUNC delay_ms_func_impl; /**< Delay callback */
} sensor_param_t;

/**
 * @brief Initialize fingerprint sensor
 *
 * @param[in] sensor_params Sensor initialization parameters
 *
 * @retval 0      Success
 * @retval others Failure
 */
int ft_sensor_init(sensor_param_t sensor_params);

/**
 * @brief Query if finger is on sensor
 *
 * @retval 1 Finger on sensor
 * @retval 0 Finger not on sensor
 */
int ft_sensor_query_finger_status_simple(void);

/**
 * @brief Capture sensor data
 *
 * @param[out] img Captured image data buffer
 *
 * @retval 1      Finger touch, capture success
 * @retval 2      Finger leave, capture failed
 * @retval others Failure
 */
int ft_sensor_capture_process(unsigned char *img);

/**
 * @brief Get sensor chip ID
 *
 * @return Sensor chip ID
 */
uint16_t ft_sensor_query_chipid(void);

/**
 * @brief Get sensor image width (columns)
 *
 * @return Number of columns
 */
uint16_t ft_sensor_query_cols(void);

/**
 * @brief Get sensor image height (rows)
 *
 * @return Number of rows
 */
uint16_t ft_sensor_query_rows(void);

/**
 * @brief Set sensor operation mode
 *
 * @param[in] mode Sensor mode (@ref focal_sensor_mode_t)
 *
 * @retval 0      Success
 * @retval others Failure
 */
int ft_sensor_set_mode(int mode);

/**
 * @brief Acquire image with specified capture mode
 *
 * @param[out] img  Captured image data buffer
 * @param[in]  mode Capture type (fingerprint_capture_type)
 *
 * @retval 0      Success
 * @retval others Failure
 */
int ft_sensor_acquire_image_with_mode(uint8_t *img, int mode);

/** @brief Algorithm library version string
 *
 */
#define LIBFP_API_LOCKER_VERSION "v5.0.3"

/**
 * @brief Print/logging function pointer type
 *
 * @param[in] tag    Log tag
 * @param[in] level  Log level
 * @param[in] file   Source file name
 * @param[in] line   Source line number
 * @param[in] format Printf format string
 * @param[in] ...    Variable arguments
 */
typedef void (*PRINT_FUNC)(const char *tag, int level, const char *file,
			   int line, const char *format, ...);

/**
 * @brief Malloc function pointer type
 *
 * @param[in]  size       Number of bytes to malloc
 */
typedef void *(*FUNC_MALLOC)(uint32_t size);

/**
 * @brief free function pointer type
 *
 * @param[in]  ptr  the pointer need to free
 */
typedef void (*FUNC_FREE)(void *ptr);

/**
 * @brief Algorithm initialization parameters structure
 *
 */
typedef struct {
	/*==================== Common Variables ====================*/
	uint32_t rows;
	uint32_t cols;
	uint32_t algo_size_limit; // Maximum available RAM for algorithm
				  // (bytes)(minimum: 64 * 1024 bytes)
	uint8_t max_template_num; // maximum number : limit -> 20, defualt -> 12
	uint8_t enroll_template_num; // enroll_template_num <= max_template_num

	uint8_t enroll_similarity_enable; // n = 0: disable,n >= 1: enable ,
					  // check n previous images  ; check
					  // duplicated regions for enrolling
	uint8_t enroll_duplicated_finger_enable; // n = 0: disable,n >= 1:
						 // enable , check n images in
						 // fingers
	uint8_t image_quality_enable; // 0: disable, 1: enable
	uint8_t update_template_enable; // 0: disable, 1: enable
	uint8_t policy_check_enable; // default -> 1
	uint8_t use_desp_speed_up; // use speed up -> default 1

	uint8_t enroll_quality_thr; // default 45
	uint8_t enroll_area_thr; // default 40
	uint8_t verify_quality_thr; // default 10
	uint8_t verify_area_thr; // default 40

	uint8_t update_frequency_num; // update_frequency_num , default -> 3
	uint8_t log_level; // 0: all, 1: vbs, 2: dbg, 3: info, 4: warn, 5:
			   // error, 6: disable

	uint32_t flash_erase_size; // erase size

	PRINT_FUNC print_func_impl;

	/*==================== ZB Variables ====================*/
	uint8_t enroll_reject_thr; // total reject numbers
	uint8_t enroll_continue_fail_thr;
	uint8_t far_level; // pass mathing level

	/*==================== SZ Variables ====================*/
	uint8_t enroll_similarity_xythr; // 與上一張錄入數據的相對偏移座標
					 // min(rows, cols)/3
	uint8_t enroll_similarity_anglethr; // 與上一張錄入數據的相對偏移角度 20
	uint8_t enroll_similarity_areathr; // 與上一張錄入數據的重疊面積 70
} algo_param_t;

/**
 * @brief Register malloc function
 *
 * @param func The malloc function pointer
 */
void register_ft_malloc(FUNC_MALLOC func);

/**
 * @brief Register free function
 *
 * @param func The free functions poiinter
 */
void register_ft_free(FUNC_FREE func);

/**
 * @brief Initialize fingerprint algorithm parameters
 *
 * @param[in] algo_param Algorithm initialization parameters
 * @retval  0 Success
 * @retval -1 Memory allocation failed
 * @retval -2 Flash or SRAM not enough for algorithm
 * @retval -3 Invalid row or column value
 * @retval -4 max_tpl_num exceeds default limit
 */
int focal_algo_init_chrome(algo_param_t algo_param);

/**
 * @brief de-Initialize fingerprint algorithm parameters
 *
 * @retval 0      Success
 * @retval others Failure
 */
int focal_algo_deinit_chrome(void);

/**
 * @brief Set the starting address of algorithm buffer
 *
 * @param[in] raw_buf Raw data buffer pointer
 * @param[in] isp_buf ISP data buffer pointer
 * @param[in] algo_buf Algorithm buffer pointer
 *
 * @retval 0      Success
 * @retval others Failure
 */
int focal_algo_set_buffer(uint16_t *raw_buf, uint8_t *isp_buf,
			  uint8_t *algo_buf);

/**
 * @brief Get algorithm version string
 *
 * @param[out] version_buf Buffer to store version string
 */
void focal_algo_version(uint8_t *version_buf);

/**
 * @brief Extract features from fingerprint image
 *
 * @param[in]  image        Input image (8-bit BMP)
 * @param[out] feature      Extracted feature buffer
 * @param[out] feature_size Size of extracted feature data
 *
 * @retval  0 Success
 * @retval -1 Memory error
 * @retval -3 Image quality too low
 * @retval -4 Valid area too small
 */
int focal_algo_get_feature(uint8_t *image, uint8_t *feature, int *feature_size);

/**
 * @brief Enroll start
 *
 * @retval  0 Success
 * @retval others Failure
 */
int focal_algo_enroll_start(void);

/**
 * @brief Enroll finger feature into template
 *
 * @param[in]  feature         Finger feature to enroll
 * @param[in]  enroll_num      Current enrollment index
 *
 * @retval  0 Success
 * @retval -1 Memory error
 * @retval -2 enroll failed
 * @retval -3 image quality low
 * @retval -4 valid area low
 * @retval -5 get image quality error
 * @retval -6 move a little
 * @retval -7 finger already existence
 */
int focal_algo_enroll_step(uint8_t *feature, uint8_t enroll_num);

/**
 * @brief Enroll finish, get the enrolled template data and size
 *
 * @param enroll_finger_data enrolled tempate data
 * @param enroll_finger_size template size
 *
 * @retval  0 Success
 * @retval others Failure
 */
int focal_algo_enroll_finish(uint8_t *enroll_finger_data,
			     uint32_t *enroll_finger_size);

/**
 * @brief Cancel enrolling
 *
 * @retval  0 Success
 * @retval others Failure
 */
int focal_algo_enroll_cancel(void);

/**
 * @brief Verify finger feature against template
 *
 * @param[in]  feature         Finger feature to verify
 * @param[in]  finger_template Template to verify against
 * @param[out] update_flag     Template update flag (1: need update, 0: no
 * update)
 *
 * @retval  0 Success
 * @retval -1 Memory error
 * @retval -2 Verification failed
 * @retval -3 Finger is null
 * @retval -4 Template is not valid
 * @retval -5 BCC check error
 * @retval -6 Accessing unused template
 */
int focal_algo_match(uint8_t *feature, uint8_t *finger_template,
		     uint8_t *update_flag);

/**
 * @brief Update finger template with new feature
 *
 * @param[in]     feature         Feature data for update
 * @param[in,out] finger_template Template to be updated
 *
 * @retval  0 Success
 * @retval -1 Memory error
 * @retval -2 Update threshold not reached
 * @retval -3 Update function not enabled
 * @retval -4 Template read error
 * @retval -5 Template not fully learned
 */
int focal_algo_update_template_by_feature(uint8_t *feature,
					  uint8_t *finger_template);

/**
 * @brief Perform ISP (Image Signal Processing) on raw sensor data
 *
 * @param[out] p_dst    Output 8-bit BMP image
 * @param[in]  p_src    Input raw image data
 * @param[in]  rows     Image height
 * @param[in]  cols     Image width
 * @param[in]  isp_type ISP type (0: coating, 1: cover-glass)
 * @param[in]  radius   SUACE radius (default: 3)
 * @param[in]  dintance SUACE dynamic range (default: 255)
 *
 * @retval 0      Success
 * @retval others Failure
 */
int focal_algo_image_isp(uint8_t *p_dst, uint16_t *p_src, int rows, int cols,
			 uint8_t isp_type, uint16_t radius, uint16_t dintance);

/**
 * @brief Get image quality score and valid area
 *
 * @param[in]  p_src         Input image data
 * @param[out] quality_score Image quality score (0~100)
 * @param[out] valid_area    Effective area percentage (0~100)
 *
 * @retval 0      Success
 * @retval others Failure
 */
int focal_algo_get_quality_area(uint8_t *p_src, uint8_t *quality_score,
				uint8_t *valid_area);

/**
 * @brief Get finger and template size information
 *
 * @param[out] finger_size  Size of finger data in bytes
 * @param[out] sub_tpl_size Size of sub-template data in bytes
 * @param[out] header_size  Size of header data in bytes
 *
 * @retval 0      Success
 * @retval others Failure
 */
int focal_algo_get_finger_detailed_info(int *finger_size, int *sub_tpl_size,
					int *header_size);

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_FT98xx_PRIVATE_H_ */
