/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Private sensor interface */

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_FT9865_PRIVATE_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_FT9865_PRIVATE_H_

#include <stddef.h>
#include <stdint.h>

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

/*******************************************************************************
*   Name: ft_sensor_init
*  Brief: init fingerprint sensor
*  Input: null
* Output: null
* Return: 0--success
	  others--fail
*******************************************************************************/
int ft_sensor_init(sensor_param_t sensor_params);

/*******************************************************************************
*   Name: ft_sensor_query_finger_status_simple
*  Brief: judge if finger is on or off
*  Input: null
* Output: null
* Return: 1--finger on sensor
	  0--finger not on sensor
*******************************************************************************/
int ft_sensor_query_finger_status_simple(void);

/*******************************************************************************
*   Name: ft_sensor_capture_process
*  Brief: capture sensor data
*  Input: null
* Output: image data
* Return: 1--finger touch, success to capture image
		  2--finger leave, fail to capture image
	  others--fail
*******************************************************************************/
int ft_sensor_capture_process(unsigned char *img);

/*******************************************************************************
 *   Name: ft_sensor_query_chipid
 *  Brief: return sensor chipid
 *  Input: null
 * Output: null
 * Return: sensor chipid
 *******************************************************************************/
uint16_t ft_sensor_query_chipid(void);

/*******************************************************************************
 *   Name: ft_sensor_query_cols
 *  Brief: return sensor cols
 *  Input: null
 * Output: null
 * Return: sensor cols
 *******************************************************************************/
uint16_t ft_sensor_query_cols(void);

/*******************************************************************************
 *   Name: ft_sensor_query_rows
 *  Brief: return sensor rows
 *  Input: null
 * Output: null
 * Return: sensor rows
 *******************************************************************************/
uint16_t ft_sensor_query_rows(void);

/*******************************************************************************
*   Name: ft_sensor_set_mode
*  Brief: set sensor mode
*  Input: null
* Output: null
* Return: 0--success
	  others--fail
*******************************************************************************/
int ft_sensor_set_mode(int mode);

/*******************************************************************************
*   Name: ft_sensor_acquire_image_with_mode
*  Brief: acquire image with mode
*  Input: mode
* Output: null
* Return: 0--success
	  others--fail
*******************************************************************************/
int ft_sensor_acquire_image_with_mode(uint8_t *img, int mode);

#define LIBFP_API_LOCKER_VERSION "v3.0.27"

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
	uint8_t finger_template_read_type; // 0 = without segmented excution , 1
					   // = with segmented excution
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
} algo_param_locker_t;

/******************************************************************************************************************
*NAME               :   focal_algo_init
*INPUT              :   none
*RETURN             :    0:          ok
			-1:          memory can't not allocation
			-2:          flash or sram not enough to calculator
algorithm -3:          error row or col value -4:          config max_tpl_num >
default max_tpl_num *FUNCTION           :   init fingerprint algorithm parameter
*******************************************************************************************************************/
int focal_algo_init(algo_param_locker_t algo_param);

/******************************************************************************************************************
*NAME               :   focal_algo_set_buffer
*INPUT              :   algo_buf:90K,
*RETURN             :   0: ok
		    :	others:failed
*FUNCTION           :   90K, the starting address of the available memory space
of the algorithm
*******************************************************************************************************************/
int focal_algo_set_buffer(uint8_t *algo_buf);

/******************************************************************************************************************
 *NAME               :   focal_algo_get_version
 *INPUT              :   version_buf    : get version string
 *RETURN             :   void
 *FUNCTION           :   get algorithm versioning
 *******************************************************************************************************************/
void focal_algo_get_version(uint8_t *version_buf);

/******************************************************************************************************************
*NAME               :   focal_algo_get_feature
*INPUT              :   image			: 8bit bmp
		    :	rows			: bmp rows
		    :	cols			: bmp cols
		    :	feature			: finger feature
		    :	feature_size	:feature data size
*RETURN             :   0: ok
			-1          :memory error
			-3          :image quality low
			-4          :valid area low
*FUNCTION           :   get finger data features
*******************************************************************************************************************/
int focal_algo_get_feature(uint8_t *image, uint8_t *feature, int *feature_size);

/******************************************************************************************************************
*NAME               :   focal_algo_enroll_partial_by_feature

* INPUT				:	feature	        : finger feature (curr
input) :	enroll_num	    : 錄製計次數量 :	finger_template	: finger
template
* finger_template   : 	16K buffer

*RETURN             :   0           :ok
			-1          :memory error
*FUNCTION           :   finger enroll
*******************************************************************************************************************/
int focal_algo_enroll_by_feature(uint8_t *feature, uint8_t enroll_num,
				 uint8_t *finger_template);

/******************************************************************************************************************
*NAME               :   focal_algo_verify_partial_by_feature

*INPUT              :
*feature            : current finger feature
*finger_template    : finger template
*update_flag        : template update flags：1.update;2.no update

*RETURN             :	0          :ok
			-1          :memory error
			-2          :verify failed
			-3          :finger is null
			-4          :finger_template is not valid
			-5          :bcc check error
			-6          :access an unused store template

*FUNCTION           :   focal_algo_verify
*******************************************************************************************************************/
int focal_algo_verify_by_feature(uint8_t *feature, uint8_t *finger_template,
				 uint8_t *update_flag);
