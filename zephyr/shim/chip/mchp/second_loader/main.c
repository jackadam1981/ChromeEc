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
#include "rom_stuff.h"
#include "serial.h"
#include "spi_flash.h"
#include "trace.h"

#define SUCCESSFUL_COMPLETION 1

/* Hex value Divider for 47 */
#define TIMER_PRE_SCL_DIV_48 0x002FUL
#define HOST_ACK_SIG_REV 0x33CC
#define EC_ACK_BYTE1 0x3C
#define EC_ACK_BYTE2 0xC3

#define PROCESSOR_CLOCK48MHZ 2

#define NUM_CMDS 6
#define CMD_ACK1 0x33
#define CMD_ACK2 0xCC
#define CMD_HDR_FILE 0x65
#define CMD_RD_FILE 0x66
#define CMD_PRG_FILE 0x67
#define CMD_GET_RD_CNT 0x68

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

uint8_t spi_initialization;
uint8_t cmd_disp_en = 1;
uint8_t cmd_disp_en_qspi = 1;

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
 * 0x33/0xCC ack flags,
 * 0x65 header file,
 * 0x66 read flash file,
 * 0x67 program payload flash file,
 * 0x68 get read file size count
 */
uint8_t known_cmds[NUM_CMDS] = { CMD_ACK1,    CMD_ACK2,	    CMD_HDR_FILE,
				 CMD_RD_FILE, CMD_PRG_FILE, CMD_GET_RD_CNT };
uint8_t rx_buff[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

uint8_t tx_buff[16] = { 0 };

uint8_t *PgmDataPtr;
volatile uint8_t t_done;

/* 0x4D434850 ('MCHP') */
uint32_t hdr_flag;
uint32_t spi_util_cmd;
uint32_t flash_start_addr;
uint32_t flash_start;
uint32_t host_crc32;
/*
 * Preserve (do not reset at 256k boundary) offset pointer
 * when reading flash memory
 */
uint32_t read_flash_offset_ptr;
uint32_t flash_data_len;
uint32_t flash_data_len_total;
uint32_t gpio_addr[GPIO_INI_SIZE];
uint32_t gpio_setting[GPIO_INI_SIZE];
/* 0x58454F46 ('XEOF') */
uint32_t terminator;
uint32_t transfer_cnt;
uint32_t gpio_cnt;
uint32_t total_xfer_prog_cnt;
uint32_t spi_err_flag;
uint32_t ptr_offset;

volatile uint32_t command_crc32;
volatile uint32_t timer0_timeout;
volatile uint32_t isr_cnt;

uint32_t board_init(void);
void timer_init(void);
void processRxdData(uint8_t rxData);

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
	uint32_t i;

	volatile uint32_t *GPIO_Control_register = 0;

	WDT_INST->WDT_CONTROL_b.WDT_ENABLE = 0;

	/* Configure for JTAG: enable JTAG/SWD, SWD on TCK/TMS */
	EC_REG_BANK_INST->DEBUG_Enable = 5;
	spi_err_flag = 0u;

	spi_err_flag = BOARD_INIT_ERR;
	ret = board_init();
	if (ret == NO_ERROR) {
		spi_err_flag = 0u;
	}

	/* Initialize GPIOs as input 0x40081000UL - 0x4008107C */
	GPIO_Control_register = (uint32_t *)GPIO_000_036_INST_BASE;
	for (i = 0; i < GPIO_000_036_COUNT; i++) {
		*(GPIO_Control_register) = 0x8040;
		GPIO_Control_register++;
	}

	/* 0x40081080UL-0x400810F8 */
	GPIO_Control_register = (uint32_t *)GPIO_040_073_INST_BASE;
	for (i = 0; i < GPIO_040_073_COUNT; i++) {
		/* GPIO062 */
		if (GPIO_Control_register !=
		    (uint32_t *)GPIO_PIN_CONTROL1_ADDR(062)) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register++;
	}

	/* 0x40081100UL-0x4008117C */
	GPIO_Control_register = (uint32_t *)GPIO_100_137_INST_BASE;
	for (i = 0; i < GPIO_100_137_COUNT; i++) {
		/* GPIO116 */
		if (GPIO_Control_register !=
		    (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0116)) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register++;
	}

	/* 0x40081180UL-0x400811F4 */
	GPIO_Control_register = (uint32_t *)GPIO_140_176_INST_BASE;
	for (i = 0; i < GPIO_140_176_COUNT; i++) {
		/*
		 * Check GPIO163, GPIO164, GPIO167, GPIO173,
		 * GPIO174, GPIO176, GPIO177
		 */
		if ((GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0163)) &&
		    (GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0164)) &&
		    (GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0167)) &&
		    (GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0173)) &&
		    (GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0174)) &&
		    (GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0176)) &&
		    (GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0177))) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register++;
	}

	/* 0x40081200UL-0x40081278 */
	GPIO_Control_register = (uint32_t *)GPIO_200_234_INST_BASE;
	for (i = 0; i < GPIO_200_234_COUNT; i++) {
		/* Check GPIO220 and GPIO232 */
		if ((GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0220)) &&
		    (GPIO_Control_register !=
		     (uint32_t *)GPIO_PIN_CONTROL1_ADDR(0232))) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register++;
	}

	ser_init(); /* Output thru UART 115200, 8Bit,NP,1SB */
	trace0(0, MAIN, 0, " ");
	trace0(0, MAIN, 0,
	       " ------------------------------------------------------");
	trace0(0, MAIN, 0, " MEC172x Crisis Recovery Flash Utility Firmware");
	trace0(0, MAIN, 0, " Copyright (c) 2023 Microchip Technology Inc");
	trace0(0, MAIN, 0, " F/W running on MEC172x");

	transfer_cnt = 0;

	if (spi_err_flag) {
		trace1(0, MAIN, 0, "!!! ERROR STATUS: 0x%08X !!!",
		       spi_err_flag);
	}

	while (1) {
		if (receive_host_char(&rx_data)) {
			processRxdData(rx_data);
		}
	}
}

