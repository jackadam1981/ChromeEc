/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "MCHP_MEC172x.h"
#include "common.h"
#include "gpio.h"
#include "trace.h"

#include <stdint.h>

#define PAGE_SIZE 256
#define FULL_CHIP_ERASE_FLAG 0xFFFFFFFF
#define ADDR_16MB 0x1000000
#define HANDSHAKE_TIMEOUT_LONG 1000000
#define HANDSHAKE_TIMEOUT 100000
#define TIMEOUT_870MS 1
/* 4K byte buffer from 0xCA400 - 0xCB400 */
#define READ_SECTOR_BUFFER (*((volatile uint32_t *)0xCA400))
#define WREN_CMD 0x06
#define WRSR_CMD 0x01
#define ERASE_SECTOR 0x20
#define ERASE_SECTOR_4B 0x21
#define ERASE_CHIP 0xC7
#define ERASE_CHIP_MICRON 0xC4
#define FAST_READ 0x0B
#define READ_STATUS 0x05
#define PAGE_PROGRAM 0x02
#define PAGE_PROGRAM_4B 0x12
#define RSTEN 0x66
#define SPI_RST 0x99
#define FAST_READ_QUAD_OUTPUT 0x6B
#define JEDEC_ID 0x9F
#define ENTER_4BYTE 0xB7
#define EXIT_4BYTE 0xE9
#define GLOBAL_UNLOCK_CMD 0x98
#define FLASH_COMPARE_ERR (1 << 0)
#define SPI_WAIT_BUSY_TIMEOUT_ERR (1 << 12)

#define MBX_ERR_INVALID_DATA_LENGTH (1 << 15)
#define MBX_ERR_INVALID_DATA_START (1 << 16)

#define MBX_DMA_WRITE_ERR (1 << 17)
#define MBX_DMA_READ_ERR (1 << 18)
#define MBX_ERASE_ERR (1 << 19)

#define POLL_STATUS_TO_ERR (1 << 20)
#define UNSUPPORTED_FLASH_DEV_ERR (1 << 23)
#define ADDRESS_MODE_ERROR (1 << 24)
#define INVALID_CMD_REC_ERROR (1 << 25)

#define QUAD_FLAG (1 << 1)
#define CS_FLAG (1 << 2)
/* 1 = Int. Flash, 0 = Ext. flash = 0 */
#define SPI_INT_EXT_FLAG (1 << 3)
#define WSR_FLAG (1 << 4)
#define TOGGLE_CS_FLAG (1 << 5)
/* 1=1.8v, 0=3.3v */
#define V1P8_FLAG (1 << 6)

#define CMD_PROG (1 << 8)
#define CMD_VERIFY (1 << 10)
#define CMD_READ (1 << 11)
#define CMD_PROG_ONLY (1 << 13)

/* Read device ID (External SPI devices only) */
#define CMD_READ_ID (1 << 14)

#define WEL_BIT (1 << 1)

/*
 * QSPI timeout s/w loop takes ~ 2.92 usec So, 200sec/0.00000292sec = 69000000.
 * 69000000 / HANDSHK_TIMEOUT = 690
 */
#define QTIMEOUT_200SEC 690

#define QMSPI_ACTIVATE 0x00000001
#define QMSPI_RESET 0x00000002
#define QMSPI_TRANSFER_LEN_IN_BYTES 0x00000400
#define QMSPI_CLOSE_XFER_EN 0x00000200
#define QMSPI_TX_EN 0x00000004
#define QMSPI_RX_EN 0x00000040
#define QMSPI_START 0x01

#define QMSPI_TRANSFER_COMPLETE 0x00000001
#define QMSPI_DESCR_BUFF_EN 0x00010000
#define QMSPI_DESCR_BUFF1 0x00001000
#define QMSPI_DESCR_BUFF_LAST 0x00010000
#define QMSPI_DESCR_LAST 0x00010000
#define QMSPI_TX_EN_0MODE 0x00000008
#define QMSPI_RX_BUFF_REQ 0x00004000
#define QMSPI_SINGLE_MODE 0x0
#define QMSPI_QUAD_MODE 0x00000002
#define QMSPI_DUMY_COMMAND 0xDD
#define QMSPI_CLR_DATA_BUFF 0x04
#define QMSPI_TX_DMA_4BYTE 0x00000030
#define QMSPI_RX_DMA_4BYTE 0x00000180
#define DMA_XFER_4BYTE 0x4
#define STAT_BUSY_BIT (1 << 0)
#define MICRON_ID 0x20
#define WINBOND_ID 0xEF
#define MACRONIX_ID 0xC2
#define MICROCHIP_ID 0xBF
#define DEV_16MB 0x18
#define DEV_32MB 0x19

#define DEV_16MB_TYPE (1 << 0)
#define DEV_32MB_TYPE (1 << 1)
#define DEVICE_MASK (0xFC)
#define WINBOND_DEVICE (1 << 2)
#define MACRONIX_DEVICE (1 << 4)
#define MICRON_DEVICE (1 << 5)
#define MCHP_DEVICE (1 << 6)

#define ENABLE_4B (1 << 2)
#define DISABLE_4B (1 << 3)

#define TOGGLE_ASSERT 0
#define TOGGLE_DEASSERT 1

#define DEFAULT_DEVICE 0

#define CLK_DIV 1

typedef struct {
	uint8_t flash_type;
	uint8_t cs_toggle;
	uint8_t qmspi_cs;
	uint8_t spi_quad_mode;
	uint8_t clear_spi_status;
	uint8_t current_flash_addr_mode;
	uint32_t host_cntrl_conf;

	uint8_t *ReadDataPtr;
	uint32_t *chip_sel_addr;
} flash_config;

flash_config flash_conf = {
	.flash_type = DEFAULT_DEVICE,
};

extern uint8_t spi_initialization;
extern uint8_t cmd_disp_en_qspi;
extern uint32_t spi_util_cmd;
extern uint32_t flash_start_addr;
extern uint32_t flash_data_len;
extern uint32_t spi_err_flag;
extern uint32_t gpio_addr[GPIO_INI_SIZE];
extern uint32_t gpio_setting[GPIO_INI_SIZE];

static void erase_program(void);
static void program_sector(uint32_t sector_address, uint32_t input_data_offset);
static void erase_sector(uint32_t sector_address, uint32_t ip_offset,
			 uint8_t *pgm);
static void exit_extended_mode(void);
static void ClearQMSPI_Status(void);
static uint32_t QSPI_EraseSector(uint32_t addr);
static uint32_t QSPI_DMA_Write(uint32_t addr, uint8_t *data_src,
			       uint32_t length);