/******************************************************************************************************************
*NAME               :   focal_algo_update_template_partial_by_feature
*INPUT              :   update          : 0:樣板不更新，1:樣板更新
			finger_id       : 指定更新的手指ID
*OUTPUT             :   0               : 樣板更新成功
			-1              : 記憶體存取或配置失敗
			-2              : 未達到更新閾值
			-3              : 樣板更新未啟用
			-4              : 樣板讀取範圍異常
			-5              : 樣板尚未完全學習
*FUNCTION           :   template update
*******************************************************************************************************************/
int focal_algo_update_template_by_feature(uint8_t *feature,
					  uint8_t *finger_template);

/******************************************************************************************************************
 *NAME               :   focal_algo_image_isp
 *INPUT              :   *p_dst     : isp result
 *                       *p_src     : input image buffer(u16 , 12 bit)
 *                       rows       : image height
 *                       cols       : image width
 *                       isp_type   : 0: coating, 1: cover-glass
 *                       radius     : radius of suace // default: 3
 *                       distance   : dynamic range of suace // default: 255
 *OUTPUT             :   none
 *FUNCTION           :   focal_algo_image_isp
 *******************************************************************************************************************/
int focal_algo_image_isp(uint8_t *p_dst, uint16_t *p_src, int rows, int cols,
			 uint8_t isp_type, uint16_t radius,
			 uint16_t dintance); // modify in v4.3.1

/******************************************************************************************************************
*NAME               :   focal_algo_get_image_quality_area
*INPUT              :   p_src     			: input image buffer(u16
, 12 bit) quality_score       : image quality valid_area       	: effective area
of the image *RETURN             :   0: ok others:failed *FUNCTION           :
get the quality score and effective area of the image
*******************************************************************************************************************/
int focal_algo_get_image_quality_area(uint8_t *p_src, uint8_t *quality_score,
				      uint8_t *valid_area);

/******************************************************************************************************************
*NAME               :   focal_algo_export_sensor_param
*INPUT              :   param     			: the current parameter
data of the sensor param_size	        : the current parameter data size of the
sensor

*RETURN             :   0: ok
			others:failed
*FUNCTION           :   get the current parameter data of the sensor
*******************************************************************************************************************/
int focal_algo_export_sensor_param(uint8_t *param, uint32_t *param_size);

/******************************************************************************************************************
*NAME               :   focal_algo_export_sensor_param
*INPUT              :   param     			: the current parameter
data of the sensor param_size	        : the current parameter data size of the
sensor

*RETURN             :   0: ok
			others:failed
*FUNCTION           :   import sensor parameter data
*******************************************************************************************************************/
int focal_algo_import_sensor_param(uint8_t *param, uint32_t param_size);

/******************************************************************************************************************
 *NAME               :   focal_algo_feature_match
 *INPUT              :
 * feat1             : first sample feature
 * feat2             : second sample feature
 *RETURN             :
 * 0                 : match success
 * -1                : memory error
 * -2                : match failed
 *FUNCTION           : two samples match
 *******************************************************************************************************************/
int focal_algo_feature_match(uint8_t *feat1, uint8_t *feat2, int *overlap_area);

/******************************************************************************************************************
*NAME               : focal_algo_get_update_data_per_finger
*INPUT              :
    tpl_update_info_addr        : init address
    sub_template        : Subtemplate data
    template_head       : Template header data

*RETURN             : 0 : ok
		      others : failed
*FUNCTION           : get the template information for the template update
*******************************************************************************************************************/
int focal_algo_get_update_data_per_finger(int *tpl_update_info_addr,
					  uint8_t *sub_template,
					  uint8_t *template_head);

/******************************************************************************************************************
*NAME               : focal_algo_get_finger_detailed_info
*INPUT              :
finger_size         : The size of the finger data.
sub_tpl_size        : The size of the subtemplate data.
header_size         : The size of the header data.

*RETURN             : 0 : ok
others : failed
*FUNCTION           : get the finger and the template size
*******************************************************************************************************************/
int focal_algo_get_finger_detailed_info(int *finger_size, int *sub_tpl_size,
					int *header_size); // new 2.5.1

/******************************************************************************************************************
*NAME               :   focal_algo_check_back_line
*INPUT              :   *p_dst     : p_src     : input image buffer(u16 , 12
bit)
*                       rows       : image height
*                       cols       : image width
*                       isp_type   : 0: coating, 1: cover-glass
*                       radius     : radius of suace // default: 3
*                       distance   : dynamic range of suace // default: 255
*RETURN             : 0 : ok
		      others : failed
*FUNCTION           : get whether there are bad wires on the surface of the
sensor
*******************************************************************************************************************/
int focal_algo_check_back_line(uint16_t *p_src, int rows, int cols);

/******************************************************************************************************************
*NAME               :   focal_algo_similar

* INPUT				:	feature	        : current finger feature
		    :	feature_prev	: previous finger feature
		    :	area	        : overlap area
		    :	delta_x     	: affine matrix dx offset
		    :	delta_y     	: affine matrix dy offset
		    :   delta_theta     : affine matrix theta

*RETURN             :   0           :ok
			-1          :memory can't not allocation
			-2          :template not matching

*FUNCTION           :   finger enroll
*******************************************************************************************************************/
int focal_algo_similar(uint8_t *feature, uint8_t *feature_prev, int *area,
		       int *delta_x, int *delta_y, float *delta_theta);

#endif
