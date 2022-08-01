/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __AMS_FIFO_DECODE_H__
#define __AMS_FIFO_DECODE_H__

/* supported fifo formats  */
typedef enum _e_fifo_data_format{
	FIFO_FORMAT_UNCOMPRESSED = 1,
	FIFO_FORMAT_DIFFERENCE,
	FIFO_FORMAT_COMPRESSED,
	FIFO_FORMAT_DIFFERENCE_COMPRESSED,
	FIFO_FORMAT_MULTI_CHL_COMPRESSED,
	FIFO_FORMAT_MULTI_CHL_DIFFERENCE_COMPRESSED
}ams_fifo_format_t;

/* supported fifo decoded results  */
typedef enum _e_fifo_decode_result{
	FIFO_DECODE_SUCCESS,
	FIFO_DECODE_FAILURE,
	FIFO_DECODE_UNSUPPORTED_FORMAT,
	FIFO_DECODE_NULL_PARAM_ERROR
}ams_fifo_decode_result_t;

ams_fifo_decode_result_t fifo_data_decode(void *input, void *output, int num_channels, uint16_t num_bytes, uint16_t output_sz_bytes, int *packet_size, ams_fifo_format_t data_format);

#endif /* __AMS_FIFO_DECODE_H__ */
