/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MEC1727 SOC spi flash update tool
 */

#include "MCHP_MEC172x.h"
#include "common.h"
#include "trace.h"

#include <stdlib.h>

#define GPIO_INI_SIZE 27
#define SUCCESSFUL_COMPLETION 1
#define ERROR_COMPLETION 0xFF
#define READ_CNT_COMPLETION 0xFE
#define BLINK_RATE_IDLE 500
#define BLINK_RATE_PASS 1000
#define BLINK_RATE_FAIL 100

#define NO_ERROR 1
#define PAGE_SIZE 256
#define SECTOR_SIZE 4096

#define MAX_CHUNK_SIZE (256 * 1024)
#define FULL_CHIP_ERASE_FLAG 0xFFFFFFFF
#define ADDR_16MB 0x1000000
#define HANDSHAKE_TIMEOUT_LONG 1000000
#define HANDSHAKE_TIMEOUT 100000
#define TIMEOUT_870MS 1
/* Hex value Divider for 47 */
#define TIMER_PRE_SCL_DIV_48 0x002FUL
#define HOST_ACK_SIG_REV 0x33CC
#define EC_ACK_BYTE1 0x3C
#define EC_ACK_BYTE2 0xC3

/* 4K byte buffer from 0xCA400 - 0xCB400 */
#define READ_SECTOR_BUFFER (*((volatile uint32_t *)0xCA400))
/* 256KB buffer from 0xCE000 - 0x10E000 */
#define DATA_IO_BUFFER (*((volatile uint32_t *)0xCE000))

#define WREN_CMD 0x06
#define WRSR_CMD 0x01
#define ERASE_SECTOR 0x20
#define ERASE_SECTOR_4B 0x21
#define ERASE_CHIP 0xC7
#define ERASE_CHIP_MICRON 0xC4
#define FAST_READ 0x0B
#define FAST_READ_4B 0x0C
#define READ_STATUS 0x05
#define PAGE_PROGRAM 0x02
#define PAGE_PROGRAM_4B 0x12
#define RSTEN 0x66
#define SPI_RST 0x99
#define FAST_READ_QUAD_OUTPUT 0x6B
#define FAST_READ_QUAD_OUTPUT_4B 0x6C
#define FLASH_DEVICE_ID 0x90
#define JEDEC_ID 0x9F
#define ENTER_4BYTE 0xB7
#define EXIT_4BYTE 0xE9
#define EXTENDED_ADDR 0xC5
#define GLOBAL_UNLOCK_CMD 0x98

#define FLASH_COMPARE_ERR (1 << 0)
#define ERASE_BUSY_ERR (1 << 1)
#define WRITE_BUSY_ERR (1 << 2)
#define READ_BUSY_ERR (1 << 3)
#define SPISR_TXBE_DMA_WR_ERR (1 << 4)
#define SPISR_TXBE_DMA_RD_ERR (1 << 5)
#define DMA_WR_NOT_DONE_ERR (1 << 6)
#define BOARD_INIT_ERR (1 << 7)
#define SPI_WRITE_TIMEOUT_ERR (1 << 8)
#define SPI_WRITE_PTR_ERR (1 << 9)
#define SPI_READ_PTR_ERR (1 << 10)
#define SPI_READ_TIMEOUT_ERR (1 << 11)
#define SPI_WAIT_BUSY_TIMEOUT_ERR (1 << 12)
#define DMA_RD_NOT_DONE_ERR (1 << 14)

#define MBX_ERR_INVALID_DATA_LENGTH (1 << 15)
#define MBX_ERR_INVALID_DATA_START (1 << 16)

#define MBX_DMA_WRITE_ERR (1 << 17)
#define MBX_DMA_READ_ERR (1 << 18)
#define MBX_ERASE_ERR (1 << 19)

#define POLL_STATUS_TO_ERR (1 << 20)
#define INVALID_INTERFACE_SELECT_ERR (1 << 21)
#define CHECK_STATUS_INFO (1 << 22)
#define UNSUPPORTED_FLASH_DEV_ERR (1 << 23)
#define ADDRESS_MODE_ERROR (1 << 24)
#define INVALID_CMD_REC_ERROR (1 << 25)

#define SPI_SHD_PVT_FLAG (1 << 0)
#define QUAD_FLAG (1 << 1)
#define CS_FLAG (1 << 2)
/* 1 = Int. Flash, 0 = Ext. flash = 0 */
#define SPI_INT_EXT_FLAG (1 << 3)
#define WSR_FLAG (1 << 4)
#define TOGGLE_CS_FLAG (1 << 5)
/* 1=1.8v, 0=3.3v */
#define V1P8_FLAG (1 << 6)
#define USE_GPIO_INI_PARAMS_FLAG (1 << 7)

#define CMD_PROG (1 << 8)
#define CMD_ERASE (1 << 9)
#define CMD_VERIFY (1 << 10)
#define CMD_READ (1 << 11)
#define CMD_PARTIAL_ERASE (1 << 12)
#define CMD_PROG_ONLY (1 << 13)

/* Read device ID (External SPI devices only) */
#define CMD_READ_ID (1 << 14)
#define CMD_HANDSHAKE_START (1 << 15)
#define STATUS_ERR_BIT (1 << 16)

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
#define STAT_TX_NOT_FULL (1 << 8)
#define MICRON_ID 0x20
#define WINBOND_ID 0xEF
#define MACRONIX_ID 0xC2
#define MICROCHIP_ID 0xBF
#define DEV_16MB 0x18
#define DEV_32MB 0x19
#define DEFAULT_DEVICE 0

#define DEV_16MB_TYPE (1 << 0)
#define DEV_32MB_TYPE (1 << 1)
#define DEVICE_MASK (0xFC)
#define WINBOND_DEVICE (1 << 2)
#define MACRONIX_DEVICE (1 << 4)
#define MICRON_DEVICE (1 << 5)
#define MCHP_DEVICE (1 << 6)

#define EN_EXT_ADDR (1)
#define DIS_EXT_ADDR (0)
#define EXTENDED_ENTRY (1 << 0)
#define EXTENDED_EXIT (1 << 1)
#define ENABLE_4B (1 << 2)
#define DISABLE_4B (1 << 3)

#define TOGGLE_ASSERT 0
#define TOGGLE_DEASSERT 1
#define PROCESSOR_CLOCK48MHZ 2

#define NUM_CMDS 6
#define CMD_ACK1 0x33
#define CMD_ACK2 0xCC
#define CMD_HDR_FILE 0x65
#define CMD_RD_FILE 0x66
#define CMD_PRG_FILE 0x67
#define CMD_GET_RD_CNT 0x68

uint8_t CS_TOGGLE;
uint8_t FLASH_TYPE = DEFAULT_DEVICE;
uint8_t QMSPI_CS;
uint8_t SPI_QUAD_MODE;
uint8_t CLEAR_SPI_STATUS;
uint8_t ONE_SHOT_EVENT;
uint8_t CURRENT_FLASH_ADDRESS_MODE;
uint8_t PTR_OFFSET_FLAG;
uint8_t SPI_INITIALIZATION;
uint8_t CMD_DISP_EN = 1;
uint8_t CMD_DISP_EN_QSPI = 1;
uint8_t spi_cmd[30] = { 0 };
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
uint8_t *ReadDataPtr;
uint8_t *PgmDataPtr;
volatile uint8_t T_DONE;

uint32_t *CHIP_SELECT_ADDR;
uint32_t CLKDIV = 1;
uint32_t HOST_CNTRL_CONFIGURATION;
/* 0x4D434850 ('MCHP') */
uint32_t HDR_FLAG;
uint32_t SPI_UTIL_CMD;
uint32_t FLSH_START_ADDR;
uint32_t FLSH_START;
/*
 * Preserve (do not reset at 256k boundary) offset pointer
 * when reading flash memory
 */
uint32_t READ_FLSH_OFFSET_PTR;
uint32_t FLSH_DATA_LENGTH;
uint32_t FLSH_DATA_LENGTH_TOTAL;
uint32_t GPIO_ADDR[GPIO_INI_SIZE];
uint32_t GPIO_SETTING[GPIO_INI_SIZE];
/* 0x58454F46 ('XEOF') */
uint32_t TERMINATOR;
uint32_t TRANSFER_CNT;
uint32_t GPIO_CNT;
uint32_t TOTAL_XFER_PROG_COUNT;
uint32_t SPI_ERROR_FLAG;
uint32_t SPI_ERROR_ADDRESS;

