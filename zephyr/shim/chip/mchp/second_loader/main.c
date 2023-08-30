/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MEC1727 SOC spi flash update tool
 */

#include "MCHP_MEC172x.h"
#include "common.h"
#include "crc32.h"
#include "gpio.h"
#include "serial.h"
#include "spi_flash.h"

#define HOST_ACK_SIG_REV 0x33CC
#define EC_ACK_BYTE1 0x3C
#define EC_ACK_BYTE2 0xC3

#define PROCESSOR_CLOCK48MHZ 2

#define NUM_CMDS 4
#define CMD_ACK1 0x33u
#define CMD_ACK2 0xCCu
#define CMD_HDR_FILE 0x65u
#define CMD_PRG_FILE 0x67u

#define CRC_MATCH 0

/* Failure Response Status */
#define CRC_MISMATCH 0x01
#define PACKET_ILLEGAL_PAYLOAD_LEN_POS 0x02
#define PACKET_ILLEGAL_HEADER_OFFSET_POS 0x04
#define SPI_OPERATION_FAILURE_POS 0x08
#define PGM_FLASH_DATA_LEN_INCORRECT_POS 0x10
#define HEADER_PACKET_INVALID_POS 0x10

#define SUCCESS_RESPONSE_SIZE 5
#define FAILURE_RESPONSE_SIZE 6

#define SUCCESS_HEADER_EXTRACT 0xA1

#define STATE_WAIT_FOR_HOST_SIG 1
#define STATE_WAIT_FOR_FLASH_CMD 2

#define PGM_PKT_SIZE (128)
/*
 * Following header program packets
 * are expect to be received.
 *
 * SPI util requests are,
 *  - CMD_ERASE
 *  - CMD_PARTIAL_ERASE
 * Those are part of header.
 */
#define NO_SPI_UTIL_REQ 0

enum failure_resp_type {
	HEADER_PACKET_ILLEGAL_OFFSET = 0,
	HEADER_PACKET_ILLEGAL_LENGTH,
	HEADER_PACKET_CRC_FAILURE,
	HEADER_PACKET_INVALID,
	PGM_PACKET_ILLEGAL_OFFSET,
	PGM_PACKET_ILLEGAL_LENGTH,
	PGM_PACKET_CRC_FAILURE,
	PGM_FLASH_DATA_LEN_INCORRECT,
	SPI_OPERATION_FAILURE,
	NO_FAILURE
};

/*
 * 0x33/0xCC ack flags
 * 0x65 header file
 * 0x67 program payload flash file
 */
static const uint8_t known_cmds[NUM_CMDS] = { CMD_ACK1, CMD_ACK2, CMD_HDR_FILE,
					      CMD_PRG_FILE };

typedef struct header_conf {
	uint32_t spi_util_cmd;
	uint32_t flash_start_addr;
	uint32_t flash_data_len_total;
	uint32_t gpio_addr[GPIO_INI_SIZE];
	uint32_t gpio_setting[GPIO_INI_SIZE];
} header_conf;

header_conf hdr_info;

static uint32_t board_init(void);
static void process_rxd_data(uint8_t rxData);
static void make_failure_resp_packet(enum failure_resp_type type,
				     uint8_t command, uint8_t *tx_buff);
static void send_response(enum failure_resp_type failureType, uint8_t command);
static uint8_t is_valid_cmd(uint8_t cmd);
static uint8_t receive_packet(uint8_t command, uint8_t *len);
static uint32_t pgm_256k_chunk(void);
static uint32_t verify_256k_chunk(void);
static void process_pgm_cmd(uint8_t command);
static uint8_t extract_header_info(void);
/*
 * External HOST program loads the resulting binary of building this
 * project into MEC172x SRAM. Host also loads binary data to be programmed to
 * an external Flash device via the QMSPI interface. Once loaded the HOST
 * sets-up the ARMCore parameters in order to have this program execute.
 */
int main(void)
{
	uint8_t rx_data = 0;
	uint32_t ret = NO_ERROR;

	ret = board_init();
	if (ret == BOARD_INIT_ERR) {
		while (1)
			; /* Hold control here */
	}

	serial_init(); /* Output thru UART 115200, 8Bit,NP,1SB */

	while (1) {
		if (serial_receive_host_char(&rx_data)) {
			process_rxd_data(rx_data);
		}
	}
}

