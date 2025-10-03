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
#define LIBFP_API_LOCKER_VERSION "v3.0.28"

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
 * @brief Flash to RAM copy function pointer type
 *
 * @param[out] ram_addr   Destination RAM address
 * @param[in]  flash_addr Source flash address
 * @param[in]  size       Number of bytes to copy
 */
typedef void (*FLASH_COPY_FUNC)(uint8_t *ram_addr, uint8_t *flash_addr,
				int size);

/**
 * @brief Hardware accelerator function pointer type
 *
 * @param[in]  samp_data Sample data input
 * @param[in]  temp_data Template data input
 * @param[out] out_data  Output data buffer
 * @param[out] out_num   Output count
 * @param[in]  v1        Parameter v1
 * @param[in]  v2        Parameter v2
 * @param[in]  v3        Parameter v3
 * @param[in]  config    Configuration value
 * @param[in]  config2   Secondary configuration value
 */
typedef void (*HARDWARE_ACC_FUNC)(uint8_t *samp_data, uint8_t *temp_data,
				  uint8_t *out_data, uint16_t *out_num,
				  uint32_t v1, uint32_t v2, uint32_t v3,
				  uint32_t config, uint32_t config2);

/**
 * @brief Algorithm initialization parameters structure
 *
 */
typedef struct {
	/** @name Common Variables
	 *  @{ */
	uint32_t rows; /**< Image rows */
	uint32_t cols; /**< Image columns */
	uint32_t algo_size_limit; /**< Max algo RAM size in bytes (min 140KB) */
	uint32_t flash_size_limit; /**< Flash size for algo */
	uint32_t sub_tpl_size; /**< Expected sub template size (v3.0.27+) */

	uint8_t max_finger_num; /**< Maximum number of fingers */
	uint8_t enroll_template_num; /**< Enroll template count (<=
					max_template_num) */

	uint8_t enroll_similarity_enable; /**< 0: disable, >=1: check n previous
					     images for duplicated regions */
	uint8_t enroll_duplicated_finger_enable; /**< 0: disable, >=1: check n
						    images in fingers */
	uint8_t image_quality_enable; /**< 0: disable, 1: enable */
	uint8_t update_template_enable; /**< 0: disable, 1: enable */

	uint8_t log_level; /**< 0:all, 1:vbs, 2:dbg, 3:info, 4:warn, 5:error,
			      6:disable */

	PRINT_FUNC print_func_impl; /**< Print/log callback function */
	/** @} */

	/** @name Segmented Template Read
	 *  For large fingerprints, divide template into smaller sections
	 *  @{ */
	uint8_t finger_template_read_type; /**< 0: normal, 1: segmented
					      execution */
	uint8_t *ram_interim_addr; /**< Interim RAM address */
	int ram_interim_size; /**< Interim RAM size */
	FLASH_COPY_FUNC flash_copy_impl; /**< Flash to RAM copy callback */
	/** @} */

	/** @name Hardware Acceleration
	 *  @{ */
	uint8_t use_harware_acc; /**< Enable hardware acceleration */
	HARDWARE_ACC_FUNC hardware_acc_impl; /**< Hardware accelerator callback
					      */
	/** @} */

	/** @name ZB Variables
	 *  @{ */
	uint8_t enroll_reject_thr; /**< Total reject threshold */
	uint8_t enroll_continue_fail_thr; /**< Continuous fail threshold */
	uint8_t gen_feat_quality_thr; /**< Feature quality threshold */
	uint8_t gen_feat_area_thr; /**< Feature area threshold */
	uint8_t enroll_non_overlap_th; /**< Non-overlap threshold 0~100%, 0:
					  disable */
	/** @} */
} algo_param_locker_t;

/**
 * @brief Initialize fingerprint algorithm parameters
 *
 * @param[in] algo_param Algorithm initialization parameters
 *
 * @retval  0 Success
 * @retval -1 Memory allocation failed
 * @retval -2 Flash or SRAM not enough for algorithm
 * @retval -3 Invalid row or column value
 * @retval -4 max_tpl_num exceeds default limit
 */
int focal_algo_init(algo_param_locker_t algo_param);

/**
 * @brief Set the starting address of algorithm buffer
 *
 * @param[in] algo_buf Algorithm buffer pointer (90KB required)
 *
 * @retval 0      Success
 * @retval others Failure
 */
int focal_algo_set_buffer(uint8_t *algo_buf);

/**
 * @brief Get algorithm version string
 *
 * @param[out] version_buf Buffer to store version string
 */
void focal_algo_get_version(uint8_t *version_buf);

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
 * @brief Enroll finger feature into template
 *
 * @param[in]  feature         Finger feature to enroll
 * @param[in]  enroll_num      Current enrollment index
 * @param[out] finger_template Generated finger template
 *
 * @retval  0 Success
 * @retval -1 Memory error
 */
int focal_algo_enroll_by_feature(uint8_t *feature, uint8_t enroll_num,
				 uint8_t *finger_template);

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
int focal_algo_verify_by_feature(uint8_t *feature, uint8_t *finger_template,
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
int focal_algo_get_image_quality_area(uint8_t *p_src, uint8_t *quality_score,
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

/**
 * @brief Check similarity between two features
 *
 * @param[in]  feature      First finger feature
 * @param[in]  feature_prev Second finger feature
 * @param[out] area         Overlap area percentage (0~100)
 * @param[out] delta_x      Affine transform X offset
 * @param[out] delta_y      Affine transform Y offset
 * @param[out] delta_theta  Affine transform rotation angle
 *
 * @retval  0 Success (features are similar)
 * @retval -1 Memory allocation failed
 * @retval -2 Features not matching
 */
int focal_algo_similar(uint8_t *feature, uint8_t *feature_prev, int *area,
		       int *delta_x, int *delta_y, float *delta_theta);

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_FT98xx_PRIVATE_H_ */