typedef uint32_t crc32_t;
volatile uint32_t command_crc32;

volatile uint32_t TIMER0_TIMEOUT;
volatile uint32_t ISR_COUNT;

uint8_t process_cmd(uint8_t command);
uint8_t isValidCmd(uint8_t cmd);
uint8_t extract_header_info(void);
uint8_t program_max_chunk(void);
uint8_t process_pgm_file(uint32_t ptr_offset, uint32_t xfer_cnt);

uint32_t QSPI_ReadStatus(void);
uint32_t QSPI_WriteStatus(void);
uint32_t QSPI_RESET(void);
uint32_t PollforQMSPI_Status(uint32_t stat);
uint32_t QSPI_EraseSector(uint32_t addr);
uint32_t QSPI_WaitForNotBusy(uint32_t extended_timeout);
uint32_t QSPI_DMA_Write(uint32_t addr, uint8_t *data_src, uint32_t length);
uint32_t QSPI_DMA_Read(uint32_t addr, uint32_t length);
uint32_t flash_4byte_mode(uint8_t mode);
uint32_t extended_address_mode(uint8_t mode);
uint32_t check_32MB_address(uint32_t data_length);
uint32_t board_init(void);
uint32_t SPI_Operations(uint32_t mem_offset_ptr);
uint32_t Write_Enable(void);
uint32_t QSPI_ReadID(void);

void ClearQMSPI_Status(void);
void manual_toggle(uint32_t state);
void QSPI_Init(void);
void QSPI_GPIO_Init(void);
void Init_Signals(void);
void Reset_DMA0(void);
void error_send(void);
void timer_init(void);
void timer_delay_1ms(uint32_t msec);
void processRxdData(uint8_t rxData);
void exit_extended_mode(void);
void program_sector(uint32_t sector_address, uint32_t input_data_offset);
void erase_sector(uint32_t sector_address, uint32_t ip_offset, uint8_t *pgm);
void erase_program(void);

static crc32_t crc32_init(void);
static crc32_t crc32_finalize(crc32_t crc);
crc32_t crc32_update(crc32_t crc, const unsigned char *data, size_t data_len);

extern void SER_init(void);
extern int send_host_char(int c);
extern void rom_prog_32kosc(void);

static const crc32_t crc32_table[256] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/*
 * External Windows HOST program loads the resulting binary of building this
 * project into MEC172x SRAM. Host also loads binary data to be programmed to
 * an external Flash device via the QMSPI interface. Once loaded the HOST
 * sets-up the ARMCore parameters in order to have this program execute.
 */