void make_failure_resp_packet(enum failure_resp_type type, uint8_t command)
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
		make_failure_resp_packet(failureType, command);
		respSize = FAILURE_RESPONSE_SIZE;
	}

	for (i = 0; i < respSize; i++)
		send_host_char(tx_buff[i]);
}

uint8_t isValidCmd(uint8_t cmd)
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

uint8_t check_packet_integrity(void)
{
	uint8_t crc32Status = CRC_MATCH;
	if (command_crc32 != host_crc32) {
		trace0(0, MAIN, 0, " !!!!CRC32 failure!!!!");
		crc32Status = CRC_MISMATCH;
	}
	return crc32Status;
}

uint8_t check_spi_util_cmd(void)
{
	uint8_t status = NO_SPI_UTIL_REQ;
	uint32_t ret;

	/*
	 * Check if only doing a chip erase in SPI_Operations - no reason to
	 * load a data file just to erase.
	 */
	if ((spi_util_cmd & CMD_ERASE) || (spi_util_cmd & CMD_PARTIAL_ERASE)) {
		if (spi_util_cmd & CMD_ERASE) {
			trace0(0, MAIN, 0, " Erase Flash ONLY...");
		} else {
			trace0(0, MAIN, 0, " Partially Erase Flash ONLY...");
		}
		ret = SPI_Operations(0);
		if (ret) {
			trace1(0, MAIN, 0, " SPI_Operation ERROR: 0x%08X", ret);
			status = SPI_OPERATION_FAILURE;
		}

		trace0(0, MAIN, 0, " SPI_Operation success");
		status = SUCCESSFUL_COMPLETION;
	}
	return status;
}

void receive_packet(uint8_t command)
{
	uint8_t len;
	uint8_t i;
	uint8_t rx_data;
	uint8_t data;
	uint8_t xfer_cnt = 0;

	/* Read 4-byte header (length + (3) header offset (LSB rx first)) */
	for (i = 0; i < 4; i++) {
		while (!(receive_host_char(&rx_data)))
			;
		rx_buff[i] = rx_data;
	}

	/* Parse out the length & offset values */
	xfer_cnt = len = rx_buff[0];
	ptr_offset = (rx_buff[3] << 16) | (rx_buff[2] << 8) | rx_buff[1];

	/* Initialize this CRC32 packet then add in the command value */
	command_crc32 = 0;
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
	while (len) {
		while (!(receive_host_char(&rx_data)))
			;
		*PgmDataPtr++ = rx_data;
		command_crc32 = crc32_update(command_crc32, &rx_data, 1);
		len--;
	}

	/* Read HOST's CRC32 */
	host_crc32 = 0;
	for (i = 0; i < 4; i++) {
		/* wait for new byte */
		while (!(receive_host_char(&rx_data)))
			;
		data = rx_data;
		host_crc32 += ((uint32_t)data << (i << 3));
	}

	command_crc32 = crc32_finalize(command_crc32);
}