void make_failure_resp_packet(enum failure_resp_type type, uint8_t command,
			      uint8_t *tx_buff)
{
	uint8_t i;
	uint32_t resp_crc32;

	tx_buff[0] = command | 0x80;

	switch (type) {
	case HEADER_PACKET_CRC_FAILURE:
	case PGM_PACKET_CRC_FAILURE: {
		tx_buff[1] = CRC_MISMATCH;
		break;
	}
	case HEADER_PACKET_ILLEGAL_LENGTH:
	case PGM_PACKET_ILLEGAL_LENGTH: {
		tx_buff[1] = PACKET_ILLEGAL_PAYLOAD_LEN_POS;
		break;
	}
	case HEADER_PACKET_ILLEGAL_OFFSET:
	case PGM_PACKET_ILLEGAL_OFFSET: {
		tx_buff[1] = PACKET_ILLEGAL_HEADER_OFFSET_POS;
		break;
	}
	case SPI_OPERATION_FAILURE: {
		tx_buff[1] = SPI_OPERATION_FAILURE_POS;
		break;
	}
	case PGM_FLASH_DATA_LEN_INCORRECT: {
		tx_buff[1] = PGM_FLASH_DATA_LEN_INCORRECT_POS;
		break;
	}
	case HEADER_PACKET_INVALID: {
		tx_buff[1] = HEADER_PACKET_INVALID_POS;
		break;
	}
	default:
		break;
	}

	resp_crc32 = crc32_init();
	resp_crc32 = crc32_update(resp_crc32, tx_buff, 2);
	resp_crc32 = crc32_finalize(resp_crc32);
	for (i = 0; i < 4; i++) {
		tx_buff[i + 2] = (uint8_t)(resp_crc32 >> (i << 3));
	}
}

void send_response(enum failure_resp_type failureType, uint8_t command)
{
	uint8_t respSize = SUCCESS_RESPONSE_SIZE;
	uint8_t i;
	uint8_t tx_buff[6];
	uint32_t resp_crc32;

	if (failureType == NO_FAILURE) {
		/* Success Response */
		tx_buff[0] = command;

		resp_crc32 = crc32_init();
		resp_crc32 = crc32_update(resp_crc32, tx_buff, 1);
		resp_crc32 = crc32_finalize(resp_crc32);
		for (i = 0; i < 4; i++) {
			tx_buff[i + 1] = (uint8_t)(resp_crc32 >> (i << 3));
		}
	} else {
		make_failure_resp_packet(failureType, command, tx_buff);
		respSize = FAILURE_RESPONSE_SIZE;
	}

	for (i = 0; i < respSize; i++)
		serial_send_host_char(tx_buff[i]);
}

uint8_t is_valid_cmd(uint8_t cmd)
{
	uint8_t n;
	uint8_t status = 0;

	for (n = 0; n < NUM_CMDS; n++) {
		if (cmd == known_cmds[n]) {
			status = 1;
		}
	}
	return status;
}

uint8_t receive_packet(uint8_t command, uint8_t *len)
{
	uint8_t i;
	uint8_t rx_data;
	uint8_t crc32Status = CRC_MATCH;
	uint8_t *PgmDataPtr;
	uint8_t rx_buff[4];
	uint32_t host_crc32;
	uint32_t command_crc32 = 0;
	uint32_t ptr_offset;

	/* Read 4-byte header (length + (3) header offset (LSB rx first)) */
	for (i = 0; i < 4; i++) {
		while (!(serial_receive_host_char(&rx_data)))
			;
		rx_buff[i] = rx_data;
	}

	/* Parse out the length & offset values */
	i = *len = rx_buff[0];
	ptr_offset = (rx_buff[3] << 16) | (rx_buff[2] << 8) | rx_buff[1];

	/* Initialize this CRC32 packet then add in the command value */
	command_crc32 = crc32_init();
	command_crc32 = crc32_update(command_crc32, &command, 1);
	/* Update command packet CRC with the chunk size & offset bytes */
	command_crc32 = crc32_update(command_crc32, &rx_buff[0], 4);

	if (ptr_offset >= MAX_CHUNK_SIZE) {
		ptr_offset -= MAX_CHUNK_SIZE;
	}

	PgmDataPtr = (uint8_t *)&DATA_IO_BUFFER;
	PgmDataPtr += ptr_offset;

	/* Receive Payload */
	while (i) {
		while (!(serial_receive_host_char(&rx_data)))
			;
		*PgmDataPtr++ = rx_data;
		command_crc32 = crc32_update(command_crc32, &rx_data, 1);
		i--;
	}

	/* Read HOST's CRC32 */
	host_crc32 = 0;
	for (i = 0; i < 4; i++) {
		/* wait for new byte */
		while (!(serial_receive_host_char(&rx_data)))
			;
		host_crc32 += ((uint32_t)rx_data << (i << 3));
	}

	command_crc32 = crc32_finalize(command_crc32);

	if (command_crc32 != host_crc32) {
		crc32Status = CRC_MISMATCH;
	}

	return crc32Status;
}