int main(void)
{
	uint32_t ret = NO_ERROR;
	uint32_t i;

	volatile uint32_t *GPIO_Control_register = 0;
	uint32_t blink_rate = 0;

	while (WDT_INST->WDT_CONTROL_b.WDT_ENABLE == 1) {
		WDT_INST->WDT_CONTROL_b.WDT_ENABLE = 0;
	}

	/* Configure for JTAG: enable JTAG/SWD, SWD on TCK/TMS */
	EC_REG_BANK_INST->DEBUG_Enable = 5;
	SPI_ERROR_FLAG = (uint32_t)0;
	SPI_ERROR_ADDRESS = (uint32_t)0;

	SPI_ERROR_FLAG = BOARD_INIT_ERR;
	ret = board_init();
	if (ret == NO_ERROR) {
		SPI_ERROR_FLAG = (uint32_t)0;
	}

	/* Initialize GPIOs as input 0x40081000UL - 0x4008107C */
	GPIO_Control_register = (uint32_t *)GPIO_000_036_INST_BASE;
	for (i = 0; i < 0x7C; i += 4) {
		*(GPIO_Control_register) = 0x8040;
		GPIO_Control_register += 1;
	}

	/* 0x40081080UL-0x400810F8 */
	GPIO_Control_register = (uint32_t *)GPIO_040_073_INST_BASE;
	for (i = 0; i < 0x7C; i += 4) {
		/* GPIO062 */
		if (GPIO_Control_register != (uint32_t *)0x400810C8) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register += 1;
	}

	/* 0x40081100UL-0x4008117C */
	GPIO_Control_register = (uint32_t *)GPIO_100_137_INST_BASE;
	for (i = 0; i < 0x78; i += 4) {
		/* GPIO116 */
		if (GPIO_Control_register != (uint32_t *)0x40081138) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register += 1;
	}

	/* 0x40081180UL-0x400811F4 */
	GPIO_Control_register = (uint32_t *)GPIO_140_176_INST_BASE;
	for (i = 0; i < 0x78; i += 4) {
		/*
		 * Check GPIO163, GPIO164, GPIO167, GPIO173,
		 * GPIO174, GPIO176, GPIO177
		 */
		if ((GPIO_Control_register != (uint32_t *)0x400811CC) &&
		    (GPIO_Control_register != (uint32_t *)0x400811D0) &&
		    (GPIO_Control_register != (uint32_t *)0x400811DC) &&
		    (GPIO_Control_register != (uint32_t *)0x400811EC) &&
		    (GPIO_Control_register != (uint32_t *)0x400811F0) &&
		    (GPIO_Control_register != (uint32_t *)0x400811F8) &&
		    (GPIO_Control_register != (uint32_t *)0x400811FC)) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register += 1;
	}

	/* 0x40081200UL-0x40081278 */
	GPIO_Control_register = (uint32_t *)GPIO_200_234_INST_BASE;
	for (i = 0; i < 0x70; i += 4) {
		/* Check GPIO220 and GPIO232 */
		if ((GPIO_Control_register != (uint32_t *)0x40081240) &&
		    (GPIO_Control_register != (uint32_t *)0x40081268)) {
			*(GPIO_Control_register) = 0x8040;
		}
		GPIO_Control_register += 1;
	}

	/* Read the data directly into the interface SRAM buffer */
	ReadDataPtr = (uint8_t *)&DATA_IO_BUFFER;
	/* Erase all of SRAM data section */
	/* memset(ReadDataPtr, 0xFF, MAX_CHUNK_SIZE);*/

	SER_init(); /* Output thru UART 115200, 8Bit,NP,1SB */
	trace0(0, MAIN, 0, " ");
	trace0(0, MAIN, 0,
	       " ------------------------------------------------------");
	trace0(0, MAIN, 0,
	       " MEC172x Crisis Recovery Flash Utility Firmware v1.00.1");
	trace0(0, MAIN, 0, " Copyright (c) 2021 Microchip Technology Inc");
	trace0(0, MAIN, 0, " F/W running on MEC172x");

	/* GPIO157 configure (LED3) */
	GPIO_140_176_INST->GPIO_157_PIN_CONTROL = 0;
	GPIO_140_176_INST->GPIO_157_PIN_CONTROL_b.ALT_GPIO_DATA = 1;
	GPIO_140_176_INST->GPIO_157_PIN_CONTROL_b.GPIO_DIRECTION = 1;
	GPIO_140_176_INST->GPIO_157_PIN_CONTROL_b.ALT_GPIO_DATA = 0;
	/* GPIO153 configure (LED2) */
	GPIO_140_176_INST->GPIO_153_PIN_CONTROL = 0;
	GPIO_140_176_INST->GPIO_153_PIN_CONTROL_b.ALT_GPIO_DATA = 1;
	GPIO_140_176_INST->GPIO_153_PIN_CONTROL_b.GPIO_DIRECTION = 1;

	TRANSFER_CNT = 0;

	blink_rate = BLINK_RATE_IDLE;
	if (SPI_ERROR_FLAG) {
		blink_rate = BLINK_RATE_FAIL;
		trace1(0, MAIN, 0, "!!! ERROR STATUS: 0x%08X !!!",
		       SPI_ERROR_FLAG);
	}

	/* Blink LEDs while waiting for communication from HOST interface */
	while (!HOST_IF_UART->LINE_STS_b.DATA_READY) {
		timer_delay_1ms(blink_rate);
		GPIO_140_176_INST->GPIO_157_PIN_CONTROL_b.ALT_GPIO_DATA ^= 1;
		timer_delay_1ms(blink_rate);
		GPIO_140_176_INST->GPIO_153_PIN_CONTROL_b.ALT_GPIO_DATA ^= 1;
	}

	while (1) {
		if (HOST_IF_UART->LINE_STS & UART0_STS_DATA_RDY_Msk) {
			processRxdData(HOST_IF_UART->RX_DATA);
		}
	}
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

void processRxdData(uint8_t rxData)
{
	uint8_t i;
	uint8_t completion_status = 0;
	uint32_t blink_rate = 0;
	static uint32_t test;

	if (!isValidCmd(rxData)) {
		return;
	}

	/* Got first byte of ACK packet */
	if (rxData == CMD_ACK1) {
		test = rxData << 8;
		return;
	}

	/* Got 2nd byte of ACK packet */
	if (rxData == CMD_ACK2) {
		test += rxData;
		if ((uint16_t)(HOST_ACK_SIG_REV) == (uint16_t)test) {
			trace0(491, MAIN, 0, "\n Received Host-to-EC ACK");
			send_host_char(EC_ACK_BYTE1);
			send_host_char(EC_ACK_BYTE2);
			trace0(491, MAIN, 0, " Sent EC-to-Host ACK");
			CMD_DISP_EN_QSPI = 1;
			SPI_INITIALIZATION = 0;
		}
		test = 0;
		return;
	}

	completion_status = process_cmd(rxData);
	if ((completion_status == 0) ||
	    (completion_status == READ_CNT_COMPLETION)) {
		return;
	}

	for (i = 0; i < 5; i++) {
		send_host_char(tx_buff[i]);
	}

	if (completion_status == SUCCESSFUL_COMPLETION)
		blink_rate = BLINK_RATE_PASS;
	else
		blink_rate = BLINK_RATE_FAIL;
	/* GPIO157 => toggle (LED3) */
	GPIO_140_176_INST->GPIO_157_PIN_CONTROL_b.ALT_GPIO_DATA = 1;
	/* GPIO153 => toggle (LED2) */
	GPIO_140_176_INST->GPIO_153_PIN_CONTROL_b.ALT_GPIO_DATA = 1;

	while (!HOST_IF_UART->LINE_STS_b.DATA_READY) {
		timer_delay_1ms(blink_rate);
		GPIO_140_176_INST->GPIO_157_PIN_CONTROL_b.ALT_GPIO_DATA ^= 1;
		GPIO_140_176_INST->GPIO_153_PIN_CONTROL_b.ALT_GPIO_DATA ^= 1;
	}
}

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

	HOST_CNTRL_CONFIGURATION = SPI_UTIL_CMD;
	if ((HOST_CNTRL_CONFIGURATION & 0x7F00) == 0) {
		ret = INVALID_CMD_REC_ERROR;
		return ret;
	}

	if (SPI_ERROR_FLAG == BOARD_INIT_ERR) {
		trace1(0, MAIN, 0, " BOARD_INIT_ERR: 0x%08X", SPI_ERROR_FLAG);
		exit_extended_mode();
		return SPI_ERROR_FLAG;
	}

	input_data_ptr = (uint8_t *)&DATA_IO_BUFFER;
	data_length = FLSH_DATA_LENGTH;
	start_sector_addr = FLSH_START_ADDR;
	ReadDataPtr = (uint8_t *)&READ_SECTOR_BUFFER;

	if ((data_length < MAX_CHUNK_SIZE) &&
	    (!(HOST_CNTRL_CONFIGURATION & CMD_READ)) &&
	    (!(HOST_CNTRL_CONFIGURATION & CMD_PARTIAL_ERASE))) {
		fill_size = MAX_CHUNK_SIZE - data_length;
		start_fill_addr = (uint32_t)(input_data_ptr + data_length);
		/*
		 * If input file is allowed to have non-conforming lengths
		 * then we need to set the destination to 0xFF
		 */
		/* memset((void *)start_fill_addr, 0xFF, fill_size); */
	}

	if ((HOST_CNTRL_CONFIGURATION & CMD_ERASE) == 0) {
		if (input_data_ptr == 0) {
			SPI_ERROR_FLAG |= MBX_ERR_INVALID_DATA_START;
			trace1(0, MAIN, 0,
			       " MBX_ERR_INVALID_DATA_START: 0x%08X",
			       SPI_ERROR_FLAG);
			exit_extended_mode();
			return SPI_ERROR_FLAG;
		}

		if (data_length == 0) {
			SPI_ERROR_FLAG |= MBX_ERR_INVALID_DATA_LENGTH;
			trace1(0, MAIN, 0,
			       " MBX_ERR_INVALID_DATA_LENGTH: 0x%08X",
			       SPI_ERROR_FLAG);
			exit_extended_mode();
			return SPI_ERROR_FLAG;
		}
	}

	/* Initialization stage, only need to do this once */
	if (!SPI_INITIALIZATION) {
		SPI_INITIALIZATION = 1;
		trace0(0, MAIN, 0, " Init_Signals");
		Init_Signals();
		trace0(0, MAIN, 0, " QSPI_Init");
		QSPI_Init();
		trace0(0, MAIN, 0, " Reset_DMA0");
		Reset_DMA0();

		if (!ONE_SHOT_EVENT) {
			trace0(0, MAIN, 0, " QSPI_RESET");
			QSPI_RESET();
			timer_delay_1ms(2);
			ONE_SHOT_EVENT = 1;
		}
	}
	/* should only read the device ID if flag is set and EXTERNAL SPI */
	if ((SPI_UTIL_CMD & CMD_READ_ID) &&
	    ((SPI_UTIL_CMD & SPI_INT_EXT_FLAG) == 0)) {
		trace0(0, MAIN, 0, " QSPI_ReadID");

		ret = QSPI_ReadID();
		if (ret != NO_ERROR) {
			SPI_ERROR_FLAG |= UNSUPPORTED_FLASH_DEV_ERR;
			trace1(0, MAIN, 0, " UNSUPPORTED_FLASH_DEV_ERR: 0x%08X",
			       SPI_ERROR_FLAG);
			exit_extended_mode();
			return SPI_ERROR_FLAG;
		}
	}

	if (CLEAR_SPI_STATUS) {
		QSPI_WriteStatus();
		QSPI_ReadStatus();
	} else
		QSPI_ReadStatus();

	if (HOST_CNTRL_CONFIGURATION & CMD_PARTIAL_ERASE) {
		trace0(0, MAIN, 0, " CMD_PARTIAL_ERASE");

		/* Read & confirm 256K bytes (64 * 4096 = 256K) */
		for (sector_address = start_sector_addr;
		     sector_address < start_sector_addr + data_length;) {
			trace1(0, MAIN, 0, "sector address: 0x%08X",
			       sector_address);
			ret = check_32MB_address(sector_address);
			if (ret != NO_ERROR) {
				SPI_ERROR_FLAG = ret | ADDRESS_MODE_ERROR;
				SPI_ERROR_ADDRESS = sector_address;
				trace1(0, MAIN, 0,
				       " ADDRESS_MODE_ERROR at 0x%08X",
				       sector_address);
				exit_extended_mode();
				return SPI_ERROR_FLAG;
			}

			trace1(0, MAIN, 0, " QSPI_EraseSector: 0x%08X",
			       sector_address);
			ret = QSPI_EraseSector(sector_address);

			if (ret != NO_ERROR) {
				SPI_ERROR_FLAG = ret | MBX_ERASE_ERR;
				SPI_ERROR_ADDRESS = sector_address;
				trace1(0, MAIN, 0, " MBX_ERASE_ERR at 0x%08X",
				       sector_address);
				exit_extended_mode();
				return SPI_ERROR_FLAG;
			}

			sector_address += SECTOR_SIZE;
		}
	} else if (HOST_CNTRL_CONFIGURATION & CMD_ERASE) {
		trace0(0, MAIN, 0, "\n Full Flash device ERASE");
		ret = QSPI_EraseSector(FULL_CHIP_ERASE_FLAG);
		if (ret != NO_ERROR) {
			SPI_ERROR_FLAG = ret | MBX_ERASE_ERR;
			SPI_ERROR_ADDRESS = FULL_CHIP_ERASE_FLAG;
			trace0(0, MAIN, 0, " MBX_ERASE_ERR");
			exit_extended_mode();
			return SPI_ERROR_FLAG;
		}
	} else {
		/*
		 * Program or Read/Verify.
		 * Program flash device, but first see what portion of device
		 * needs to be modified (to save erase/programming time)
		 */
		if ((HOST_CNTRL_CONFIGURATION & CMD_PROG) ||
		    (HOST_CNTRL_CONFIGURATION & CMD_PROG_ONLY)) {
			trace0(0, MAIN, 0, " ");

			if (HOST_CNTRL_CONFIGURATION & CMD_PROG_ONLY) {
				trace0(0, MAIN, 0,
				       "***PROGRAM(NO VERIFY) SPI FLASH***");
			} else {
				trace0(0, MAIN, 0,
				       "***PROGRAM/VERIFY SPI FLASH***");
			}
			erase_program();
			if (SPI_ERROR_FLAG) {
				exit_extended_mode();
				return SPI_ERROR_FLAG;
			}
		} else if (HOST_CNTRL_CONFIGURATION & CMD_READ) {
			if (CMD_DISP_EN_QSPI) {
				CMD_DISP_EN_QSPI = 0;
				trace0(0, MAIN, 0, " ");
				trace0(0, MAIN, 0, "***READ SPI FLASH***");
			}
			input_data_offset = 0;
			/* Read the data directly into the interface SRAM buffer
			 */
			ReadDataPtr = (uint8_t *)&DATA_IO_BUFFER;
			ReadDataPtr += mem_offset_ptr;
			/* Erase all of SRAM data section */
			/*
			 * memset(ReadDataPtr, 0, MAX_CHUNK_SIZE -
			 * mem_offset_ptr);
			 */
			/* Read 4096 bytes at a time */
			for (sector_address = start_sector_addr;
			     sector_address <
			     start_sector_addr + data_length;) {
				ret = check_32MB_address(sector_address);
				if (ret != NO_ERROR) {
					SPI_ERROR_FLAG = ret |
							 ADDRESS_MODE_ERROR;
					SPI_ERROR_ADDRESS = sector_address;
					trace1(0, MAIN, 0,
					       "   ADDRESS_MODE_ERROR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return SPI_ERROR_FLAG;
				}

				trace1(0, MAIN, 0, "   QSPI_DMA_Read at 0x%08X",
				       sector_address);
				/* Read 4096 sector */
				ret = QSPI_DMA_Read(sector_address,
						    SECTOR_SIZE);
				if (ret != NO_ERROR) {
					SPI_ERROR_FLAG = ret | MBX_DMA_READ_ERR;
					SPI_ERROR_ADDRESS = sector_address;
					trace1(0, MAIN, 0,
					       "   MBX_DMA_READ_ERR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return SPI_ERROR_FLAG;
				}
				ReadDataPtr += SECTOR_SIZE;
				sector_address += SECTOR_SIZE;
				input_data_offset += SECTOR_SIZE;
			}
		}
		/*
		 * At this point the device has been checked for this CHUNK.
		 * now do a read/verify of the flash device contents for either
		 * a full PROGRAMMING command or the READ/VERIFY command.
		 */
		if ((HOST_CNTRL_CONFIGURATION & CMD_PROG) ||
		    (HOST_CNTRL_CONFIGURATION & CMD_VERIFY)) {
			/* This section does a READ SECTOR and checks data
			 * integrity VERIFY for the entire CHUNK.
			 */
			trace0(0, MAIN, 0, "   Read Flash and Verify...");
			input_data_offset = 0;
			ReadDataPtr = (uint8_t *)&READ_SECTOR_BUFFER;
			for (sector_address = start_sector_addr;
			     sector_address <
			     start_sector_addr + data_length;) {
				ret = check_32MB_address(sector_address);
				if (ret != NO_ERROR) {
					SPI_ERROR_FLAG = ret |
							 ADDRESS_MODE_ERROR;
					SPI_ERROR_ADDRESS = sector_address;
					trace1(0, MAIN, 0,
					       "   ADDRESS_MODE_ERROR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return SPI_ERROR_FLAG;
				}

				trace1(0, MAIN, 0, "   QSPI_DMA_Read at 0x%08X",
				       sector_address);
				ret = QSPI_DMA_Read(sector_address,
						    SECTOR_SIZE);

				if (ret != NO_ERROR) {
					SPI_ERROR_FLAG = ret | MBX_DMA_READ_ERR;
					SPI_ERROR_ADDRESS = sector_address;
					trace1(0, MAIN, 0,
					       "   MBX_DMA_READ_ERR at 0x%08X",
					       sector_address);
					exit_extended_mode();
					return SPI_ERROR_FLAG;
				}
				DwnloadedDataPtr =
					(uint8_t *)(input_data_ptr +
						    input_data_offset);

				for (i = 0; i < SECTOR_SIZE; i++) {
					if (ReadDataPtr[i] !=
					    DwnloadedDataPtr[i]) {
						SPI_ERROR_FLAG |=
							FLASH_COMPARE_ERR;
						SPI_ERROR_ADDRESS =
							sector_address + i;
						trace1(0, MAIN, 0,
						       "   FLASH_COMPARE_ERR"
						       " at 0x%08X",
						       sector_address + i);
						exit_extended_mode();
						return SPI_ERROR_FLAG;
					}
				}
				sector_address += SECTOR_SIZE;
				input_data_offset += SECTOR_SIZE;
			}
		}
	}

	exit_extended_mode();
	return SPI_ERROR_FLAG;
}

void erase_program(void)
{
	uint32_t input_data_offset = 0;
	uint32_t sector_address = 0;
	uint8_t program_this_sector = 0;
	uint32_t ret;

	SPI_ERROR_FLAG = (uint32_t)0;
	/*
	 * Erase and program 256K bytes (64 * 4096 = 256K).
	 * Points to 1st byte of input data.
	 * NOTE: The data in the .bin file would HAVE TO START
	 * AT 0x0 in the file, even it is all 0xFF.
	 */
	for (sector_address = FLSH_START_ADDR;
	     sector_address < FLSH_START_ADDR + FLSH_DATA_LENGTH;) {
		trace1(0, MAIN, 0, "   sector address: 0x%08X", sector_address);

		ret = check_32MB_address(sector_address);
		if (ret != NO_ERROR) {
			SPI_ERROR_FLAG = ret | ADDRESS_MODE_ERROR;
			SPI_ERROR_ADDRESS = sector_address;
			trace1(0, MAIN, 0, "   ADDRESS_MODE_ERROR at 0x%08X",
			       sector_address);
			return;
		}

		if (HOST_CNTRL_CONFIGURATION & CMD_PROG_ONLY) {
			program_this_sector = 1;
		} else {
			erase_sector(sector_address, input_data_offset,
				     &program_this_sector);
			if (SPI_ERROR_FLAG)
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
			if (SPI_ERROR_FLAG)
				return;
		}

		sector_address += SECTOR_SIZE;
		input_data_offset += SECTOR_SIZE;
	}
}

void program_sector(uint32_t sector_address, uint32_t input_data_offset)
{
	uint8_t *DwnloadedDataPtr;
	uint8_t *input_data_ptr;
	uint32_t page_address = 0;
	uint32_t ret;

	SPI_ERROR_FLAG = (uint32_t)0;
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
			SPI_ERROR_FLAG = ret | MBX_DMA_WRITE_ERR;
			SPI_ERROR_ADDRESS = sector_address + page_address;
			trace1(0, MAIN, 0, "   MBX_DMA_WRITE_ERR at 0x%08X",
			       sector_address + page_address);
			return;
		}

		page_address += PAGE_SIZE;
	}
}

void erase_sector(uint32_t sector_address, uint32_t ip_offset, uint8_t *pgm)
{
	uint8_t *DwnloadedDataPtr;
	uint8_t *input_data_ptr = 0;
	uint32_t ret;
	uint32_t i;

	SPI_ERROR_FLAG = (uint32_t)0;
	input_data_ptr = (uint8_t *)&DATA_IO_BUFFER;
	/* READ a SECTOR from device */
	trace1(0, MAIN, 0, "   QSPI_DMA_Read sector address: 0x%08X",
	       sector_address);
	/* Read 4096 sector */
	ret = QSPI_DMA_Read(sector_address, SECTOR_SIZE);

	if (ret != NO_ERROR) {
		SPI_ERROR_FLAG = ret | MBX_DMA_READ_ERR;
		SPI_ERROR_ADDRESS = sector_address;
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
		if (ReadDataPtr[i] != DwnloadedDataPtr[i]) {
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
				SPI_ERROR_FLAG = ret | MBX_ERASE_ERR;
				SPI_ERROR_ADDRESS = sector_address;
				trace1(0, MAIN, 0, "   MBX_ERASE_ERR at 0x%08X",
				       sector_address);
				return;
			}
			*pgm = 1;
			break;
		}
	}
}

