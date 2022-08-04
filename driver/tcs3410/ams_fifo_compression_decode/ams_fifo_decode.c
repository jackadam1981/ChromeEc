/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
/*#include <unistd.h>*/
#include <stdbool.h>
#include <sys/types.h>

#include "ams_fifo_decode.h"

#define CHAR_BIT 8

/* Global variables */
static size_t previous_bit_index = 0;

/***********************************************************************************
 * Function to get n-bit packetized array for given multi-channel input encode data
 ***********************************************************************************/
static uint32_t get_packetized_array_multichl(uint8_t* input_byte, uint16_t num_bytes, int packet_size)
{
	size_t bit_index = previous_bit_index;
	uint8_t n_bits;
	size_t byte_index = bit_index / 8;
	uint8_t bit_in_byte_index = bit_index % 8;
	uint32_t result = input_byte[byte_index] >> bit_in_byte_index;

	for (n_bits = 8 - bit_in_byte_index; n_bits < packet_size; n_bits += 8)
	{
		if (byte_index < num_bytes )
			result |= input_byte[++byte_index] << n_bits;
		else
			return 0xffff;
	}
		previous_bit_index = packet_size + previous_bit_index;
	return result & ~(~0u << packet_size);
}

/**********************************************************************
 * Function to get n-bit packetized array from given input encode data
 **********************************************************************/
static uint32_t get_packetized_array(uint8_t* input_byte, uint16_t index, int packet_size)
{
	uint8_t n_bits;
	size_t bit_index = packet_size * index;
	size_t byte_index = bit_index / 8;
	uint8_t bit_in_byte_index = bit_index % 8;
	uint32_t result = input_byte[byte_index] >> bit_in_byte_index;

	for (n_bits = 8 - bit_in_byte_index; n_bits < packet_size; n_bits += 8)
		result |= input_byte[++byte_index] << n_bits;

	return result & ~(~0u << packet_size);
}


/**********************************************************
 * FIFO data decode function
 *
 * Only supports 1 channel of raw data.
 *
 **********************************************************/