static uint32_t QSPI_DMA_Read(uint32_t addr, uint32_t length);
static uint32_t QSPI_RESET(void);
static uint32_t QSPI_WaitForNotBusy(uint32_t extended_timeout);
static uint32_t PollforQMSPI_Status(uint32_t stat);
static void QSPI_Init(void);
static void Reset_DMA0(void);
static void QSPI_GPIO_Init(void);
static void Init_Signals(void);
static uint32_t QSPI_ReadID(void);
static uint32_t flash_4byte_mode(uint8_t mode);
static uint32_t check_32MB_address(uint32_t data_length);
static uint32_t QSPI_ReadStatus(void);
static uint32_t QSPI_WriteStatus(void);
static void manual_toggle(uint32_t state);
static uint32_t Write_Enable(void);

uint32_t SPI_Operations(uint32_t mem_offset_ptr)
{
	uint8_t *input_data_ptr = 0;
	uint8_t *DwnloadedDataPtr;
	uint32_t start_sector_addr = 0;
	uint32_t data_length = 0;
	uint32_t sector_address = 0;
	uint32_t input_data_offset = 0;
	uint32_t ret = NO_ERROR;
	uint32_t i;
	uint32_t fill_size = 0;
	uint32_t start_fill_addr = 0;
	static uint8_t one_shot_event;

	flash_conf.host_cntrl_conf = spi_util_cmd;
	if ((flash_conf.host_cntrl_conf & 0x7F00) == 0) {
		ret = INVALID_CMD_REC_ERROR;
		return ret;
	}

	if (spi_err_flag == BOARD_INIT_ERR) {
		trace1(0, MAIN, 0, " BOARD_INIT_ERR: 0x%08X", spi_err_flag);
		exit_extended_mode();
		return spi_err_flag;
	}

	input_data_ptr = (uint8_t *)&DATA_IO_BUFFER;
	data_length = flash_data_len;
	start_sector_addr = flash_start_addr;
	flash_conf.ReadDataPtr = (uint8_t *)&READ_SECTOR_BUFFER;

	if ((data_length < MAX_CHUNK_SIZE) &&
	    (!(flash_conf.host_cntrl_conf & CMD_READ)) &&
	    (!(flash_conf.host_cntrl_conf & CMD_PARTIAL_ERASE))) {
		fill_size = MAX_CHUNK_SIZE - data_length;
		start_fill_addr = (uint32_t)(input_data_ptr + data_length);
		/*
		 * If input file is allowed to have non-conforming lengths
		 * then we need to set the destination to 0xFF
		 */
		/* memset((void *)start_fill_addr, 0xFF, fill_size); */
	}

	if ((flash_conf.host_cntrl_conf & CMD_ERASE) == 0) {
		if (input_data_ptr == 0) {
			spi_err_flag |= MBX_ERR_INVALID_DATA_START;
			trace1(0, MAIN, 0,
			       " MBX_ERR_INVALID_DATA_START: 0x%08X",
			       spi_err_flag);
			exit_extended_mode();
			return spi_err_flag;
		}

		if (data_length == 0) {
			spi_err_flag |= MBX_ERR_INVALID_DATA_LENGTH;
			trace1(0, MAIN, 0,
			       " MBX_ERR_INVALID_DATA_LENGTH: 0x%08X",
			       spi_err_flag);
			exit_extended_mode();
			return spi_err_flag;
		}
	}

	/* Initialization stage, only need to do this once */
	if (!spi_initialization) {
		spi_initialization = 1;
		trace0(0, MAIN, 0, " Init_Signals");
		Init_Signals();
		trace0(0, MAIN, 0, " QSPI_Init");
		QSPI_Init();
		trace0(0, MAIN, 0, " Reset_DMA0");
		Reset_DMA0();

		if (!one_shot_event) {
			trace0(0, MAIN, 0, " QSPI_RESET");
			QSPI_RESET();
			timer_delay_1ms(2);
			one_shot_event = 1;
		}
	}
	/* should only read the device ID if flag is set and EXTERNAL SPI */
	if ((spi_util_cmd & CMD_READ_ID) &&
	    ((spi_util_cmd & SPI_INT_EXT_FLAG) == 0)) {
		trace0(0, MAIN, 0, " QSPI_ReadID");

		ret = QSPI_ReadID();
		if (ret != NO_ERROR) {
			spi_err_flag |= UNSUPPORTED_FLASH_DEV_ERR;
			trace1(0, MAIN, 0, " UNSUPPORTED_FLASH_DEV_ERR: 0x%08X",
			       spi_err_flag);
			exit_extended_mode();
			return spi_err_flag;
		}
	}

	if (flash_conf.clear_spi_status) {
		QSPI_WriteStatus();
		QSPI_ReadStatus();
	} else
		QSPI_ReadStatus();

	if (flash_conf.host_cntrl_conf & CMD_PARTIAL_ERASE) {
		trace0(0, MAIN, 0, " CMD_PARTIAL_ERASE");

		/* Read & confirm 256K bytes (64 * 4096 = 256K) */
		for (sector_address = start_sector_addr;
		     sector_address < start_sector_addr + data_length;) {
			trace1(0, MAIN, 0, "sector address: 0x%08X",
			       sector_address);
			ret = check_32MB_address(sector_address);
			if (ret != NO_ERROR) {
				spi_err_flag = ret | ADDRESS_MODE_ERROR;
				trace1(0, MAIN, 0,
				       " ADDRESS_MODE_ERROR at 0x%08X",
				       sector_address);
				exit_extended_mode();
				return spi_err_flag;
			}

			trace1(0, MAIN, 0, " QSPI_EraseSector: 0x%08X",
			       sector_address);
			ret = QSPI_EraseSector(sector_address);

			if (ret != NO_ERROR) {
				spi_err_flag = ret | MBX_ERASE_ERR;
				trace1(0, MAIN, 0, " MBX_ERASE_ERR at 0x%08X",
				       sector_address);
				exit_extended_mode();
				return spi_err_flag;
			}

			sector_address += SECTOR_SIZE;
		}
	} else if (flash_conf.host_cntrl_conf & CMD_ERASE) {
		trace0(0, MAIN, 0, "\n Full Flash device ERASE");
		ret = QSPI_EraseSector(FULL_CHIP_ERASE_FLAG);
		if (ret != NO_ERROR) {
			spi_err_flag = ret | MBX_ERASE_ERR;
			trace0(0, MAIN, 0, " MBX_ERASE_ERR");
			exit_extended_mode();
			return spi_err_flag;
		}
	} else {
		/*
		 * Program or Read/Verify.
		 * Program flash device, but first see what portion of device
		 * needs to be modified (to save erase/programming time)
		 */
		if ((flash_conf.host_cntrl_conf & CMD_PROG) ||
		    (flash_conf.host_cntrl_conf & CMD_PROG_ONLY)) {
			trace0(0, MAIN, 0, " ");

			if (flash_conf.host_cntrl_conf & CMD_PROG_ONLY) {
				trace0(0, MAIN, 0,
				       "***PROGRAM(NO VERIFY) SPI FLASH***");
			} else {
				trace0(0, MAIN, 0,
				       "***PROGRAM/VERIFY SPI FLASH***");
			}
			erase_program();
			if (spi_err_flag) {
				exit_extended_mode();
				return spi_err_flag;
			}
		} else if (flash_conf.host_cntrl_conf & CMD_READ) {
			if (cmd_disp_en_qspi) {
				cmd_disp_en_qspi = 0;
				trace0(0, MAIN, 0, " ");
				trace0(0, MAIN, 0, "***READ SPI FLASH***");
			}
			input_data_offset = 0;
			/* Read the data directly into the interface SRAM buffer
			 */
			flash_conf.ReadDataPtr = (uint8_t *)&DATA_IO_BUFFER;
			flash_conf.ReadDataPtr += mem_offset_ptr;
			/* Erase all of SRAM data section */
			/*
			 * memset(flash_conf.ReadDataPtr, 0, MAX_CHUNK_SIZE -
			 * mem_offset_ptr);
			 */
			/* Read 4096 bytes at a time */
			for (sector_address = start_sector_addr;
			     sector_address <
			     start_sector_addr + data_length;) {
				ret = check_32MB_address(sector_address);
				if (ret != NO_ERROR) {
					spi_err_flag = ret | ADDRESS_MODE_ERROR;
					trace1(0, MAIN, 0,
					       "   ADDRESS_MODE_ERROR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return spi_err_flag;
				}

				trace1(0, MAIN, 0, "   QSPI_DMA_Read at 0x%08X",
				       sector_address);
				/* Read 4096 sector */
				ret = QSPI_DMA_Read(sector_address,
						    SECTOR_SIZE);
				if (ret != NO_ERROR) {
					spi_err_flag = ret | MBX_DMA_READ_ERR;
					trace1(0, MAIN, 0,
					       "   MBX_DMA_READ_ERR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return spi_err_flag;
				}
				flash_conf.ReadDataPtr += SECTOR_SIZE;
				sector_address += SECTOR_SIZE;
				input_data_offset += SECTOR_SIZE;
			}
		}
		/*
		 * At this point the device has been checked for this CHUNK.
		 * now do a read/verify of the flash device contents for either
		 * a full PROGRAMMING command or the READ/VERIFY command.
		 */
		if ((flash_conf.host_cntrl_conf & CMD_PROG) ||
		    (flash_conf.host_cntrl_conf & CMD_VERIFY)) {
			/* This section does a READ SECTOR and checks data
			 * integrity VERIFY for the entire CHUNK.
			 */
			trace0(0, MAIN, 0, "   Read Flash and Verify...");
			input_data_offset = 0;
			flash_conf.ReadDataPtr = (uint8_t *)&READ_SECTOR_BUFFER;
			for (sector_address = start_sector_addr;
			     sector_address <
			     start_sector_addr + data_length;) {
				ret = check_32MB_address(sector_address);
				if (ret != NO_ERROR) {
					spi_err_flag = ret | ADDRESS_MODE_ERROR;
					trace1(0, MAIN, 0,
					       "   ADDRESS_MODE_ERROR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return spi_err_flag;
				}

				trace1(0, MAIN, 0, "   QSPI_DMA_Read at 0x%08X",
				       sector_address);
				ret = QSPI_DMA_Read(sector_address,
						    SECTOR_SIZE);

				if (ret != NO_ERROR) {
					spi_err_flag = ret | MBX_DMA_READ_ERR;
					trace1(0, MAIN, 0,
					       "   MBX_DMA_READ_ERR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return spi_err_flag;
				}
				DwnloadedDataPtr =
					(uint8_t *)(input_data_ptr +
						    input_data_offset);

				for (i = 0; i < SECTOR_SIZE; i++) {
					if (flash_conf.ReadDataPtr[i] !=
					    DwnloadedDataPtr[i]) {
						spi_err_flag |=
							FLASH_COMPARE_ERR;
						trace1(0, MAIN, 0,
						       "   FLASH_COMPARE_ERR"
						       " at 0x%08X",
						       sector_address + i);
						exit_extended_mode();
						return spi_err_flag;
					}
				}
				sector_address += SECTOR_SIZE;
				input_data_offset += SECTOR_SIZE;
			}
		}
	}

	exit_extended_mode();
	return spi_err_flag;
}

static void erase_program(void)
{
	uint32_t input_data_offset = 0;
	uint32_t sector_address = 0;
	uint8_t program_this_sector = 0;
	uint32_t ret;

	spi_err_flag = 0u;
	/*
	 * Erase and program 256K bytes (64 * 4096 = 256K).
	 * Points to 1st byte of input data.
	 * NOTE: The data in the .bin file would HAVE TO START
	 * AT 0x0 in the file, even it is all 0xFF.
	 */
	for (sector_address = flash_start_addr;
	     sector_address < flash_start_addr + flash_data_len;) {
		trace1(0, MAIN, 0, "   sector address: 0x%08X", sector_address);

		ret = check_32MB_address(sector_address);
		if (ret != NO_ERROR) {
			spi_err_flag = ret | ADDRESS_MODE_ERROR;
			trace1(0, MAIN, 0, "   ADDRESS_MODE_ERROR at 0x%08X",
			       sector_address);
			return;
		}

		if (flash_conf.host_cntrl_conf & CMD_PROG_ONLY) {
			program_this_sector = 1;
		} else {
			erase_sector(sector_address, input_data_offset,
				     &program_this_sector);
			if (spi_err_flag)
				return;
		}

		if (program_this_sector) {
			/*
			 * Data read from the device was different (even only
			 * one bit) than the input data.
			 */
			program_this_sector = 0;
			/* Now program this sector in 256 byte pages */
			trace1(0, MAIN, 0, "   QSPI_DMA_Write : 0x%08X",
			       sector_address);
			program_sector(sector_address, input_data_offset);
			if (spi_err_flag)
				return;
		}

		sector_address += SECTOR_SIZE;
		input_data_offset += SECTOR_SIZE;
	}
}

static void program_sector(uint32_t sector_address, uint32_t input_data_offset)
{
	uint8_t *DwnloadedDataPtr;
	uint8_t *input_data_ptr;
	uint32_t page_address = 0;
	uint32_t ret;

	spi_err_flag = 0u;
	input_data_ptr = (uint8_t *)&DATA_IO_BUFFER;
	/* program 4K sector worth of data in 256 byte chunks (16*256=4096) */
	for (page_address = 0; page_address < SECTOR_SIZE;) {
		DwnloadedDataPtr =
			(uint8_t *)(input_data_ptr + input_data_offset +
				    page_address);
		/* W25Q128FV can only be programmed in 256 byte pages */
		ret = QSPI_DMA_Write(sector_address + page_address,
				     DwnloadedDataPtr, PAGE_SIZE);
		if (ret != NO_ERROR) {
			spi_err_flag = ret | MBX_DMA_WRITE_ERR;
			trace1(0, MAIN, 0, "   MBX_DMA_WRITE_ERR at 0x%08X",
			       sector_address + page_address);
			return;
		}

		page_address += PAGE_SIZE;
	}
}

static void erase_sector(uint32_t sector_address, uint32_t ip_offset,
			 uint8_t *pgm)
{
	uint8_t *DwnloadedDataPtr;
	uint8_t *input_data_ptr = 0;
	uint32_t ret;
	uint32_t i;

	spi_err_flag = 0u;
	input_data_ptr = (uint8_t *)&DATA_IO_BUFFER;
	/* READ a SECTOR from device */
	trace1(0, MAIN, 0, "   QSPI_DMA_Read sector address: 0x%08X",
	       sector_address);
	/* Read 4096 sector */
	ret = QSPI_DMA_Read(sector_address, SECTOR_SIZE);

	if (ret != NO_ERROR) {
		spi_err_flag = ret | MBX_DMA_READ_ERR;
		trace1(0, MAIN, 0, "   MBX_DMA_READ_ERR at 0x%08X",
		       sector_address);
		return;
	}
	/*
	 * Address in range NOTE: input data comprises this entire SECTOR.
	 * Check the entire SECTOR, if data read from DEVICE is same as
	 * INPUT data.
	 */
	DwnloadedDataPtr = (uint8_t *)(input_data_ptr + ip_offset);
	/* Clear flag to program the current sector */
	*pgm = 0;
	trace0(0, MAIN, 0,
	       "   Compare current flash data to programming data...");
	for (i = 0; i < SECTOR_SIZE; i++) {
		/*
		 * Compare the sector of data we just read from the flash to the
		 * new input data to see if there are any differences.
		 */
		if (flash_conf.ReadDataPtr[i] != DwnloadedDataPtr[i]) {
			/*
			 * If even one byte of data does not compare, then erase
			 * this entire Sector and Program.
			 */
			trace0(0, MAIN, 0,
			       "   Sector data different,"
			       " erase & program new data");
			trace1(0, MAIN, 0, "   QSPI_EraseSector : 0x%08X",
			       sector_address);

			ret = QSPI_EraseSector(sector_address);
			if (ret != NO_ERROR) {
				spi_err_flag = ret | MBX_ERASE_ERR;
				trace1(0, MAIN, 0, "   MBX_ERASE_ERR at 0x%08X",
				       sector_address);
				return;
			}
			*pgm = 1;
			break;
		}
	}
}

static void exit_extended_mode(void)
{
	if (flash_conf.current_flash_addr_mode == ENABLE_4B) {
		trace0(0, MAIN, 0, "   Exit 4-byte mode");
		/* Don't leave device in extended mode state */
		flash_4byte_mode(EXIT_4BYTE);
	}
}

static void QMSPI_Mode_Init(void)
{
	if (spi_util_cmd & SPI_INT_EXT_FLAG) { /* Internal SPI flash */
		QMSPI_INST->QMSPI_MODE = CLK_DIV << 16 | QMSPI_ACTIVATE;
	} else { /* External flash */
		QMSPI_INST->QMSPI_MODE = CLK_DIV << 16 |
					 (flash_conf.qmspi_cs << 12) |
					 (4 << 8) | QMSPI_ACTIVATE;
	}
}

static void ClearQMSPI_Status(void)
{
	QMSPI_INST->QMSPI_MODE = QMSPI_RESET;
	/* Clear status (including TRANSFER_COMPLETE bit 0 ) */
	QMSPI_INST->QMSPI_STATUS = (uint16_t)0xFFFF;
	/* Clear Tx/Rx FIFO buffers */
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_CLR_DATA_BUFF;
	QMSPI_Mode_Init();
}

static uint32_t QSPI_EraseSector(uint32_t addr)
{
	/*
	 * Erase a sector(4k bytes) of flash device starting at address
	 * provided.
	 */
	uint32_t ret = 0;
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;

	ClearQMSPI_Status();
	Write_Enable();

	QMSPI_INST->QMSPI_STATUS = (uint16_t)0xFFFF;
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_CLR_DATA_BUFF;

	if (addr == FULL_CHIP_ERASE_FLAG) {
		QMSPI_INST->QMSPI_CTRL = (1 << 17) |
					 QMSPI_TRANSFER_LEN_IN_BYTES |
					 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
		if (flash_conf.flash_type & MICRON_DEVICE)
			*TX_FIFO = (uint8_t)ERASE_CHIP_MICRON;
		else
			*TX_FIFO = (uint8_t)ERASE_CHIP;
	} else {
		if (flash_conf.current_flash_addr_mode == ENABLE_4B) {
			QMSPI_INST->QMSPI_CTRL =
				(5 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
			*TX_FIFO = (uint8_t)ERASE_SECTOR_4B;
			*TX_FIFO = (uint8_t)((addr >> 24) & 0xFF);
			*TX_FIFO = (uint8_t)((addr >> 16) & 0xFF);
			*TX_FIFO = (uint8_t)((addr >> 8) & 0xFF);
			*TX_FIFO = (uint8_t)(addr & 0xFF);
		} else {
			QMSPI_INST->QMSPI_CTRL =
				(4 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
			*TX_FIFO = (uint8_t)ERASE_SECTOR;
			*TX_FIFO = (uint8_t)((addr >> 16) & 0xFF);
			*TX_FIFO = (uint8_t)((addr >> 8) & 0xFF);
			*TX_FIFO = (uint8_t)(addr & 0xFF);
		}
	}

	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return ret;
	manual_toggle(TOGGLE_DEASSERT);

	ClearQMSPI_Status();

	/*
	 * Flash device can take up to 200 sec for a full chip erase,
	 * adjust the timeout accordingly.
	 */
	ret = QSPI_WaitForNotBusy(QTIMEOUT_200SEC);
	return ret;
}

static uint32_t QSPI_DMA_Write(uint32_t flash_addr, uint8_t *data_src,
			       uint32_t length)
{
	/*
	 * Program a sector(256 bytes) of flash device starting at address
	 * provided
	 */
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;
	uint32_t dma_done = 0;

	ClearQMSPI_Status();
	Write_Enable();
	ClearQMSPI_Status();

	QMSPI_INST->QMSPI_CTRL = QMSPI_DESCR_BUFF_EN;

	if (flash_conf.current_flash_addr_mode == ENABLE_4B) {
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_0 =
			(5 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
			QMSPI_DESCR_BUFF1 | QMSPI_TX_EN;
		*TX_FIFO = PAGE_PROGRAM_4B;
		*TX_FIFO = (flash_addr >> 24) & 0xFF;
		*TX_FIFO = (flash_addr >> 16) & 0xFF;
		*TX_FIFO = (flash_addr >> 8) & 0xFF;
		*TX_FIFO = flash_addr & 0xFF;
	} else {
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_0 =
			(4 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
			QMSPI_DESCR_BUFF1 | QMSPI_TX_EN;
		*TX_FIFO = PAGE_PROGRAM;
		*TX_FIFO = (flash_addr >> 16) & 0xFF;
		*TX_FIFO = (flash_addr >> 8) & 0xFF;
		*TX_FIFO = flash_addr & 0xFF;
	}

	QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_1 =
		(length << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
		QMSPI_DESCR_BUFF_LAST | QMSPI_CLOSE_XFER_EN |
		QMSPI_TX_DMA_4BYTE | QMSPI_TX_EN;

	DMA_MAIN_INST->DMA_MAIN_CONTROL = 0x02; /* Reset */
	DMA_MAIN_INST->DMA_MAIN_CONTROL = 0x01;
	DMA_CHAN00_INST->DMA_CHANNEL_ACTIVATE = 0x01;
	DMA_CHAN00_INST->CONTROL = 0x00011500 | (DMA_XFER_4BYTE << 20);
	DMA_CHAN00_INST->DEVICE_ADDRESS = QMSPI_INST_BASE + 0x20;
	DMA_CHAN00_INST->MEMORY_START_ADDRESS = (uint32_t)data_src;
	DMA_CHAN00_INST->MEMORY_END_ADDRESS = (uint32_t)(data_src) + length;

	DMA_CHAN00_INST->CONTROL = 0x00011501 | (DMA_XFER_4BYTE << 20);
	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	while (1) {
		dma_done = DMA_CHAN00_INST->CONTROL;
		if (dma_done & 0x00000004)
			break;
	}

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return ret;
	manual_toggle(TOGGLE_DEASSERT);

	ret = QSPI_WaitForNotBusy(TIMEOUT_870MS);
	if (ret != NO_ERROR)
		return ret;

	DMA_CHAN00_INST->CONTROL = 0;
	return NO_ERROR;
}

static uint32_t QSPI_DMA_Read(uint32_t addr, uint32_t length)
{
	/* Read flash device starting at address provided */
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;
	uint32_t dma_done = 0;
	uint8_t dumy_cnt = 0;
	uint8_t cmd_cnt = 4;

	if (flash_conf.current_flash_addr_mode == ENABLE_4B)
		cmd_cnt = 5;

	ClearQMSPI_Status();

	ret = QSPI_WaitForNotBusy(TIMEOUT_870MS);
	if (ret != NO_ERROR) {
		return ret;
	}

	QMSPI_INST->QMSPI_CTRL = QMSPI_DESCR_BUFF_EN;

	dumy_cnt = 1;

	QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_0 =
		((cmd_cnt + dumy_cnt) << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
		QMSPI_DESCR_BUFF1 | QMSPI_TX_EN;

	if (!flash_conf.spi_quad_mode) {
		*TX_FIFO = FAST_READ;
	} else {
		*TX_FIFO = FAST_READ_QUAD_OUTPUT;
	}
	if (flash_conf.current_flash_addr_mode == ENABLE_4B)
		*TX_FIFO = (addr >> 24) & 0xFF;
	*TX_FIFO = (addr >> 16) & 0xFF;
	*TX_FIFO = (addr >> 8) & 0xFF;
	*TX_FIFO = addr & 0xFF;
	if (dumy_cnt == 1)
		*TX_FIFO = QMSPI_DUMY_COMMAND;

	if (!flash_conf.spi_quad_mode) {
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_1 =
			(length << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
			QMSPI_DESCR_BUFF_LAST | QMSPI_CLOSE_XFER_EN |
			QMSPI_TX_EN_0MODE | QMSPI_RX_EN | QMSPI_RX_DMA_4BYTE;
	} else {
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_1 =
			(length << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
			QMSPI_DESCR_BUFF_LAST | QMSPI_CLOSE_XFER_EN |
			QMSPI_RX_EN | QMSPI_RX_DMA_4BYTE | QMSPI_QUAD_MODE;
	}

	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	DMA_MAIN_INST->DMA_MAIN_CONTROL = 0x02;
	DMA_MAIN_INST->DMA_MAIN_CONTROL = 0x01;
	DMA_CHAN00_INST->DMA_CHANNEL_ACTIVATE = 0x01;
	DMA_CHAN00_INST->CONTROL = 0x00011600 | (DMA_XFER_4BYTE << 20);
	DMA_CHAN00_INST->DEVICE_ADDRESS = QMSPI_INST_BASE + 0x24;

	DMA_CHAN00_INST->MEMORY_START_ADDRESS =
		(uint32_t)flash_conf.ReadDataPtr;
	DMA_CHAN00_INST->MEMORY_END_ADDRESS =
		(uint32_t)flash_conf.ReadDataPtr + length;
	DMA_CHAN00_INST->CONTROL = 0x00011601 | (DMA_XFER_4BYTE << 20);

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR) {
		trace0(0, MAIN, 0,
		       "   QSPI_DMA_Read PollforQMSPI_Status Error");
		return ret;
	}
	while (1) {
		dma_done = DMA_CHAN00_INST->CONTROL;
		if (dma_done & 0x00000004)
			break;
	}

	DMA_CHAN00_INST->CONTROL = 0;
	manual_toggle(TOGGLE_DEASSERT);
	return NO_ERROR;
}

static uint32_t QSPI_RESET(void)
{
	/* Issue reset enable and SPI reset command */
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;

	ClearQMSPI_Status();
	/* Set WREN as required by flash device */
	QMSPI_INST->QMSPI_CTRL = (1 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
	*TX_FIFO = (uint8_t)RSTEN;
	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return (ret);
	manual_toggle(TOGGLE_DEASSERT);

	ClearQMSPI_Status();

	/* Set WREN as required by flash device */
	QMSPI_INST->QMSPI_CTRL = (1 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
	*TX_FIFO = (uint8_t)SPI_RST;
	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return (ret);
	manual_toggle(TOGGLE_DEASSERT);
	ClearQMSPI_Status();

	return NO_ERROR;
}

static uint32_t QSPI_WaitForNotBusy(uint32_t extended_timeout)
{
	uint32_t cnt = HANDSHAKE_TIMEOUT_LONG * extended_timeout;
	uint8_t data = 0;
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	volatile uint8_t *RX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_RECEIVE_BUFFER;
	uint32_t ret = SPI_WAIT_BUSY_TIMEOUT_ERR;

	while (cnt--) {
		QMSPI_Mode_Init();
		QMSPI_INST->QMSPI_CTRL = QMSPI_DESCR_BUFF_EN;
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_0 =
			(1 << 17) | QMSPI_DESCR_BUFF1 |
			QMSPI_TRANSFER_LEN_IN_BYTES | QMSPI_TX_EN;
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_1 =
			(1 << 17) | QMSPI_DESCR_LAST |
			QMSPI_TRANSFER_LEN_IN_BYTES | QMSPI_CLOSE_XFER_EN |
			QMSPI_TX_EN_0MODE | QMSPI_RX_EN;
		*TX_FIFO = (uint8_t)READ_STATUS;
		QMSPI_INST->QMSPI_BUFFER_COUNT_TRIGGER = (1 << 16);

		manual_toggle(TOGGLE_ASSERT);
		QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

		PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
		data = *RX_FIFO;
		manual_toggle(TOGGLE_DEASSERT);

		if ((data & STAT_BUSY_BIT) == 0) {
			ret = NO_ERROR;
			break;
		}
	}

	return (ret);
}

static uint32_t PollforQMSPI_Status(uint32_t status_val)
{
	uint32_t cnt = HANDSHAKE_TIMEOUT_LONG;
	uint32_t result = 0;

	while (cnt--) {
		result = QMSPI_INST->QMSPI_STATUS;
		if (result & status_val) {
			QMSPI_INST->QMSPI_STATUS = status_val;
			return NO_ERROR;
		}
	}

	manual_toggle(TOGGLE_DEASSERT);

	return POLL_STATUS_TO_ERR;
}

static void QSPI_Init(void)
{
	QMSPI_INST->QMSPI_MODE = QMSPI_RESET;
	ClearQMSPI_Status();
}

static void Reset_DMA0(void)
{
	DMA_MAIN_INST->DMA_MAIN_CONTROL_b.SOFT_RESET = 1;
	DMA_MAIN_INST->DMA_MAIN_CONTROL = 0;
	DMA_CHAN00_INST->INT_STATUS = 0x07;
}

static void QSPI_GPIO_Init(void)
{
	trace0(0, MAIN, 0, " QSPI_GPIO_Init with DEFAULT settings");

	/* set the internal SPI signals tri-state */
	/* INT_SPI_MOSI */
	gpio_pin_ctrl1_reg_write(074, 0x0040);
	/* INT_SPI_MISO */
	gpio_pin_ctrl1_reg_write(075, 0x0040);
	/* INT_SPI_nCS */
	gpio_pin_ctrl1_reg_write(0116, 0x0040);
	/* INT_SPI_SCLK */
	gpio_pin_ctrl1_reg_write(0117, 0x0040);
	/* INT_SPI_IO2 */
	gpio_pin_ctrl1_reg_write(076, 0x0040);
	/* INT_SPI_IO3 */
	gpio_pin_ctrl1_reg_write(034, 0x0040);

	/* Set the GPIOs associated with SHD flash if to default POR value */
	/* SHD_nCS */
	gpio_pin_ctrl1_reg_write(055, 0x2000);
	/* SHD_CLK */
	gpio_pin_ctrl1_reg_write(056, 0x2000);
	/* SHD_IO3 (nHOLD) */
	gpio_pin_ctrl1_reg_write(016, 0x2000);
	/* SHD_IO0 (MOSI) */
	gpio_pin_ctrl1_reg_write(0223, 0x1000);
	/* SHD_IO1 (MISO) */
	gpio_pin_ctrl1_reg_write(0224, 0x2000);
	/* SHD_IO2 (nWP) */
	gpio_pin_ctrl1_reg_write(0227, 0x1000);

	/* SHD_nCS 12mA */
	gpio_pin_ctrl2_reg_write(055, 0);
	/* SHD_CLK */
	gpio_pin_ctrl2_reg_write(056, 0);
	/* SHD_IO3 (nHOLD) */
	gpio_pin_ctrl2_reg_write(016, 0);
	/* SHD_IO0 (MOSI) */
	gpio_pin_ctrl2_reg_write(0223, 0);
	/* SHD_IO1 (MISO) */
	gpio_pin_ctrl2_reg_write(0224, 0);
	/* SHD_IO2 (nWP) */
	gpio_pin_ctrl2_reg_write(0227, 0);
}

static void Default_Internal_SPI_Settings(void)
{
	/*
	 * Set the GPIOs associated with PVT and SHD flash if to default POR
	 * value
	 */
	/* PVT_nCS */
	gpio_pin_ctrl1_reg_write(0124, 0x0040);
	/* PVT_CLK */
	gpio_pin_ctrl1_reg_write(0125, 0x0040);
	/* PVT_IO3 (nHOLD) */
	gpio_pin_ctrl1_reg_write(0126, 0x0040);
	/* PVT_IO0 (MOSI) */
	gpio_pin_ctrl1_reg_write(0121, 0x0040);
	/* PVT_IO1 (MISO) */
	gpio_pin_ctrl1_reg_write(0122, 0x0040);
	/* PVT_IO2 (nWP) */
	gpio_pin_ctrl1_reg_write(0123, 0x0040);
	/* SHD_nCS */
	gpio_pin_ctrl1_reg_write(055, 0x0040);
	/* SHD_CLK */
	gpio_pin_ctrl1_reg_write(056, 0x0040);
	/* SHD_IO3 (nHOLD) */
	gpio_pin_ctrl1_reg_write(016, 0x0040);
	/* SHD_IO0 (MOSI) */
	gpio_pin_ctrl1_reg_write(0223, 0x0040);
	/* SHD_IO1 (MISO) */
	gpio_pin_ctrl1_reg_write(0224, 0x0040);
	/* SHD_IO2 (nWP) */
	gpio_pin_ctrl1_reg_write(0227, 0x0040);

	/* INT_SPI_MOSI */
	gpio_pin_ctrl1_reg_write(074, 0x1000);
	/* INT_SPI_MISO */
	gpio_pin_ctrl1_reg_write(075, 0x1000);
	/* INT_SPI_nCS */
	gpio_pin_ctrl1_reg_write(0116, 0x1000);
	/* INT_SPI_SCLK */
	gpio_pin_ctrl1_reg_write(0117, 0x1000);
	/* INT_SPI_IO2 */
	gpio_pin_ctrl1_reg_write(076, 0x1000);
	/* INT_SPI_IO3 */
	gpio_pin_ctrl1_reg_write(034, 0x3000);
}

static void Init_Signals(void)
{
	uint32_t *GPIO_addr_reg = 0;
	uint32_t GPIO_data_val = 0;
	uint32_t i;

	trace1(0, MAIN, 0, " Init_Signals() value = 0x%X", spi_util_cmd);
	flash_conf.qmspi_cs = 0;
	if (spi_util_cmd & CS_FLAG) {
		flash_conf.qmspi_cs = 1;
	}
	trace1(0, MAIN, 0, " flash_conf.qmspi_cs = %d", flash_conf.qmspi_cs);

	if (spi_util_cmd & V1P8_FLAG) { /* 1.8V */
		EC_REG_BANK_INST->GPIO_BANK_PWR |=
			(1 << EC_REG_BANK_INST_GPIO_BANK_PWR_VTR_LVL2_Pos);
		trace0(0, MAIN, 0, " 1.8v SPI i/f selected");
	} else { /* 3.3V */
		EC_REG_BANK_INST->GPIO_BANK_PWR &=
			~(1 << EC_REG_BANK_INST_GPIO_BANK_PWR_VTR_LVL2_Pos);
		trace0(0, MAIN, 0, " 3.3v SPI i/f selected");
	}

	flash_conf.clear_spi_status = 0;
	if (spi_util_cmd & WSR_FLAG) {
		flash_conf.clear_spi_status = 1;
		trace0(0, MAIN, 0, " Write/Clear SPI device Status register");
	}

	flash_conf.spi_quad_mode = 0;
	if (spi_util_cmd & QUAD_FLAG) {
		flash_conf.spi_quad_mode = 1;
		trace0(0, MAIN, 0, " Quad mode selected");
	} else {
		trace0(0, MAIN, 0, " Non-Quad mode selected");
	}

	flash_conf.cs_toggle = 0;
	if (spi_util_cmd & TOGGLE_CS_FLAG) {
		flash_conf.cs_toggle = 1;
		trace0(0, MAIN, 0, " Toggle CS manually");
	}

	if ((spi_util_cmd & SPI_INT_EXT_FLAG) &&
	    ((spi_util_cmd & USE_GPIO_INI_PARAMS_FLAG) == 0)) {
		trace0(0, MAIN, 0,
		       " Initialize default GPIOs for Internal SPI flash i/f");
		Default_Internal_SPI_Settings();
	}

	if (spi_util_cmd & USE_GPIO_INI_PARAMS_FLAG) {
		trace0(0, MAIN, 0,
		       " Init_Signals from spi_util_cmd parameters");
		spi_util_cmd &= ~USE_GPIO_INI_PARAMS_FLAG;

		for (i = 0; i < GPIO_INI_SIZE; i++) {
			GPIO_addr_reg = (uint32_t *)(gpio_addr[i]);
			GPIO_data_val = (gpio_setting[i]);
			trace1(0, MAIN, 0, " Init_Signals() value = 0x%X",
			       GPIO_addr_reg);
			if (i == 0) {
				flash_conf.chip_sel_addr = GPIO_addr_reg;
				trace1(0, MAIN, 0,
				       " CHIP Select address = 0x%X",
				       flash_conf.chip_sel_addr);
			}

			if ((int)GPIO_addr_reg >= 0x40080000) {
				trace1(0, MAIN, 0, " GPIO_PIN_REG = 0x%08X",
				       GPIO_addr_reg);
				*GPIO_addr_reg = GPIO_data_val;
				trace1(0, MAIN, 0, "   Data = 0x%08X",
				       GPIO_data_val);
			}
		}

	}

	else { /* No GPIOs specified, use default settings instead */
		if ((spi_util_cmd & SPI_INT_EXT_FLAG) == 0) {
			trace0(0, MAIN, 0,
			       " Initialize default GPIOs for "
			       "External SHD (nCS=0) SPI flash i/f");
			QSPI_GPIO_Init();
		}
	}
}

static uint32_t QSPI_ReadID(void)
{
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	volatile uint8_t *RX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_RECEIVE_BUFFER;
	uint8_t rxptr = 0;
	uint8_t i, rd_cnt;
	uint32_t ret = 0;
	uint8_t spi_cmd[30];

	ClearQMSPI_Status();

	ret = QSPI_WaitForNotBusy(TIMEOUT_870MS);
	if (ret != NO_ERROR) {
		return ret;
	}
	QMSPI_INST->QMSPI_CTRL = QMSPI_DESCR_BUFF_EN;

	*TX_FIFO = (uint8_t)JEDEC_ID;
	QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_0 =
		(1 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES | QMSPI_DESCR_BUFF1 |
		QMSPI_TX_EN;

	QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_1 =
		(3 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
		QMSPI_DESCR_BUFF_LAST | QMSPI_CLOSE_XFER_EN |
		QMSPI_TX_EN_0MODE | QMSPI_RX_EN | QMSPI_SINGLE_MODE;
	rd_cnt = 3;
	QMSPI_INST->QMSPI_BUFFER_COUNT_TRIGGER = (1 << 16);

	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	rxptr = 0;
	while (1) {
		ret = PollforQMSPI_Status(QMSPI_RX_BUFF_REQ);
		if (ret != NO_ERROR) {
			return ret;
		}
		for (i = 0; i < rd_cnt; i++) {
			spi_cmd[rxptr++] = *RX_FIFO;
			trace2(0, MAIN, 0, " ID[%d] : 0x%02X", i, spi_cmd[i]);
		}
		if (rxptr == rd_cnt)
			break;
	}

	manual_toggle(TOGGLE_DEASSERT);

	ClearQMSPI_Status();
	if (ret != NO_ERROR)
		return (ret);

	flash_conf.flash_type = DEFAULT_DEVICE;

	if (spi_cmd[0] == WINBOND_ID) {
		flash_conf.flash_type |= WINBOND_DEVICE;
		trace1(0, MAIN, 0, " WINBOND_DEVICE found: 0x%02X", WINBOND_ID);
	} else if (spi_cmd[0] == MACRONIX_ID) {
		flash_conf.flash_type |= MACRONIX_DEVICE;
		trace1(0, MAIN, 0, " MACRONIX_DEVICE found: 0x%02X",
		       MACRONIX_ID);
	} else if (spi_cmd[0] == MICRON_ID) {
		flash_conf.flash_type |= MICRON_DEVICE;
		trace1(0, MAIN, 0, " MICRON_DEVICE found: 0x%02X",
		       MICRON_DEVICE);
	} else if (spi_cmd[0] == MICROCHIP_ID) {
		flash_conf.flash_type |= MCHP_DEVICE;
		flash_conf.flash_type |= DEV_16MB_TYPE;
		trace1(0, MAIN, 0, " MICROCHIP_DEVICE found: 0x%02X",
		       MCHP_DEVICE);
	} else {
		trace1(0, MAIN, 0, " Unidentified device found: 0x%02X",
		       spi_cmd[0]);
		return UNSUPPORTED_FLASH_DEV_ERR;
	}

	if ((flash_conf.flash_type & DEVICE_MASK) != MCHP_DEVICE) {
		trace1(0, MAIN, 0, " Device size parameter: 0x%02X",
		       spi_cmd[2]);

		if (spi_cmd[2] == DEV_16MB) {
			flash_conf.flash_type |= DEV_16MB_TYPE;
			trace0(0, MAIN, 0, " 16 MB flash");
		} else if (spi_cmd[2] >= DEV_32MB) {
			flash_conf.flash_type |= DEV_32MB_TYPE;
			trace0(0, MAIN, 0,
			       " 32 MB flash (or greater) "
			       "supports 4-byte addressing");
		}
	}
	ClearQMSPI_Status();
	return NO_ERROR;
}

static uint32_t flash_4byte_mode(uint8_t mode)
{
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;

	/* No 32MB check needed for Internal SPI flash */
	if (spi_util_cmd & SPI_INT_EXT_FLAG)
		return (NO_ERROR);

	if ((mode == ENTER_4BYTE) &&
	    (flash_conf.current_flash_addr_mode == ENABLE_4B))
		return NO_ERROR;
	if ((mode == EXIT_4BYTE) &&
	    (flash_conf.current_flash_addr_mode == DISABLE_4B))
		return NO_ERROR;

	trace1(0, MAIN, 0, " flash_4byte_mode %X", mode);

	ClearQMSPI_Status();

	QMSPI_INST->QMSPI_CTRL = (1 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
	*TX_FIFO = (uint8_t)mode;

	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return (ret);

	manual_toggle(TOGGLE_DEASSERT);
	ClearQMSPI_Status();

	if (mode == ENTER_4BYTE)
		flash_conf.current_flash_addr_mode = ENABLE_4B;
	else
		flash_conf.current_flash_addr_mode = DISABLE_4B;

	return NO_ERROR;
}

static uint32_t check_32MB_address(uint32_t address)
{
	/*
	 * Check the address + length, if > 16MB set the extended addressing
	 * (4 byte) mode
	 */
	uint32_t ret = NO_ERROR;

	/* No 32MB check needed for Internal SPI flash */
	if (spi_util_cmd & SPI_INT_EXT_FLAG)
		return (ret);

	if (flash_conf.flash_type & DEV_16MB_TYPE) {
		if (address >= ADDR_16MB)
			return ADDRESS_MODE_ERROR;
		else
			return (ret);
	}

	if (flash_conf.flash_type & DEV_32MB_TYPE) {
		if (address >= ADDR_16MB)
			ret = flash_4byte_mode(ENTER_4BYTE);
		else
			ret = flash_4byte_mode(EXIT_4BYTE);
	}
	return (ret);
}

static uint32_t QSPI_ReadStatus(void)
{
	uint32_t ret = NO_ERROR;
	uint8_t data = 0;
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	volatile uint8_t *RX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_RECEIVE_BUFFER;

	QMSPI_Mode_Init();

	QMSPI_INST->QMSPI_CTRL = QMSPI_DESCR_BUFF_EN;
	QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_0 = (1 << 17) | QMSPI_DESCR_BUFF1 |
						 QMSPI_TRANSFER_LEN_IN_BYTES |
						 QMSPI_TX_EN;
	QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_1 =
		(1 << 17) | QMSPI_DESCR_LAST | QMSPI_TRANSFER_LEN_IN_BYTES |
		QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN_0MODE | QMSPI_RX_EN;
	*TX_FIFO = (uint8_t)READ_STATUS;
	QMSPI_INST->QMSPI_BUFFER_COUNT_TRIGGER = (1 << 16);

	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	data = *RX_FIFO;
	manual_toggle(TOGGLE_DEASSERT);
	if (data) {
		trace1(0, MAIN, 0, "     QSPI_ReadStatus: 0x%02X", data);
	}
	return (ret);
}

static uint32_t QSPI_WriteStatus(void)
{
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;

	ClearQMSPI_Status();

	QMSPI_INST->QMSPI_CTRL = (1 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
	*TX_FIFO = (uint8_t)WREN_CMD;
	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return (ret);
	manual_toggle(TOGGLE_DEASSERT);

	ClearQMSPI_Status();
	trace0(0, MAIN, 0, " QSPI_WriteStatus Register");

	QMSPI_INST->QMSPI_CTRL = (2 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
	*TX_FIFO = WRSR_CMD;
	*TX_FIFO = 0x0;
	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return (ret);
	manual_toggle(TOGGLE_DEASSERT);

	ClearQMSPI_Status();
	return ret;
}

static void manual_toggle(uint32_t state)
{
	/* If QSPI block only has GPIO for CS signal, must be toggled manually
	 */
	if (flash_conf.cs_toggle == 0)
		return;

	else { /* Toggle CS manually */
		if (state) {
			/* Set GPIO value = 1 */
			*flash_conf.chip_sel_addr = (0x10200);
		} else {
			/* Clear GPIO value = 0 */
			*flash_conf.chip_sel_addr = (0x00200);
		}
	}
}

static uint32_t Write_Enable(void)
{
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	volatile uint8_t *RX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_RECEIVE_BUFFER;
	uint32_t ret = NO_ERROR;
	uint8_t done = 0;
	uint8_t data, first_time = 0;
	uint32_t cnt = HANDSHAKE_TIMEOUT;

	ClearQMSPI_Status();
	ret = QSPI_WaitForNotBusy(TIMEOUT_870MS);
	if (ret != NO_ERROR)
		return (ret);

	QMSPI_Mode_Init();

	while (!done) {
		QMSPI_INST->QMSPI_CTRL = (1 << 17) |
					 QMSPI_TRANSFER_LEN_IN_BYTES |
					 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
		*TX_FIFO = (uint8_t)WREN_CMD;
		manual_toggle(TOGGLE_ASSERT);
		QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;
		ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
		if (ret != NO_ERROR)
			return (ret);
		manual_toggle(TOGGLE_DEASSERT);

		cnt = HANDSHAKE_TIMEOUT;
		while (cnt--)
			__NOP();

		if (first_time == 0) {
			first_time++;
			QMSPI_INST->QMSPI_CTRL =
				(1 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
			*TX_FIFO = (uint8_t)GLOBAL_UNLOCK_CMD;
			manual_toggle(TOGGLE_ASSERT);
			QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;
			ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
			if (ret != NO_ERROR)
				return (ret);
			manual_toggle(TOGGLE_DEASSERT);

			cnt = HANDSHAKE_TIMEOUT;
			while (cnt--)
				__NOP();
		}

		QMSPI_INST->QMSPI_CTRL = QMSPI_DESCR_BUFF_EN;
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_0 =
			(1 << 17) | QMSPI_DESCR_BUFF1 |
			QMSPI_TRANSFER_LEN_IN_BYTES | QMSPI_TX_EN;
		QMSPI_INST->QMSPI_DESCRIPTION_BUFFER_1 =
			(1 << 17) | QMSPI_DESCR_LAST |
			QMSPI_TRANSFER_LEN_IN_BYTES | QMSPI_CLOSE_XFER_EN |
			QMSPI_TX_EN_0MODE | QMSPI_RX_EN;
		*TX_FIFO = (uint8_t)READ_STATUS;
		QMSPI_INST->QMSPI_BUFFER_COUNT_TRIGGER = (1 << 16);

		manual_toggle(TOGGLE_ASSERT);
		QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

		PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
		data = *RX_FIFO;
		manual_toggle(TOGGLE_DEASSERT);

		if (data & WEL_BIT) {
			done = 1;
		}
	}

	return (ret);
}