void exit_extended_mode(void)
{
	if (CURRENT_FLASH_ADDRESS_MODE == ENABLE_4B) {
		trace0(0, MAIN, 0, "   Exit 4-byte mode");
		/* Don't leave device in extended mode state */
		flash_4byte_mode(EXIT_4BYTE);
	}
}

void QMSPI_Mode_Init(void)
{
	if (SPI_UTIL_CMD & SPI_INT_EXT_FLAG) { /* Internal SPI flash */
		QMSPI_INST->QMSPI_MODE = CLKDIV << 16 | QMSPI_ACTIVATE;
	} else { /* External flash */
		QMSPI_INST->QMSPI_MODE = CLKDIV << 16 | (QMSPI_CS << 12) |
					 (4 << 8) | QMSPI_ACTIVATE;
	}
}

void ClearQMSPI_Status(void)
{
	QMSPI_INST->QMSPI_MODE = QMSPI_RESET;
	/* Clear status (including TRANSFER_COMPLETE bit 0 ) */
	QMSPI_INST->QMSPI_STATUS = (uint16_t)0xFFFF;
	/* Clear Tx/Rx FIFO buffers */
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_CLR_DATA_BUFF;
	QMSPI_Mode_Init();
}

uint32_t QSPI_EraseSector(uint32_t addr)
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
		if (FLASH_TYPE & MICRON_DEVICE)
			*TX_FIFO = (uint8_t)ERASE_CHIP_MICRON;
		else
			*TX_FIFO = (uint8_t)ERASE_CHIP;
	} else {
		if (CURRENT_FLASH_ADDRESS_MODE == ENABLE_4B) {
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

uint32_t QSPI_DMA_Write(uint32_t flash_addr, uint8_t *data_src, uint32_t length)
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

	if (CURRENT_FLASH_ADDRESS_MODE == ENABLE_4B) {
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

uint32_t QSPI_DMA_Read(uint32_t addr, uint32_t length)
{
	/* Read flash device starting at address provided */
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;
	uint32_t dma_done = 0;
	uint8_t dumy_cnt = 0;
	uint8_t cmd_cnt = 4;

	if (CURRENT_FLASH_ADDRESS_MODE == ENABLE_4B)
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

	if (!SPI_QUAD_MODE) {
		*TX_FIFO = FAST_READ;
	} else {
		*TX_FIFO = FAST_READ_QUAD_OUTPUT;
	}
	if (CURRENT_FLASH_ADDRESS_MODE == ENABLE_4B)
		*TX_FIFO = (addr >> 24) & 0xFF;
	*TX_FIFO = (addr >> 16) & 0xFF;
	*TX_FIFO = (addr >> 8) & 0xFF;
	*TX_FIFO = addr & 0xFF;
	if (dumy_cnt == 1)
		*TX_FIFO = QMSPI_DUMY_COMMAND;

	if (!SPI_QUAD_MODE) {
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

	DMA_CHAN00_INST->MEMORY_START_ADDRESS = (uint32_t)ReadDataPtr;
	DMA_CHAN00_INST->MEMORY_END_ADDRESS = (uint32_t)ReadDataPtr + length;
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

uint32_t QSPI_RESET(void)
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

uint32_t QSPI_WaitForNotBusy(uint32_t extended_timeout)
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

uint32_t PollforQMSPI_Status(uint32_t status_val)
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

void QSPI_Init(void)
{
	QMSPI_INST->QMSPI_MODE = QMSPI_RESET;
	ClearQMSPI_Status();
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
	TIMER0_TIMEOUT = msec;
	T_DONE = 0;
	ISR_COUNT = 0;

	while (!T_DONE)
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
		ISR_COUNT++;
		if (ISR_COUNT >= TIMER0_TIMEOUT) {
			ISR_COUNT = 0;
			T_DONE = 1;
		}
	}
}

void Reset_DMA0(void)
{
	DMA_MAIN_INST->DMA_MAIN_CONTROL_b.SOFT_RESET = 1;
	DMA_MAIN_INST->DMA_MAIN_CONTROL = 0;
	DMA_CHAN00_INST->INT_STATUS = 0x07;
}

void QSPI_GPIO_Init(void)
{
	trace0(0, MAIN, 0, " QSPI_GPIO_Init with DEFAULT settings");

	/* set the internal SPI signals tri-state */
	/* INT_SPI_MOSI */
	GPIO_040_073_INST->GPIO_074_PIN_CONTROL = 0x0040;
	/* INT_SPI_MISO */
	GPIO_040_073_INST->GPIO_075_PIN_CONTROL = 0x0040;
	/* INT_SPI_nCS */
	GPIO_100_137_INST->GPIO_116_PIN_CONTROL = 0x0040;
	/* INT_SPI_SCLK */
	GPIO_100_137_INST->GPIO_117_PIN_CONTROL = 0x0040;
	/* INT_SPI_IO2 */
	GPIO_040_073_INST->GPIO_076_PIN_CONTROL = 0x0040;
	/* INT_SPI_IO3 */
	GPIO_000_036_INST->GPIO_034_PIN_CONTROL = 0x0040;

	/* Set the GPIOs associated with SHD flash if to default POR value */
	/* SHD_nCS */
	GPIO_040_073_INST->GPIO_055_PIN_CONTROL = 0x2000;
	/* SHD_CLK */
	GPIO_040_073_INST->GPIO_056_PIN_CONTROL = 0x2000;
	/* SHD_IO3 (nHOLD) */
	GPIO_000_036_INST->GPIO_016_PIN_CONTROL = 0x2000;
	/* SHD_IO0 (MOSI) */
	GPIO_200_234_INST->GPIO_223_PIN_CONTROL = 0x1000;
	/* SHD_IO1 (MISO) */
	GPIO_200_234_INST->GPIO_224_PIN_CONTROL = 0x2000;
	/* SHD_IO2 (nWP) */
	GPIO_200_234_INST->GPIO_227_PIN_CONTROL = 0x1000;

	/* SHD_nCS 12mA */
	GPIO_PIN_CONTROL_2_INST->GPIO_055_PIN_CONTROL_2 = 0x0;
	/* SHD_CLK */
	GPIO_PIN_CONTROL_2_INST->GPIO_056_PIN_CONTROL_2 = 0x0;
	/* SHD_IO3 (nHOLD) */
	GPIO_PIN_CONTROL_2_INST->GPIO_016_PIN_CONTROL_2 = 0x0;
	/* SHD_IO0 (MOSI) */
	GPIO_PIN_CONTROL_2_INST->GPIO_223_PIN_CONTROL_2 = 0x0;
	/* SHD_IO1 (MISO) */
	GPIO_PIN_CONTROL_2_INST->GPIO_224_PIN_CONTROL_2 = 0x0;
	/* SHD_IO2 (nWP) */
	GPIO_PIN_CONTROL_2_INST->GPIO_227_PIN_CONTROL_2 = 0x0;
}

void Default_Internal_SPI_Settings(void)
{
	/*
	 * Set the GPIOs associated with PVT and SHD flash if to default POR
	 * value
	 */
	/* PVT_nCS */
	GPIO_100_137_INST->GPIO_124_PIN_CONTROL = 0x0040;
	/* PVT_CLK */
	GPIO_100_137_INST->GPIO_125_PIN_CONTROL = 0x0040;
	/* PVT_IO3 (nHOLD) */
	GPIO_100_137_INST->GPIO_126_PIN_CONTROL = 0x0040;
	/* PVT_IO0 (MOSI) */
	GPIO_100_137_INST->GPIO_121_PIN_CONTROL = 0x0040;
	/* PVT_IO1 (MISO) */
	GPIO_100_137_INST->GPIO_122_PIN_CONTROL = 0x0040;
	/* PVT_IO2 (nWP) */
	GPIO_100_137_INST->GPIO_123_PIN_CONTROL = 0x0040;
	/* SHD_nCS */
	GPIO_040_073_INST->GPIO_055_PIN_CONTROL = 0x0040;
	/* SHD_CLK */
	GPIO_040_073_INST->GPIO_056_PIN_CONTROL = 0x0040;
	/* SHD_IO3 (nHOLD) */
	GPIO_000_036_INST->GPIO_016_PIN_CONTROL = 0x0040;
	/* SHD_IO0 (MOSI) */
	GPIO_200_234_INST->GPIO_223_PIN_CONTROL = 0x0040;
	/* SHD_IO1 (MISO) */
	GPIO_200_234_INST->GPIO_224_PIN_CONTROL = 0x0040;
	/* SHD_IO2 (nWP) */
	GPIO_200_234_INST->GPIO_227_PIN_CONTROL = 0x0040;

	/* INT_SPI_MOSI */
	GPIO_040_073_INST->GPIO_074_PIN_CONTROL = 0x1000;
	/* INT_SPI_MISO */
	GPIO_040_073_INST->GPIO_075_PIN_CONTROL = 0x1000;
	/* INT_SPI_nCS */
	GPIO_100_137_INST->GPIO_116_PIN_CONTROL = 0x1000;
	/* INT_SPI_SCLK */
	GPIO_100_137_INST->GPIO_117_PIN_CONTROL = 0x1000;
	/* INT_SPI_IO2 */
	GPIO_040_073_INST->GPIO_076_PIN_CONTROL = 0x1000;
	/* INT_SPI_IO3 */
	GPIO_000_036_INST->GPIO_034_PIN_CONTROL = 0x3000;
}

void Init_Signals(void)
{
	uint32_t *GPIO_addr_reg = 0;
	uint32_t GPIO_data_val = 0;
	uint32_t i;

	trace1(0, MAIN, 0, " Init_Signals() value = 0x%X", SPI_UTIL_CMD);
	QMSPI_CS = 0;
	if (SPI_UTIL_CMD & CS_FLAG) {
		QMSPI_CS = 1;
	}
	trace1(0, MAIN, 0, " QMSPI_CS = %d", QMSPI_CS);

	if (SPI_UTIL_CMD & V1P8_FLAG) { /* 1.8V */
		EC_REG_BANK_INST->GPIO_BANK_PWR |=
			(1 << EC_REG_BANK_INST_GPIO_BANK_PWR_VTR_LVL2_Pos);
		trace0(0, MAIN, 0, " 1.8v SPI i/f selected");
	} else { /* 3.3V */
		EC_REG_BANK_INST->GPIO_BANK_PWR &=
			~(1 << EC_REG_BANK_INST_GPIO_BANK_PWR_VTR_LVL2_Pos);
		trace0(0, MAIN, 0, " 3.3v SPI i/f selected");
	}

	CLEAR_SPI_STATUS = 0;
	if (SPI_UTIL_CMD & WSR_FLAG) {
		CLEAR_SPI_STATUS = 1;
		trace0(0, MAIN, 0, " Write/Clear SPI device Status register");
	}

	SPI_QUAD_MODE = 0;
	if (SPI_UTIL_CMD & QUAD_FLAG) {
		SPI_QUAD_MODE = 1;
		trace0(0, MAIN, 0, " Quad mode selected");
	} else {
		trace0(0, MAIN, 0, " Non-Quad mode selected");
	}

	CS_TOGGLE = 0;
	if (SPI_UTIL_CMD & TOGGLE_CS_FLAG) {
		CS_TOGGLE = 1;
		trace0(0, MAIN, 0, " Toggle CS manually");
	}

	if ((SPI_UTIL_CMD & SPI_INT_EXT_FLAG) &&
	    ((SPI_UTIL_CMD & USE_GPIO_INI_PARAMS_FLAG) == 0)) {
		trace0(0, MAIN, 0,
		       " Initialize default GPIOs for Internal SPI flash i/f");
		Default_Internal_SPI_Settings();
	}

	if (SPI_UTIL_CMD & USE_GPIO_INI_PARAMS_FLAG) {
		trace0(0, MAIN, 0,
		       " Init_Signals from SPI_UTIL_CMD parameters");
		SPI_UTIL_CMD &= ~USE_GPIO_INI_PARAMS_FLAG;

		for (i = 0; i < GPIO_INI_SIZE; i++) {
			GPIO_addr_reg = (uint32_t *)(GPIO_ADDR[i]);
			GPIO_data_val = (GPIO_SETTING[i]);
			trace1(0, MAIN, 0, " Init_Signals() value = 0x%X",
			       GPIO_addr_reg);
			if (i == 0) {
				CHIP_SELECT_ADDR = GPIO_addr_reg;
				trace1(0, MAIN, 0,
				       " CHIP Select address = 0x%X",
				       CHIP_SELECT_ADDR);
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
		if ((SPI_UTIL_CMD & SPI_INT_EXT_FLAG) == 0) {
			trace0(0, MAIN, 0,
			       " Initialize default GPIOs for "
			       "External SHD (nCS=0) SPI flash i/f");
			QSPI_GPIO_Init();
		}
	}
}

uint32_t QSPI_ReadID(void)
{
	uint32_t ret = 0;
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	volatile uint8_t *RX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_RECEIVE_BUFFER;
	uint8_t rxptr = 0;
	uint8_t i, rd_cnt;

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

	FLASH_TYPE = DEFAULT_DEVICE;

	if (spi_cmd[0] == WINBOND_ID) {
		FLASH_TYPE |= WINBOND_DEVICE;
		trace1(0, MAIN, 0, " WINBOND_DEVICE found: 0x%02X", WINBOND_ID);
	} else if (spi_cmd[0] == MACRONIX_ID) {
		FLASH_TYPE |= MACRONIX_DEVICE;
		trace1(0, MAIN, 0, " MACRONIX_DEVICE found: 0x%02X",
		       MACRONIX_ID);
	} else if (spi_cmd[0] == MICRON_ID) {
		FLASH_TYPE |= MICRON_DEVICE;
		trace1(0, MAIN, 0, " MICRON_DEVICE found: 0x%02X",
		       MICRON_DEVICE);
	} else if (spi_cmd[0] == MICROCHIP_ID) {
		FLASH_TYPE |= MCHP_DEVICE;
		FLASH_TYPE |= DEV_16MB_TYPE;
		trace1(0, MAIN, 0, " MICROCHIP_DEVICE found: 0x%02X",
		       MCHP_DEVICE);
	} else {
		trace1(0, MAIN, 0, " Unidentified device found: 0x%02X",
		       spi_cmd[0]);
		return UNSUPPORTED_FLASH_DEV_ERR;
	}

	if ((FLASH_TYPE & DEVICE_MASK) != MCHP_DEVICE) {
		trace1(0, MAIN, 0, " Device size parameter: 0x%02X",
		       spi_cmd[2]);

		if (spi_cmd[2] == DEV_16MB) {
			FLASH_TYPE |= DEV_16MB_TYPE;
			trace0(0, MAIN, 0, " 16 MB flash");
		} else if (spi_cmd[2] >= DEV_32MB) {
			FLASH_TYPE |= DEV_32MB_TYPE;
			trace0(0, MAIN, 0,
			       " 32 MB flash (or greater) "
			       "supports 4-byte addressing");
		}
	}
	ClearQMSPI_Status();
	return NO_ERROR;
}

uint32_t flash_4byte_mode(uint8_t mode)
{
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;

	/* No 32MB check needed for Internal SPI flash */
	if (SPI_UTIL_CMD & SPI_INT_EXT_FLAG)
		return (NO_ERROR);

	if ((mode == ENTER_4BYTE) && (CURRENT_FLASH_ADDRESS_MODE == ENABLE_4B))
		return NO_ERROR;
	if ((mode == EXIT_4BYTE) && (CURRENT_FLASH_ADDRESS_MODE == DISABLE_4B))
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
		CURRENT_FLASH_ADDRESS_MODE = ENABLE_4B;
	else
		CURRENT_FLASH_ADDRESS_MODE = DISABLE_4B;

	return NO_ERROR;
}

uint32_t extended_address_mode(uint8_t mode)
{
	volatile uint8_t *TX_FIFO =
		(uint8_t *)&QMSPI_INST->QMSPI_TRANSMIT_BUFFER;
	uint32_t ret = 0;

	if ((mode == EN_EXT_ADDR) && (CURRENT_FLASH_ADDRESS_MODE == ENABLE_4B))
		return NO_ERROR;
	if ((mode == DIS_EXT_ADDR) &&
	    (CURRENT_FLASH_ADDRESS_MODE == EXTENDED_EXIT))
		return NO_ERROR;

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

	QMSPI_INST->QMSPI_CTRL = (2 << 17) | QMSPI_TRANSFER_LEN_IN_BYTES |
				 QMSPI_CLOSE_XFER_EN | QMSPI_TX_EN;
	*TX_FIFO = EXTENDED_ADDR;
	*TX_FIFO = mode;
	manual_toggle(TOGGLE_ASSERT);
	QMSPI_INST->QMSPI_EXECUTE = QMSPI_START;

	ret = PollforQMSPI_Status(QMSPI_TRANSFER_COMPLETE);
	if (ret != NO_ERROR)
		return (ret);

	manual_toggle(TOGGLE_DEASSERT);
	ClearQMSPI_Status();

	if (mode == EN_EXT_ADDR)
		CURRENT_FLASH_ADDRESS_MODE = EXTENDED_ENTRY;
	else
		CURRENT_FLASH_ADDRESS_MODE = EXTENDED_EXIT;

	return NO_ERROR;
}

uint32_t check_32MB_address(uint32_t address)
{
	/*
	 * Check the address + length, if > 16MB set the extended addressing
	 * (4 byte) mode
	 */
	uint32_t ret = NO_ERROR;

	/* No 32MB check needed for Internal SPI flash */
	if (SPI_UTIL_CMD & SPI_INT_EXT_FLAG)
		return (ret);

	if (FLASH_TYPE & DEV_16MB_TYPE) {
		if (address >= ADDR_16MB)
			return ADDRESS_MODE_ERROR;
		else
			return (ret);
	}

	if (FLASH_TYPE & DEV_32MB_TYPE) {
		if (address >= ADDR_16MB)
			ret = flash_4byte_mode(ENTER_4BYTE);
		else
			ret = flash_4byte_mode(EXIT_4BYTE);
	}
	return (ret);
}

uint32_t QSPI_ReadStatus(void)
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

uint32_t QSPI_WriteStatus(void)
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

void manual_toggle(uint32_t state)
{
	/* If QSPI block only has GPIO for CS signal, must be toggled manually
	 */
	if (CS_TOGGLE == 0)
		return;

	else { /* Toggle CS manually */
		if (state) {
			/* Set GPIO value = 1 */
			*CHIP_SELECT_ADDR = (0x10200);
		} else {
			/* Clear GPIO value = 0 */
			*CHIP_SELECT_ADDR = (0x00200);
		}
	}
}

uint32_t Write_Enable(void)
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

static crc32_t crc32_init(void)
{
	return 0xffffffff;
}

static crc32_t crc32_finalize(crc32_t crc)
{
	return crc ^ 0xffffffff;
}

crc32_t crc32_update(crc32_t crc, const unsigned char *data, size_t data_len)
{
	unsigned int tbl_idx;

	while (data_len--) {
		tbl_idx = (crc ^ *data) & 0xff;
		crc = (crc32_table[tbl_idx] ^ (crc >> 8ul)) & 0xffffffff;

		data++;
	}
	return crc & 0xffffffff;
}

uint8_t process_cmd(uint8_t command)
{
	uint8_t i, cnt;
	uint8_t data;
	uint32_t host_crc32, resp_crc32;
	uint32_t ii;
	uint32_t xfer_cnt = 0;
	uint8_t comp_stat = 0;
	uint32_t ptr_offset = 0;
	uint32_t ret = 0;

	if (command == CMD_GET_RD_CNT) {
		tx_buff[0] = FLSH_DATA_LENGTH_TOTAL & 0xFF;
		tx_buff[1] = (FLSH_DATA_LENGTH_TOTAL >> 8) & 0xFF;
		tx_buff[2] = (FLSH_DATA_LENGTH_TOTAL >> 16) & 0xFF;
		tx_buff[3] = (FLSH_DATA_LENGTH_TOTAL >> 24) & 0xFF;

		for (i = 0; i < 4; i++)
			send_host_char(tx_buff[i]);
		return READ_CNT_COMPLETION;
	}

	/* Read 4-byte header (length + (3) header offset (LSB rx first)) */
	for (ii = 0; ii < 4; ii++) {
		while ((HOST_IF_UART->LINE_STS & UART0_STS_DATA_RDY_Msk) == 0)
			;
		rx_buff[ii] = HOST_IF_UART->RX_DATA;
	}

	/* Initialize this CRC32 packet then add in the command value */
	command_crc32 = 0;
	command_crc32 = crc32_init();
	command_crc32 = crc32_update(command_crc32, &command, 1);

	if ((command == CMD_PRG_FILE) & CMD_DISP_EN) {
		CMD_DISP_EN = 0;
		trace0(0, MAIN, 0, "\n Program Flash cmd received");
	} else if ((command == CMD_RD_FILE) & CMD_DISP_EN) {
		CMD_DISP_EN = 0;
		trace0(0, MAIN, 0, "\n Read Flash cmd received");
	}

	/* Update command packet CRC with the chunk size & offset bytes */
	command_crc32 = crc32_update(command_crc32, &rx_buff[0], 4);

	/* Parse out the length & offset values */
	xfer_cnt = cnt = rx_buff[0];
	ptr_offset = (rx_buff[3] << 16) | (rx_buff[2] << 8) | rx_buff[1];

	if (command == CMD_RD_FILE)
		READ_FLSH_OFFSET_PTR = ptr_offset;

	if (ptr_offset >= MAX_CHUNK_SIZE) {
		ptr_offset -= MAX_CHUNK_SIZE;
	}

	PgmDataPtr = (uint8_t *)&DATA_IO_BUFFER;
	PgmDataPtr += ptr_offset;

	if ((command == CMD_PRG_FILE) || (command == CMD_HDR_FILE)) {
		while (cnt) {
			while ((HOST_IF_UART->LINE_STS &
				UART0_STS_DATA_RDY_Msk) == 0)
				;
			data = HOST_IF_UART->RX_DATA;
			*PgmDataPtr++ = data;
			command_crc32 = crc32_update(command_crc32, &data, 1);
			cnt--;
		}
	}

	else if (command == CMD_RD_FILE) {
		/*
		 * For CMD_RD we read a 4k sector from flash then we transfer
		 * in 128 byte chunks to HOST.
		 */
		if ((ptr_offset % SECTOR_SIZE) == 0) {
			trace1(0, MAIN, 0, "   Transfer offset: 0x%X ",
			       ptr_offset);

			/* All READs must be on SECTOR boundary */
			FLSH_DATA_LENGTH = SECTOR_SIZE;
			FLSH_START_ADDR = FLSH_START + READ_FLSH_OFFSET_PTR;

			ret = SPI_Operations(ptr_offset);
			if (ret) {
				trace1(0, MAIN, 0,
				       " SPI_Operation ERROR: 0x%08X", ret);
				return ERROR_COMPLETION;
			}
		}
		if (ret == 0) {
			/*
			 * Transfer a chunk of read data to HOST with
			 * checksums.
			 */
			while (cnt) {
				/* Read data byte from SRAM */
				data = *PgmDataPtr++;
				send_host_char(data);
				command_crc32 =
					crc32_update(command_crc32, &data, 1);
				cnt--;
			}
		}

		while (cnt) {
			data = *PgmDataPtr++;
			send_host_char(data);
			command_crc32 = crc32_update(command_crc32, &data, 1);
			cnt--;
		}
	}

	/* Read HOST's CRC32 */
	host_crc32 = 0;
	for (ii = 0; ii < 4; ii++) {
		/* wait for new byte */
		while ((HOST_IF_UART->LINE_STS & UART0_STS_DATA_RDY_Msk) == 0)
			;
		data = HOST_IF_UART->RX_DATA;
		host_crc32 += ((uint32_t)data << (ii << 3));
	}

	command_crc32 = crc32_finalize(command_crc32);

	/*
	 * At this point we need to calculate a response CRC packet, but don't
	 * send it yet because the python script (Host) is waiting on this to
	 * continue.
	 */
	tx_buff[0] = command;

	resp_crc32 = crc32_init();
	resp_crc32 = crc32_update(resp_crc32, tx_buff, 1);
	resp_crc32 = crc32_finalize(resp_crc32);
	for (i = 0; i < 4; i++) {
		tx_buff[i + 1] = (uint8_t)(resp_crc32 >> (i << 3));
	}

	if (command_crc32 == host_crc32) {
		if (command == CMD_PRG_FILE) {
			comp_stat = process_pgm_file(ptr_offset, xfer_cnt);
		} else if (command == CMD_HDR_FILE) {
			comp_stat = extract_header_info();
		} else if (command == CMD_RD_FILE) {
			for (i = 0; i < 5; i++) {
				send_host_char(tx_buff[i]);
			}
		}
	} else { /*
		  * Send a failing CRC packet to python script(Host) so it
		  * doesn't hang.
		  */
		trace0(0, MAIN, 0, " !!!!CRC32 failure!!!!");
		trace0(0, MAIN, 0,
		       " !Sending erroneous CRC value to halt host process!");
		error_send();
		return ERROR_COMPLETION;
	}

	return (comp_stat);
}

uint8_t process_pgm_file(uint32_t ptr_offset, uint32_t xfer_cnt)
{
	uint8_t comp_stat = 0;
	uint8_t i;

	if ((ptr_offset % 0x1000) == 0) {
		trace1(0, MAIN, 0, "   Transfer offset: 0x%X ", ptr_offset);
	}

	if (FLSH_DATA_LENGTH) {
		TRANSFER_CNT += xfer_cnt;
		TOTAL_XFER_PROG_COUNT += xfer_cnt;

		if (TRANSFER_CNT > MAX_CHUNK_SIZE) {
			trace1(0, MAIN, 0,
			       " !!! ERROR: XFER_CNT > SRAM size = 0x%08X !!!",
			       TRANSFER_CNT);
			return ERROR_COMPLETION;
		}
		comp_stat = program_max_chunk();
		if (comp_stat)
			return comp_stat;
	} else {
		trace0(0, MAIN, 0, " !!!ERROR: FLSH_DATA_LENGTH = 0!!!");
	}

	for (i = 0; i < 5; i++) {
		send_host_char(tx_buff[i]);
	}

	return comp_stat;
}

uint8_t program_max_chunk(void)
{
	uint8_t status = 0;
	uint32_t ret = 0;

	if ((TRANSFER_CNT == FLSH_DATA_LENGTH) ||
	    (TRANSFER_CNT == MAX_CHUNK_SIZE)) {
		trace1(0, MAIN, 0, " TRANSFER_CNT 0x%08X", TRANSFER_CNT);

		ret = SPI_Operations(0);

		if (ret) {
			trace1(0, MAIN, 0, " SPI_Operation ERROR: 0x%08X", ret);
			status = ERROR_COMPLETION;
			return status;
		} else if (TOTAL_XFER_PROG_COUNT == FLSH_DATA_LENGTH_TOTAL) {
			trace2(0, MAIN, 0,
			       " FLSH_START_ADDR: 0x%08X,"
			       " FLSH_DATA_LENGTH: 0x%08X",
			       FLSH_START, TOTAL_XFER_PROG_COUNT);
			trace0(0, MAIN, 0, " SPI_Operation success");
			status = SUCCESSFUL_COMPLETION;
			return status;
		}

		FLSH_START_ADDR += MAX_CHUNK_SIZE;
		FLSH_DATA_LENGTH = FLSH_DATA_LENGTH_TOTAL - MAX_CHUNK_SIZE;
		trace2(0, MAIN, 0,
		       " FLSH_START_ADDR: 0x%08X, FLSH_DATA_LENGTH:"
		       " 0x%08X",
		       FLSH_START_ADDR, FLSH_DATA_LENGTH);

		TRANSFER_CNT = 0;

	} else {
		/* Waits for more chunks */
	}
	return status;
}

uint8_t extract_header_info(void)
{
	uint8_t i, ii;
	uint32_t ret = 0;
	uint32_t *readHdrPtr;

	CMD_DISP_EN = 1;
	TRANSFER_CNT = 0;
	TOTAL_XFER_PROG_COUNT = 0;

	readHdrPtr = (uint32_t *)&DATA_IO_BUFFER;
	HDR_FLAG = __builtin_bswap32(*readHdrPtr++);
	SPI_UTIL_CMD = __builtin_bswap32(*readHdrPtr++);
	FLSH_START = FLSH_START_ADDR = __builtin_bswap32(*readHdrPtr++);
	FLSH_DATA_LENGTH = __builtin_bswap32(*readHdrPtr++);
	FLSH_DATA_LENGTH_TOTAL = FLSH_DATA_LENGTH;

	if (FLSH_DATA_LENGTH_TOTAL > MAX_CHUNK_SIZE)
		FLSH_DATA_LENGTH = MAX_CHUNK_SIZE;

	GPIO_CNT = 0;
	for (ii = 0; ii < 27; ii++) {
		GPIO_ADDR[GPIO_CNT] = __builtin_bswap32(*readHdrPtr++);
		GPIO_SETTING[GPIO_CNT++] = __builtin_bswap32(*readHdrPtr++);
	}

	TERMINATOR = __builtin_bswap32(*readHdrPtr);

	if ((HDR_FLAG != 0x4D434850) || (TERMINATOR != 0x58454F46)) {
		trace2(0, MAIN, 0,
		       "\n !!!PgmHdrFile corrupt "
		       "header=0x%08X (s/b:0x4D434850), "
		       "terminator=0x%08X (s/b:0x58454F46)!!!",
		       HDR_FLAG, TERMINATOR);
		trace0(0, MAIN, 0,
		       " !!!Sending erroneous CRC"
		       " value to halt host process!!!");

		error_send();
		return ERROR_COMPLETION;
	}

	trace0(0, MAIN, 0, "\n PgmHdrFile.bin parameters received");
	trace1(0, MAIN, 0, " SPI_UTIL_CMD: 0x%08X", SPI_UTIL_CMD);
	trace1(0, MAIN, 0, " FLSH_START_ADDR: 0x%08X", FLSH_START_ADDR);
	trace1(0, MAIN, 0, " FLSH_DATA_LENGTH_TOTAL: 0x%08X",
	       FLSH_DATA_LENGTH_TOTAL);
	if (SPI_UTIL_CMD & USE_GPIO_INI_PARAMS_FLAG) {
		for (i = 0; i < 27; i++) {
			if (GPIO_ADDR[i]) {
				trace2(0, MAIN, 0, " GPIO_ADDR[%d]: 0x%08X", i,
				       GPIO_ADDR[i]);
				trace2(0, MAIN, 0, " GPIO_SETTING[%d]: 0x%08X",
				       i, GPIO_SETTING[i]);
			}
		}
	}

	/*
	 * Check if only doing a chip erase in SPI_Operations - no reason to
	 * load a data file just to erase.
	 */
	if ((SPI_UTIL_CMD & CMD_ERASE) || (SPI_UTIL_CMD & CMD_PARTIAL_ERASE)) {
		if (SPI_UTIL_CMD & CMD_ERASE) {
			trace0(0, MAIN, 0, " Erase Flash ONLY...");
		} else {
			trace0(0, MAIN, 0, " Partially Erase Flash ONLY...");
		}
		ret = SPI_Operations(0);
		if (ret) {
			trace1(0, MAIN, 0, " SPI_Operation ERROR: 0x%08X", ret);
			return ERROR_COMPLETION;
		}

		trace0(0, MAIN, 0, " SPI_Operation success");
		return SUCCESSFUL_COMPLETION;
	}

	for (i = 0; i < 5; i++) {
		send_host_char(tx_buff[i]);
	}

	return 0;
}

void error_send(void)
{
	send_host_char(0xDE);
	send_host_char(0xAD);
	send_host_char(0xDE);
	send_host_char(0xAD);
	send_host_char(0xFF);
}