void process_pgm_cmd(uint8_t command)
{
	uint8_t status;
	uint32_t ret;

	if (cmd_disp_en) {
		cmd_disp_en = 0;
		trace0(0, MAIN, 0, "\n Program Flash cmd received");
	}

	receive_packet(command);
	status = check_packet_integrity();
	if (status == CRC_MISMATCH) {
		send_response(PGM_PACKET_CRC_FAILURE, command);
		return;
	}

	transfer_cnt += rx_buff[0];
	total_xfer_prog_cnt += rx_buff[0];

	if (!flash_data_len) {
		trace0(0, MAIN, 0, " !!!ERROR: flash_data_len = 0!!!");
		send_response(PGM_FLASH_DATA_LEN_INCORRECT, command);
		return;
	}

	if (transfer_cnt > MAX_CHUNK_SIZE) {
		trace1(0, MAIN, 0,
		       " !!! ERROR: XFER_CNT > SRAM size = 0x%08X !!!",
		       transfer_cnt);
		send_response(PGM_PACKET_ILLEGAL_LENGTH, command);
		return;
	}

	if ((ptr_offset % 0x1000) == 0) {
		trace1(0, MAIN, 0, "   Transfer offset: 0x%X ", ptr_offset);
	}

	if ((transfer_cnt < flash_data_len) ||
	    (transfer_cnt < MAX_CHUNK_SIZE)) {
		/* Wait for more chunks */
		send_response(NO_FAILURE, command);
		return;
	}

	trace1(0, MAIN, 0, " transfer_cnt 0x%08X", transfer_cnt);

	ret = SPI_Operations(0);
	if (ret) {
		trace1(0, MAIN, 0, " SPI_Operation ERROR: 0x%08X", ret);
		send_response(SPI_OPERATION_FAILURE, command);
		return;
	} else if (total_xfer_prog_cnt == flash_data_len_total) {
		trace2(0, MAIN, 0,
		       " flash_start_addr: 0x%08X,"
		       " flash_data_len: 0x%08X",
		       flash_start, total_xfer_prog_cnt);
		trace0(0, MAIN, 0, " SPI_Operation success");
		send_response(NO_FAILURE, command);
		return;
	}

	flash_start_addr += MAX_CHUNK_SIZE;
	flash_data_len = flash_data_len_total - MAX_CHUNK_SIZE;
	trace2(0, MAIN, 0,
	       " flash_start_addr: 0x%08X, flash_data_len:"
	       " 0x%08X",
	       flash_start_addr, flash_data_len);
	transfer_cnt = 0;
	send_response(NO_FAILURE, command);
}

uint8_t extract_header_info(void)
{
	uint8_t i, ii;
	uint32_t *readHdrPtr;

	cmd_disp_en = 1;
	transfer_cnt = 0;
	total_xfer_prog_cnt = 0;

	readHdrPtr = (uint32_t *)&DATA_IO_BUFFER;
	hdr_flag = __builtin_bswap32(*readHdrPtr++);
	spi_util_cmd = __builtin_bswap32(*readHdrPtr++);
	flash_start = flash_start_addr = __builtin_bswap32(*readHdrPtr++);
	flash_data_len = __builtin_bswap32(*readHdrPtr++);
	flash_data_len_total = flash_data_len;

	if (flash_data_len_total > MAX_CHUNK_SIZE)
		flash_data_len = MAX_CHUNK_SIZE;

	gpio_cnt = 0;
	for (ii = 0; ii < 27; ii++) {
		gpio_addr[gpio_cnt] = __builtin_bswap32(*readHdrPtr++);
		gpio_setting[gpio_cnt++] = __builtin_bswap32(*readHdrPtr++);
	}

	terminator = __builtin_bswap32(*readHdrPtr);

	if ((hdr_flag != 0x4D434850) || (terminator != 0x58454F46)) {
		trace2(0, MAIN, 0,
		       "\n !!!PgmHdrFile corrupt "
		       "header=0x%08X (s/b:0x4D434850), "
		       "terminator=0x%08X (s/b:0x58454F46)!!!",
		       hdr_flag, terminator);
		return HEADER_PACKET_INVALID;
	}

	trace0(0, MAIN, 0, "\n PgmHdrFile.bin parameters received");
	trace1(0, MAIN, 0, " spi_util_cmd: 0x%08X", spi_util_cmd);
	trace1(0, MAIN, 0, " flash_start_addr: 0x%08X", flash_start_addr);
	trace1(0, MAIN, 0, " flash_data_len_total: 0x%08X",
	       flash_data_len_total);
	if (spi_util_cmd & USE_GPIO_INI_PARAMS_FLAG) {
		for (i = 0; i < 27; i++) {
			if (gpio_addr[i]) {
				trace2(0, MAIN, 0, " gpio_addr[%d]: 0x%08X", i,
				       gpio_addr[i]);
				trace2(0, MAIN, 0, " gpio_setting[%d]: 0x%08X",
				       i, gpio_setting[i]);
			}
		}
	}

	return SUCCESS_HEADER_EXTRACT;
}