uint32_t pgm_256k_chunk(void)
{
	uint8_t program_this_sector;
	uint32_t ret;
	uint32_t sector_address = 0;
	uint32_t input_data_offset = 0;

	/*
	 * Read sector content.
	 * Check against content to be programmed.
	 * Perform Erase/Program only if content was different.
	 */
	for (sector_address = hdr_info.flash_start_addr;
	     sector_address < hdr_info.flash_start_addr + MAX_CHUNK_SIZE;) {
		ret = spi_splash_check_sector_content_same(
			sector_address, input_data_offset,
			&program_this_sector);
		if (ret != NO_ERROR)
			return ret;

		if (program_this_sector) {
			/*
			 * Data read from the device was different (even only
			 * one bit) than the input data.
			 */
			program_this_sector = 0;
			ret = spi_flash_sector_erase(sector_address);
			if (ret != NO_ERROR)
				return ret;

			/* Now program this sector in 256 byte pages */
			ret = spi_flash_program_sector(sector_address,
						       input_data_offset);
			if (ret != NO_ERROR)
				return ret;
		}

		sector_address += SECTOR_SIZE;
		input_data_offset += SECTOR_SIZE;
	}

	return NO_ERROR;
}

uint32_t verify_256k_chunk(void)
{
	uint8_t status;
	uint32_t sector_address = 0;
	uint32_t spi_err_flag = NO_ERROR;
	uint32_t input_data_offset = 0;

	for (sector_address = hdr_info.flash_start_addr;
	     sector_address < hdr_info.flash_start_addr + MAX_CHUNK_SIZE;) {
		spi_err_flag = spi_splash_check_sector_content_same(
			sector_address, input_data_offset, &status);
		if (spi_err_flag != NO_ERROR)
			break;

		if (status) {
			/* Content not match */
			spi_err_flag = FLASH_DATA_COMPARE_ERROR;
			break;
		}

		sector_address += SECTOR_SIZE;
		input_data_offset += SECTOR_SIZE;
	}
	return spi_err_flag;
}

void process_pgm_cmd(uint8_t command)
{
	uint8_t status;
	uint8_t len;
	uint32_t ret;
	static uint32_t total_xfer_prog_cnt;

	status = receive_packet(command, &len);
	if (status == CRC_MISMATCH) {
		send_response(PGM_PACKET_CRC_FAILURE, command);
		return;
	}

	if (len != PGM_PKT_SIZE) {
		send_response(PGM_PACKET_ILLEGAL_LENGTH, command);
		total_xfer_prog_cnt = 0;
		return;
	}

	total_xfer_prog_cnt += len;

	if ((total_xfer_prog_cnt % MAX_CHUNK_SIZE) != 0) {
		/* Wait for more chunks */
		send_response(NO_FAILURE, command);
		return;
	}

	/* We have 256K chunk lets start program */
	ret = pgm_256k_chunk();
	if (ret != NO_ERROR) {
		total_xfer_prog_cnt = 0;
		send_response(SPI_OPERATION_FAILURE, command);
		return;
	}

	ret = verify_256k_chunk();
	if (ret != NO_ERROR) {
		total_xfer_prog_cnt = 0;
		send_response(SPI_OPERATION_FAILURE, command);
		return;
	}

	if (total_xfer_prog_cnt == hdr_info.flash_data_len_total) {
		total_xfer_prog_cnt = 0;
		send_response(NO_FAILURE, command);
		return;
	}

	hdr_info.flash_start_addr += MAX_CHUNK_SIZE;

	send_response(NO_FAILURE, command);
}