ams_fifo_decode_result_t fifo_data_decode(void *input, void *output, int num_of_chls, uint16_t num_bytes, uint16_t output_sz_bytes, int *packet_size, ams_fifo_format_t data_format)
{
	int signed_mask;
	int signed_data;
	int chl_num = 0;
	int data_width;
	int i = 0;
	int shift_cnt;

	uint32_t packet_data;
	
	/* Here temp_fifo_data[0] is used for string previous data
		temp_fifo_data[1] is used for channel data
	*/
	uint32_t temp_fifo_data[2];  // used to be -> (uint32_t *)malloc((num_of_chls + 1) * sizeof(uint32_t));
	uint32_t previous_fifo_data[3]= {0x0000};

	uint16_t fifo_output_count = 0;
	uint16_t *fifo_output = (uint16_t *)output;
	uint16_t iterations = (num_bytes * 8) / packet_size[0];

	uint8_t *fifo_input  = (uint8_t *)input;
	uint8_t count_zero_sample_ind = 0;
	uint8_t indicator_status;
	ams_fifo_decode_result_t rc = FIFO_DECODE_SUCCESS;

	if ((input == NULL) || (output == NULL))
	{
		return FIFO_DECODE_NULL_PARAM_ERROR;
	}

	if (num_of_chls != 1)
	{
	    return FIFO_DECODE_UNSUPPORTED_FORMAT;
	}

	switch (data_format)
	{
	case FIFO_FORMAT_UNCOMPRESSED:
		for (i = 0; i < iterations; i++)
		{
			 fifo_output[i] = get_packetized_array(fifo_input, i, packet_size[0]);
		}
	break;
	case FIFO_FORMAT_COMPRESSED:
		temp_fifo_data[0] = 0x0000;
		data_width = (packet_size[0] - 1);

		/* Loop over fifo encoded input data */
		for (i = 0; i < iterations; i++)
		{
			/* Get packetized array of required packet size from the given fifo input encode array */
			packet_data = get_packetized_array(fifo_input, i, packet_size[0]);

			/* Examine the packet indicator bit, which is MSB bit of packet */
			indicator_status = (packet_data >> data_width) & 1;

			/* Discard sample indicator bit of that packet, which is the MSB bit of that packet */
			packet_data   = packet_data & ( ~(1 << data_width));
			//printf("\n%d ind:%d wo_ind:0x%04x ", i, indicator_status, packet_data);

			if (indicator_status == 1)
			{
				/* Assemble the completed decoded samples into the fifo output array*/
				fifo_output[fifo_output_count] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				//printf("temp_fifo_data[0]:0x%04x,fifo_output[%d]:0x%04x\n", temp_fifo_data[0],fifo_output_count, fifo_output[fifo_output_count]);
				temp_fifo_data[0] = 0x0000;
				fifo_output_count++;
				count_zero_sample_ind = 0;
			}
			else
			{
				/* In this case the indicator is 0, therefore the next packet is required in order to complete the decode */
				temp_fifo_data[0] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				count_zero_sample_ind ++;
				continue;
			}
		}
	break;
	case FIFO_FORMAT_DIFFERENCE:
		data_width = (packet_size[0] - 1);

		/* Generate bit mask for "packet_size", which is number of bits representing the number for signed packetized data */
		signed_mask = 1U << data_width;
		//printf("\ndata_width: %d signed_mask:0x%04x ", data_width, signed_mask);

		/* Loop over fifo encoded input data */
		for (i = 0; i < iterations; i++)
		{
			/* Get packetized array of required packet size from the given fifo input encode array */
			packet_data = get_packetized_array(fifo_input, i, packet_size[0]);
			//printf("\n%d  packet_data:0x%04x ", i, packet_data);

			/* check if packet_data is a signed number, i.e MSB bit of packet data set to 1 */
			if (packet_data  & signed_mask)
			{
				/* Transform a 'packet_data' into a sign-extended number */
				packet_data = (packet_data ^ signed_mask) - signed_mask;
				//printf("\nsigned_packet_data:0x%04x ", packet_data);
			}

			/* Assemble the completed decoded samples into the fifo output array*/
			fifo_output[fifo_output_count] = previous_fifo_data[0] + packet_data;
			previous_fifo_data[0] = fifo_output[fifo_output_count];
			//printf("packet_data:0x%04x fifo_output[%d]:0x%04x\n", packet_data, fifo_output_count, fifo_output[fifo_output_count]);
			fifo_output_count++;
		}
	break;
	case FIFO_FORMAT_DIFFERENCE_COMPRESSED:
        fifo_output[0] = 0x0000;
		temp_fifo_data[0] = 0x0000;
		data_width = (packet_size[0] - 1);

		//if (data_width > 4)
		//	signed_mask = 1U << CHAR_BIT;
		//else
		//	signed_mask = 1U << 4;
        signed_mask = 1 << (data_width - 1);

		/* Loop over fifo encoded input data */
		for (i = 0; i < iterations; i++)
		{
			/* Get packetized array of required packet size fron given fifo input encode array */
			packet_data = get_packetized_array(fifo_input, i, packet_size[0]);

			/* Examine the packet indicator bit, which is MSB bit of packet */
			indicator_status = (packet_data >> data_width) & 1;

			/* Discard sample indicator bit of that packet, which is the MSB bit of packet */
			packet_data = packet_data & ( ~(1 << data_width));
			//printf("\n%d ind:%d wo_ind:0x%04x ", i, indicator_status, packet_data);

			/* Assemble the completed decoded samples into the fifo output array */
			if (indicator_status == 1)
			{
				temp_fifo_data[1] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				//printf("temp[0]:0x%04x temp[1]:0x%x ", temp_fifo_data[0], temp_fifo_data[1]);

				/* Assgined to signed integer to compute sign-extended number */
				signed_data = temp_fifo_data[1];

				/* check if packet_data is a signed number, i.e MSB bit of packet data set to 1 */

				//if (temp_fifo_data[1]  & signed_mask)
                if ((packet_data & signed_mask) != 0)
				{
				 shift_cnt = 32 - ((count_zero_sample_ind + 1) * data_width);
                 temp_fifo_data[1] = (uint32_t)((signed_data << shift_cnt) >> shift_cnt);
					/* Transform a 'packet_data' into a sign-extended number */
					//bit_mask = 8 * sizeof(signed_data) - CHAR_BIT;
					//temp_fifo_data[1] = (signed_data << bit_mask) >> bit_mask;
				}
				temp_fifo_data[0] = 0x0000;
				count_zero_sample_ind = 0;
				if (fifo_output_count < output_sz_bytes)
                {
                    if (fifo_output_count != 0)
                        fifo_output[fifo_output_count] = (uint16_t)(fifo_output[fifo_output_count - 1] + temp_fifo_data[1]);
                    else
                        fifo_output[0] = (uint16_t)temp_fifo_data[1];
                    //printf("fifo_output[%d]:0x%04x\n", fifo_output_count, fifo_output[fifo_output_count]);
                }
				//fifo_output[fifo_output_count] = previous_fifo_data[0] + temp_fifo_data[1];
				//previous_fifo_data[0] = fifo_output[fifo_output_count];
				//printf("temp_fifo_data[1]: 0x%04x fifo_output[%d]:0x%04x  ", temp_fifo_data[1], fifo_output_count, fifo_output[fifo_output_count]);
				fifo_output_count++;
			}
			else
			{
				temp_fifo_data[0] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				//printf("temp[0]:0x%04x count_zero_sample_ind:%d ", temp_fifo_data[0], count_zero_sample_ind);
				count_zero_sample_ind ++;
				continue;
			}
		}
	break;
	case FIFO_FORMAT_MULTI_CHL_COMPRESSED:
		fifo_output[0] = 0x0000;
		temp_fifo_data[0] = 0x0000;

		/* Loop over fifo encoded input data till we get required fifo output */
		while(1)
		{
			/* Get packetized array of required packet size from the given fifo input encode array */
			packet_data = get_packetized_array_multichl(fifo_input, num_bytes, packet_size[chl_num]);

			data_width = (packet_size[chl_num] - 1);

			/* We get return value of packet_data of 0xffff if we complete the decoding of num_bytes of encode data samples */
			if (packet_data == 0xffff)
			{
			    rc = FIFO_DECODE_SUCCESS;
				break;
		    }

			/* Examine the packet indicator bit, which is MSB bit of packet  */
			indicator_status = (packet_data >> data_width) & 1;

			/* Discard sample indicator bit of that packet, which is the MSB bit of that packet */
			packet_data = packet_data & ( ~(1 << data_width));
			//printf("\n%d id:%d wo:0x%04x width:%d ", i, indicator_status, packet_data, data_width);

			/* Assemble the completed decoded samples into the fifo output array*/
			if (indicator_status == 1)
			{
				temp_fifo_data[chl_num + 1] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				//printf("temp[0]:0x%04x temp[%d]:0x%04x ", temp_fifo_data[0], chl_num + 1, temp_fifo_data[chl_num + 1]);

				fifo_output[fifo_output_count] = temp_fifo_data[chl_num + 1];
				//printf("chl_%d fifo[%d]0x%04x\n",chl_num, fifo_output_count, fifo_output[fifo_output_count]);
				temp_fifo_data[0] = 0x0000;
				count_zero_sample_ind = 0;
				chl_num++;
				fifo_output_count++;
				
				if (chl_num >= num_of_chls)
					chl_num = 0;
			}
			else
			{
				/* In this case the indicator is 0, therefore the next packet is required in order to complete the decode */
				temp_fifo_data[0] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				//printf("temp[0]:0x%04x cnt:%d ", temp_fifo_data[0], count_zero_sample_ind);
				count_zero_sample_ind ++;
				continue;
			}
			i++;
		}
	break;
	case FIFO_FORMAT_MULTI_CHL_DIFFERENCE_COMPRESSED:
		temp_fifo_data[0] = 0x0000;

		/* Loop over fifo encoded input data till we get required fifo output */
		while(1)
		{
			/* Get packetized array of required packet size from the given fifo input encode array */
			packet_data = get_packetized_array_multichl(fifo_input, num_bytes, packet_size[chl_num]);

			/* We get return value of packet_data of 0xffff if we complete the decoding of num_bytes of encode data samples */
			if (packet_data == 0xffff)
			{
			    rc = FIFO_DECODE_SUCCESS;
				break;
            }
			data_width = (packet_size[chl_num] - 1);

			/* Examine the packet indicator bit, which is MSB bit of packet  */
			indicator_status = (packet_data >> data_width) & 1;

			/* Discard sample indicator bit of that packet, which is the MSB bit of that packet */
			packet_data = packet_data & ( ~(1 << data_width));
			//printf("\n%d id:%d wo:0x%04x ", i, indicator_status, packet_data);

			/* Assemble the completed decoded samples into the fifo output array*/
			if (indicator_status == 1)
			{
				temp_fifo_data[chl_num + 1] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				//printf("temp[0]:0x%04x temp[%d]:0x%04x ", temp_fifo_data[0], chl_num + 1, temp_fifo_data[chl_num + 1]);

				if (data_width == 4)
					data_width = 5;  // Need fo fix this behaviour

				signed_mask = 1U << (data_width - 1);

				/* check if packet_data is a signed number, i.e MSB bit of packet data set to 1 */
				if (temp_fifo_data[chl_num + 1]  & signed_mask)
				{
					/* skip this if bits in packet_data above position of packet_size are already zero */
					temp_fifo_data[chl_num + 1] = temp_fifo_data[chl_num + 1] & ((1U << data_width) - 1);

					/* Transform a 'packet_data' into a sign-extended number */
					temp_fifo_data[chl_num + 1] = (temp_fifo_data[chl_num + 1] ^ signed_mask) - signed_mask;
				}
				fifo_output[fifo_output_count] = previous_fifo_data[chl_num] + temp_fifo_data[chl_num + 1];
				previous_fifo_data[chl_num] = fifo_output[fifo_output_count];
				//printf("chl_%d temp_fifo_data[chl_%d]: 0x%04x  fifo[%d]0x%04x\n",chl_num,chl_num,temp_fifo_data[chl_num+1], fifo_output_count, fifo_output[fifo_output_count]);

				temp_fifo_data[0] = 0x0000;
				count_zero_sample_ind = 0;
				chl_num++;
				fifo_output_count++;

				if (chl_num >= num_of_chls)
					chl_num = 0;
			}
			else
			{
				/* In this case the indicator is 0, therefore the next packet is required in order to complete the decode */
				temp_fifo_data[0] = temp_fifo_data[0] + (packet_data << (count_zero_sample_ind * data_width));
				count_zero_sample_ind ++;
				continue;
			}
			i++;
		}
	break;
	default:
		rc = FIFO_DECODE_UNSUPPORTED_FORMAT;
	break;
	}
	return rc;
}