void processRxdData(uint8_t rxData)
{
	uint8_t status;
	static uint32_t test;
	static uint8_t state = STATE_WAIT_FOR_HOST_SIG;

	if (!isValidCmd(rxData)) {
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
				trace0(491, MAIN, 0,
				       "\n Received Host-to-EC ACK");
				send_host_char(EC_ACK_BYTE1);
				send_host_char(EC_ACK_BYTE2);
				trace0(491, MAIN, 0, " Sent EC-to-Host ACK");
				cmd_disp_en_qspi = 1;
				spi_initialization = 0;
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
			receive_packet(rxData);
			status = check_packet_integrity();
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

			status = check_spi_util_cmd();
			if (status == SPI_OPERATION_FAILURE) {
				send_response(SPI_OPERATION_FAILURE, rxData);
				break;
			}

			send_response(NO_FAILURE, rxData);
			break;
		}

		case CMD_PRG_FILE: {
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

	rom_prog_32kosc();

	if (VBAT_INST->TRIM_CNT_32K == 0)
		VBAT_INST->TRIM_CNT_32K = 0x59;

	while (PCR_INST->OSC_ID_b.PLL_LOCK == 0) {
		cnt--;
		if (cnt == 0)
			return (BOARD_INIT_ERR);
	};

	/* Set Interrupt Control NVIC_EN bit for direct NVIC vectors */
	EC_REG_BANK_INST->INTERRUPT_CONTROL |= 1;

	/*
	 * Need UART2 pins shared with JTAG signals that are not used when in
	 * SWD JTAG mode. Set DEBUG_ENABLE register for 2 wire configuration.
	 */
	EC_REG_BANK_INST->DEBUG_Enable = 5;

	/* Initialize Timer 0 */
	timer_init();

	__enable_irq();

	return NO_ERROR;
}

void timer_delay_1ms(uint32_t msec)
{
	TIMER16_0_INST->INT_EN = 1;
	timer0_timeout = msec;
	t_done = 0;
	isr_cnt = 0;

	while (!t_done)
		;
	TIMER16_0_INST->INT_EN = 0;
}

void timer_init(void)
{
	/*
	 * Initializes timer 0 to interrupt at 1 ms intervals, enables
	 * interrupts.
	 */
	TIMER16_0_INST->CONTROL_b.SOFT_RESET = 1;

	EC_REG_BANK_INST->INTERRUPT_CONTROL |= 1;
	INTS_INST->BLOCK_ENABLE_CLEAR = 0xFFFFFF00;
	INTS_INST->GIRQ23_EN_CLR |= (1 << 0);
	NVIC->ISER[4] |= (1 << 8);
	/* Enable 16-bit basic timer 0 */
	INTS_INST->GIRQ23_EN_SET |= (1 << 0);
	/*1000 => 1 msec */
	TIMER16_0_INST->PRE_LOAD = 1000;
	TIMER16_0_INST->INT_EN = 1;
	TIMER16_0_INST->CONTROL_b.PRE_SCALE = TIMER_PRE_SCL_DIV_48;
	TIMER16_0_INST->CONTROL_b.AUTO_RESTART = 1;
	TIMER16_0_INST->CONTROL_b.ENABLE = 1;
	TIMER16_0_INST->CONTROL_b.START = 1;
}

void BTMR0_IRQHandler(void)
{
	if (INTS_INST->GIRQ23_EN_SET & 1) {
		INTS_INST->GIRQ23_SRC |= 1;
		isr_cnt++;
		if (isr_cnt >= timer0_timeout) {
			isr_cnt = 0;
			t_done = 1;
		}
	}
}
