
#ifndef __FF_FT93XX_API_H__
#define __FF_FT93XX_API_H__

#include <stdint.h>

typedef int (*SENSOR_HW_RESET_FUNC)(void);
typedef int (*SPI_WRITE_FUNC)(uint8_t *tx_buf, uint32_t tx_len);
typedef int (*SPI_WRITE_READ_FUNC)(uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len);
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

#endif