uint8_t extract_header_info(void)
{
	uint8_t i;
	uint32_t gpio_cnt;
	/* 0x4D434850 ('MCHP') */
	uint32_t hdr_flag;
	/* 0x58454F46 ('XEOF') */
	uint32_t terminator;
	uint32_t *readHdrPtr;

	readHdrPtr = (uint32_t *)&DATA_IO_BUFFER;
	hdr_flag = __builtin_bswap32(*readHdrPtr++);
	hdr_info.spi_util_cmd = __builtin_bswap32(*readHdrPtr++);
	hdr_info.flash_start_addr = __builtin_bswap32(*readHdrPtr++);
	hdr_info.flash_data_len_total = __builtin_bswap32(*readHdrPtr++);

	gpio_cnt = 0;
	for (i = 0; i < GPIO_INI_SIZE; i++) {
		hdr_info.gpio_addr[gpio_cnt] = __builtin_bswap32(*readHdrPtr++);
		hdr_info.gpio_setting[gpio_cnt++] =
			__builtin_bswap32(*readHdrPtr++);
	}

	terminator = __builtin_bswap32(*readHdrPtr);

	if ((hdr_flag != 0x4D434850) || (terminator != 0x58454F46)) {
		return HEADER_PACKET_INVALID;
	}

	return SUCCESS_HEADER_EXTRACT;
}

void process_rxd_data(uint8_t rxData)
{
	uint8_t status;
	uint8_t len;
	static uint32_t test;
	static uint8_t state = STATE_WAIT_FOR_HOST_SIG;

	if (!is_valid_cmd(rxData)) {
		return;
	}

	switch (state) {
	case STATE_WAIT_FOR_HOST_SIG: {
		/* Wait for 0x33CC*/
		if (rxData == CMD_ACK1) {
			/* Got first byte of ACK packet */
			test = rxData << 8;
		} else if (rxData == CMD_ACK2) {
			/* Got 2nd byte of ACK packet */
			test |= rxData;
			if ((uint16_t)(HOST_ACK_SIG_REV) == (uint16_t)test) {
				serial_send_host_char(EC_ACK_BYTE1);
				serial_send_host_char(EC_ACK_BYTE2);

				state = STATE_WAIT_FOR_FLASH_CMD;
			}
			test = 0;
		} else {
			test = 0;
		}
		break;
	}
	case STATE_WAIT_FOR_FLASH_CMD: {
		switch (rxData) {
		case CMD_HDR_FILE: {
			status = receive_packet(rxData, &len);
			if (status == CRC_MISMATCH) {
				send_response(HEADER_PACKET_CRC_FAILURE,
					      rxData);
				break;
			}

			status = extract_header_info();
			if (status == HEADER_PACKET_INVALID) {
				send_response(HEADER_PACKET_INVALID, rxData);
				break;
			}

			spi_flash_init(hdr_info.spi_util_cmd);
			send_response(NO_FAILURE, rxData);
			break;
		}

		case CMD_PRG_FILE: {
			if (!hdr_info.flash_data_len_total) {
				send_response(PGM_FLASH_DATA_LEN_INCORRECT,
					      rxData);
				break;
			}

			process_pgm_cmd(rxData);
			break;
		}
		}
		break;
	}
	default: {
		/* Nothing to do */
		break;
	}
	}
}

uint32_t board_init(void)
{
	uint32_t cnt = 0x10000;
	/* Use 96mhz clock; 2= 48 MHz processor clock */
	PCR_INST->PROC_CLK_CNTRL = PROCESSOR_CLOCK48MHZ;
	/* Enable internal 32khz osc */
	VBAT_INST->VBAT_SRC_32K = 0x1;
	/* Using internal 32KHz */
	PCR_INST->VTR_32K_SRC_b.PLL_REF_SOURCE = 0;

	while (PCR_INST->OSC_ID_b.PLL_LOCK == 0) {
		cnt--;
		if (cnt == 0)
			return (BOARD_INIT_ERR);
	};

	return NO_ERROR;
}
