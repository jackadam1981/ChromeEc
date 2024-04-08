/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Filename: itecomdbgr.c         For Chipset: ITE EC
 *
 * Function: ITE COM DBGR Flash Utility
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

#define VERSION "0.0.7 debug12"
#define ITE_ERR 0xF0

#define USE_3M 0

#define BOOL bool
#define BYTE uint8_t
#define UCHAR uint8_t
#define WORD uint32_t
#define DWORD uint64_t

#define FW_UPDATE_START 0x00000
#define FW_UPDATE_END 0x80000

#define TRUE 0
#define FALSE 1

#define msleep(msecs)                                                     \
	nanosleep(&(struct timespec){ msecs / 1000,                       \
				      (msecs * 1000000) % 1000000000UL }, \
		  NULL)

#define W_CMD_PORT 0xB4
#define W_DATA_PORT 0x6A
#define R_CMD_PORT 0xB4
#define R_DATA_PORT 0x6B
#define W_BURST_DATA_PORT 0xF2
#define R_BURST_DATA_PORT 0xF3

#define CHIPID_1 0x00
#define CHIPID_2 0x01
#define CHIPIDVER 0x02
#define DBUS_ADDR_0 0x04
#define DBUS_ADDR_1 0x05
#define DBUS_ADDR_2 0x06
#define DBUS_ADDR_3 0x07
#define DBUS_DATA 0x08
#define DBUS_256R_DATA 0x09
#define DBUS_256W_DATA 0x0A
#define EMU_KSI 0x20
#define RAM_ADDR_0 0x2E
#define RAM_ADDR_1 0x2F
#define RAM_DATA 0x30

#define REG_WRITE 0
#define REG_READ 1

/* SPI Command Set*/
/* SPI Page Program command*/
#define SPI_PP 0x02
/* SPI Write Disable command*/
#define SPI_WRDI 0x04
/* SPI Read Status command*/
#define SPI_RDSR 0x05
/* SPI Write Enable command*/
#define SPI_WREN 0x06
/* SPI Fast Read command*/
#define SPI_FAST_READ 0x0B
/* SPI Sector Erase command*/
#define SPI_SE_4K 0x20
#define SPI_SE_1K 0xD7
/* SPI Read ID command*/
#define SPI_RDID 0x9F

uint8_t enable_follow_mode[16] = { W_CMD_PORT, DBUS_ADDR_3, W_DATA_PORT, 0x7F,
				   W_CMD_PORT, DBUS_ADDR_2, W_DATA_PORT, 0xFF,
				   W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFF,
				   W_CMD_PORT, DBUS_ADDR_0, W_DATA_PORT, 0xFF };

uint8_t disable_follow_mode[8] = { W_CMD_PORT, DBUS_ADDR_3, W_DATA_PORT, 0x40,
				   W_CMD_PORT, DBUS_ADDR_2, W_DATA_PORT, 0x00 };

uint8_t CS_Low[4] = { W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD };

uint8_t CS_High[8] = { W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
		       W_CMD_PORT, DBUS_DATA,	W_DATA_PORT, 0x00 };

uint8_t spi_write_enable[16] = { W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD,
				 W_CMD_PORT, DBUS_DATA,	  W_DATA_PORT, SPI_WREN,
				 W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				 W_CMD_PORT, DBUS_DATA,	  W_DATA_PORT, 0x00 };

uint8_t spi_write_disable[16] = { W_CMD_PORT,  DBUS_ADDR_1, W_DATA_PORT,
				  0xFD,	       W_CMD_PORT,  DBUS_DATA,
				  W_DATA_PORT, SPI_WRDI,    W_CMD_PORT,
				  DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				  W_CMD_PORT,  DBUS_DATA,   W_DATA_PORT,
				  0x00 };

/* Config mostly comes from the command line.  Defaults are set in main(). */
struct itecomdbgr_config {
	int g_flash_size;
	int g_blk_size;
	int g_blk_no;

	UCHAR SaveFlag;
	unsigned long update_start_addr;
	unsigned long update_end_addr;
	char *device_name;
	char *file_name;
	int file_size;
	/*
	 *int eFlashSizeInK = 512;
	 *UCHAR eflash_type = 0xFF;
	 *UCHAR sector_erase_pages;
	 *UCHAR spi_cmd_sector_erase;
	 *UCHAR G_DBG_BUF[256];
	 */
};

int g_steps;
FILE *fi;
uint8_t *g_readbuf;
uint8_t *g_writebuf;
int g_flash_size;
int g_blk_size;
int g_blk_no;
int g_fd;

int eFlashSizeInK = 512;

#define EFLASH_TYPE_8315 0x01
#define EFLASH_TYPE_KGD 0x02
#define EFLASH_TYPE_NONE 0xFF

UCHAR eflash_type = 0xFF;
UCHAR sector_erase_pages;
UCHAR spi_cmd_sector_erase;

UCHAR G_DBG_BUF[256];

#define SPI_CMD_SECTOR_ERASE_1K 0xD7
#define SPI_CMD_SECTOR_ERASE_4K 0x20

#define delay_ms(x) msleep(x)

uint8_t Read_ID_buf[8] = { W_CMD_PORT,	      DBUS_DATA,  W_DATA_PORT,
			   SPI_RDID,	      W_CMD_PORT, DBUS_256R_DATA,
			   R_BURST_DATA_PORT, 0x02 };

uint8_t write_enable_buf[40] = { W_CMD_PORT, DBUS_ADDR_3, W_DATA_PORT, 0x7F,
				 W_CMD_PORT, DBUS_ADDR_2, W_DATA_PORT, 0xFF,
				 W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				 W_CMD_PORT, DBUS_ADDR_0, W_DATA_PORT, 0xFF,
				 W_CMD_PORT, DBUS_DATA,	  W_DATA_PORT, 0x00,
				 W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD,
				 W_CMD_PORT, DBUS_DATA,	  W_DATA_PORT, SPI_WREN,
				 W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				 W_CMD_PORT, DBUS_ADDR_0, W_DATA_PORT, 0xFF,
				 W_CMD_PORT, DBUS_DATA,	  W_DATA_PORT, 0x00 };

uint8_t read_status[31] = { W_CMD_PORT, DBUS_ADDR_3, W_DATA_PORT, 0x7F,
			    W_CMD_PORT, DBUS_ADDR_2, W_DATA_PORT, 0xFF,
			    W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
			    W_CMD_PORT, DBUS_ADDR_0, W_DATA_PORT, 0xFF,
			    W_CMD_PORT, DBUS_DATA,   W_DATA_PORT, 0x00,
			    W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD,
			    W_CMD_PORT, DBUS_DATA,   W_DATA_PORT, SPI_RDSR,
			    W_CMD_PORT, DBUS_DATA,   R_DATA_PORT };

uint8_t read_status_buf[7] = { W_CMD_PORT, DBUS_DATA, W_DATA_PORT, SPI_RDSR,
			       W_CMD_PORT, DBUS_DATA, R_DATA_PORT };

uint8_t PP_burst_write[60] = { W_CMD_PORT,
			       DBUS_ADDR_3,
			       W_DATA_PORT,
			       0x7F,
			       W_CMD_PORT,
			       DBUS_ADDR_2,
			       W_DATA_PORT,
			       0xFF,
			       W_CMD_PORT,
			       DBUS_ADDR_1,
			       W_DATA_PORT,
			       0xFE,
			       W_CMD_PORT,
			       DBUS_ADDR_0,
			       W_DATA_PORT,
			       0xFF,
			       W_CMD_PORT,
			       DBUS_DATA,
			       W_DATA_PORT,
			       0x00,
			       W_CMD_PORT,
			       DBUS_ADDR_1,
			       W_DATA_PORT,
			       0xFD,
			       W_CMD_PORT,
			       DBUS_DATA,
			       W_DATA_PORT,
			       SPI_PP,
			       W_CMD_PORT,
			       DBUS_DATA,
			       W_DATA_PORT,
			       0x00,
			       W_CMD_PORT,
			       DBUS_DATA,
			       W_DATA_PORT,
			       0x80,
			       W_CMD_PORT,
			       DBUS_DATA,
			       W_DATA_PORT,
			       0x00,
			       W_CMD_PORT,
			       DBUS_256W_DATA,
			       W_BURST_DATA_PORT,
			       0x03,
			       0x11,
			       0x22,
			       0x33,
			       0x44,
			       W_CMD_PORT,
			       DBUS_ADDR_1,
			       W_DATA_PORT,
			       0xFE,
			       W_CMD_PORT,
			       DBUS_ADDR_0,
			       W_DATA_PORT,
			       0xFF,
			       W_CMD_PORT,
			       DBUS_DATA,
			       W_DATA_PORT,
			       0x00 };

uint8_t PP_single_write[56] = { W_CMD_PORT, DBUS_ADDR_3, W_DATA_PORT, 0x7F,
				W_CMD_PORT, DBUS_ADDR_2, W_DATA_PORT, 0xFF,
				W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				W_CMD_PORT, DBUS_ADDR_0, W_DATA_PORT, 0xFF,
				W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, 0x00,
				W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD,
				W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, SPI_PP,
				W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, 0x00,
				W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, 0x80,
				W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, 0x00,
				W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, 0x77,
				W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				W_CMD_PORT, DBUS_ADDR_0, W_DATA_PORT, 0xFF,
				W_CMD_PORT, DBUS_DATA,	 W_DATA_PORT, 0x00 };

uint8_t FastRead_burst_read[52] = {
	W_CMD_PORT, DBUS_ADDR_3,    W_DATA_PORT,       0x7F,
	W_CMD_PORT, DBUS_ADDR_2,    W_DATA_PORT,       0xFF,
	W_CMD_PORT, DBUS_ADDR_1,    W_DATA_PORT,       0xFF,
	W_CMD_PORT, DBUS_ADDR_0,    W_DATA_PORT,       0xFF,
	W_CMD_PORT, DBUS_ADDR_1,    W_DATA_PORT,       0xFE,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_ADDR_1,    W_DATA_PORT,       0xFD,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       SPI_FAST_READ,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_256R_DATA, R_BURST_DATA_PORT, 0xFF
};

uint8_t FastRead_single_read[47] = {
	W_CMD_PORT,  DBUS_ADDR_3, W_DATA_PORT,	 0x7F,	      W_CMD_PORT,
	DBUS_ADDR_2, W_DATA_PORT, 0xFF,		 W_CMD_PORT,  DBUS_ADDR_1,
	W_DATA_PORT, 0xFE,	  W_CMD_PORT,	 DBUS_ADDR_0, W_DATA_PORT,
	0xFF,	     W_CMD_PORT,  DBUS_DATA,	 W_DATA_PORT, 0x00,
	W_CMD_PORT,  DBUS_ADDR_1, W_DATA_PORT,	 0xFD,	      W_CMD_PORT,
	DBUS_DATA,   W_DATA_PORT, SPI_FAST_READ, W_CMD_PORT,  DBUS_DATA,
	W_DATA_PORT, 0x00,	  W_CMD_PORT,	 DBUS_DATA,   W_DATA_PORT,
	0x80,	     W_CMD_PORT,  DBUS_DATA,	 W_DATA_PORT, 0x00,
	W_CMD_PORT,  DBUS_DATA,	  W_DATA_PORT,	 0x00,	      W_CMD_PORT,
	DBUS_DATA,   R_DATA_PORT
};

/* [3] mapping to spi erase command , [7][11][15] mapping to Address A2 A1 A0 */
uint8_t erase_buf[16] = {
	W_CMD_PORT,  DBUS_DATA, W_DATA_PORT, 0x00,	W_CMD_PORT,  DBUS_DATA,
	W_DATA_PORT, 0x00,	W_CMD_PORT,  DBUS_DATA, W_DATA_PORT, 0x00,
	W_CMD_PORT,  DBUS_DATA, W_DATA_PORT, 0x00,
};

/* [3][7][11] mapping to Address A2 A1 A0 */
uint8_t rw_reg_buf[15] = { W_CMD_PORT, 0x80, W_DATA_PORT, 0x00,
			   W_CMD_PORT, 0x2F, W_DATA_PORT, 0x00,
			   W_CMD_PORT, 0x2E, W_DATA_PORT, 0x00,
			   W_CMD_PORT, 0x30, 0x00 };

void hexdump(uint8_t *buffer, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if ((i % 16 == 0)) {
			printf(" %06X :", i);
		}
		printf(" %02x", buffer[i]);
		if ((i % 16 == 7)) {
			printf(" - ");
		}
		if (i % 16 == 15) {
			printf("\n\r");
		}
	}
}

void Sleep(int MicroSeconds)
{
	msleep(MicroSeconds);
}

int init_file(struct itecomdbgr_config *conf)
{
	int r = 0;
	int len;

	printf("\n\rOpen file: %s\n\r", conf->file_name);
	fi = fopen(conf->file_name, "rb");
	if (fi != NULL) {
		fseek(fi, 0, SEEK_END);
		conf->file_size = ftell(fi);
		fseek(fi, 0, SEEK_SET);
		g_writebuf = malloc(conf->file_size);
		if (g_writebuf == NULL) {
			printf("\n\ralloc g_writebuf fail");
		}
		g_readbuf = malloc(conf->file_size);
		if (g_readbuf == NULL) {
			printf("\n\ralloc g_readbuf fail");
		}
		len = fread(g_writebuf, 1, conf->file_size, fi);
	} else {
		printf("open file error : %s\n", conf->file_name);
		r = ITE_ERR;
	}

	return r;
}

void exit_file(void)
{
	free(g_writebuf);
	free(g_readbuf);
	fclose(fi);
}

void show_time(void)
{
	time_t current_time;
	char *c_time_string;

	/* Obtain current time. */
	current_time = time(NULL);

	/* Convert to local time format. */
	c_time_string = ctime(&current_time);
	(void)printf("Current time is %s", c_time_string);
}

unsigned int read_com(uint8_t *inbuff, int ReadBytes)
{
	BOOL bReadStat;

	bReadStat = read(g_fd, inbuff, ReadBytes);

	/* debug */
	memcpy(G_DBG_BUF, inbuff, 256);

	return bReadStat;
}

BOOL write_com(char *lpOutBuffer, int WriteBytes)
{
	BOOL bWriteStat;

	bWriteStat = write(g_fd, lpOutBuffer, WriteBytes);

	return bWriteStat;
}

void debug_putc(uint8_t in_data)
{
	write(g_fd, &in_data, 1);
}

uint8_t debug_getc(void)
{
	uint8_t data[1];

	read(g_fd, data, 1);
	return data[0];
}

void delay_500us(void)
{
	Sleep(1);
}

uint8_t Chk_AAh(void)
{
	delay_500us();
	if (debug_getc() != 0xAA) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t rw_reg(unsigned long Address, uint8_t RW)
{
	uint8_t WrA2, WrA1, WrA0;

	WrA2 = (uint8_t)(Address >> 16);
	WrA1 = (uint8_t)(Address >> 8);
	WrA0 = (uint8_t)(Address);

	/*[3][7][11] mapping to Address A2 A1 A0,[15] mapping to write value*/
	rw_reg_buf[3] = WrA2;
	rw_reg_buf[7] = WrA1;
	rw_reg_buf[11] = WrA0;
	if (RW == REG_WRITE) {
		rw_reg_buf[14] = W_DATA_PORT;
	} else {
		rw_reg_buf[14] = R_DATA_PORT;
	}
	write_com(rw_reg_buf, sizeof(rw_reg_buf));
}

void Wr_REG(unsigned long Address, uint8_t WrD0)
{
	uint8_t Data[1];

	Data[0] = WrD0;
	rw_reg(Address, REG_WRITE);
	write_com(Data, sizeof(Data));
}

uint8_t Rd_REG(unsigned long Address)
{
	rw_reg(Address, REG_READ);
	Sleep(1);
	return debug_getc();
}

void check_wel(void)
{
	UCHAR status[1];

	while (1) {
		write_com(read_status, sizeof(read_status));
		Sleep(1);
		read_com(status, 1);
		if ((status[0] & 0x02))
			break;
	}
}

void check_busy(void)
{
	UCHAR status[1];

	while (1) {
		write_com(read_status, sizeof(read_status));
		Sleep(1);
		read_com(status, 1);
		if (!(status[0] & 0x01))
			break;
	}
}

void write_enable(void)
{
	write_com(write_enable_buf, sizeof(write_enable_buf));
	Sleep(2);
	check_wel();
}

void Enter_Uart_DBGR_mode_and_Set_NACK_mode(void)
{
	uint8_t Data[1];
	uint8_t rData[2];

	Data[0] = 0x00;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = 0x00;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = 0x00;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_BURST_DATA_PORT;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = 0x00;
	write_com(Data, sizeof(Data));

	Sleep(5);

	Data[0] = W_CMD_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0x80;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0xF0;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0x2F;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0x40;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0x2E;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0x00;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0x30;
	write_com(Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(Data, sizeof(Data));
	Data[0] = 0x03;
	write_com(Data, sizeof(Data));
	Sleep(1);
}

uint8_t check_status(void)
{
	uint8_t Temp;

	write_com(CS_Low, sizeof(CS_Low));
	write_com(read_status_buf, sizeof(read_status_buf));
	Temp = debug_getc();
	write_com(CS_High, sizeof(CS_High));

	return Temp;
}

int GetChipID(struct itecomdbgr_config *conf)
{
	uint8_t chipid[3], chipver, eflash_size_flag;
	int eflash_size = 0;

	chipid[0] = Rd_REG(0xF02085);
	chipid[1] = Rd_REG(0xF02086);
	chipid[2] = Rd_REG(0xF02087);
	chipver = Rd_REG(0xF02002);
	printf("\n\rChip ID = %02x%02x%02x", chipid[0], chipid[1], chipid[2]);
	printf(" , Chip Ver= %02x", chipver);
	eflash_size_flag = chipver >> 4;
	if (eflash_size_flag == 0xC)
		eflash_size = 1024;
	if (eflash_size_flag == 0x8)
		eflash_size = 512;
	printf(" , eflash size = %04d KB", eflash_size);
	printf(" , file size = %04d B", conf->file_size);

	/* Get the real flash size , 64K for 1 Block*/
	/* Reset the global flash value */
	conf->g_flash_size = eflash_size * 1024;
	conf->g_blk_size = eflash_size / 64;

	conf->update_start_addr = 0;
	if (conf->file_size < conf->g_flash_size) {
		conf->update_end_addr = conf->file_size;
	} else {
		conf->update_end_addr = conf->g_flash_size;
	}
}

int Read_ID_2(void)
{
	int result = 0;
	uint8_t FlashID[3];

	write_com(enable_follow_mode, sizeof(enable_follow_mode));
	write_com(CS_Low, sizeof(CS_Low));
	write_com(Read_ID_buf, sizeof(Read_ID_buf));

	FlashID[0] = debug_getc();
	FlashID[1] = debug_getc();
	FlashID[2] = debug_getc();

	write_com(CS_High, sizeof(CS_High));
	write_com(disable_follow_mode, sizeof(disable_follow_mode));
	printf("\n\rFlash ID :%02x%02x%02x", FlashID[0], FlashID[1],
	       FlashID[2]);
	tcflush(g_fd, TCIOFLUSH);

	if ((FlashID[0] == 0xFF) && (FlashID[1] == 0xFF) &&
	    (FlashID[2] == 0xFE)) {
		printf("\n\rFLASH TYPE = 8315");
		eflash_type = EFLASH_TYPE_8315;
		result = 0;
	} else if ((FlashID[0] == 0xC8) || (FlashID[0] == 0xEF)) {
		printf("\n\rFLASH TYPE = KGD");
		eflash_type = EFLASH_TYPE_KGD;
		result = 0;
		g_steps = 0;
	} else {
		printf("\n\rInvalid EFLASH TYPE");
		eflash_type = EFLASH_TYPE_NONE;
		result = 1;
	}
	return result;
}

int FastRead(UCHAR BA, UCHAR A1, UCHAR A0, UCHAR *buffer)
{
	int result;
	int i;

	FastRead_burst_read[31 + 4] = BA;
	FastRead_burst_read[35 + 4] = A1;
	FastRead_burst_read[39 + 4] = A0;
	write_com(FastRead_burst_read, 52);
	tcflush(g_fd, TCIOFLUSH);
	if (USE_3M) {
		Sleep(6);
	} else {
		Sleep(60);
	}
	result = read_com(buffer, 256);
	tcflush(g_fd, TCIOFLUSH);
	return result;
}

void Erase_4K(unsigned long start_addr, unsigned long end_addr)
{
	int i = 0;

	while (start_addr < end_addr) {
		write_com(enable_follow_mode, sizeof(enable_follow_mode));
		write_com(spi_write_enable, sizeof(spi_write_enable));
		while (!(check_status() & 0x02))
			;
		write_com(CS_Low, sizeof(CS_Low));
		erase_buf[3] = SPI_SE_4K;
		erase_buf[7] = start_addr >> 16;
		erase_buf[11] = start_addr >> 8;
		erase_buf[15] = 0;
		write_com(erase_buf, sizeof(erase_buf));
		write_com(CS_High, sizeof(CS_High));
		while (check_status() & 0x01)
			;
		write_com(spi_write_disable, sizeof(spi_write_disable));
		write_com(disable_follow_mode, sizeof(disable_follow_mode));
		start_addr += 0x1000;
		printf("\rEraseing...     : %d%%", (++i * 100) / 0x7F);
		fflush(stdout);
	}
}

void erase_flash(struct itecomdbgr_config *conf)
{
	Erase_4K(conf->update_start_addr, conf->update_end_addr);
	printf("\n\r");
}

uint8_t FastRead_buf[24] = {
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       SPI_FAST_READ,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
	W_CMD_PORT, DBUS_256R_DATA, R_BURST_DATA_PORT, 0xFF
};

uint8_t FastRead_Burst_CData(unsigned long start_addr, unsigned long end_addr,
			     uint8_t *C_Data, uint8_t FLAG)
{
	unsigned int i = 0;
	uint8_t R_Data;
	UCHAR DBG_BUF[256];
	int j = 0;

	while (start_addr < end_addr) {
		write_com(enable_follow_mode, sizeof(enable_follow_mode));
		while (check_status() & 0x01)
			;
		write_com(CS_Low, sizeof(CS_Low));

		FastRead_buf[7] = (start_addr >> 16) & 0xFF;
		FastRead_buf[11] = (start_addr >> 8) & 0xFF;
		FastRead_buf[15] = (start_addr) & 0xFF;
		write_com(FastRead_buf, sizeof(FastRead_buf));

		for (i = 0; i < 0x100; i++) {
			DBG_BUF[i] = debug_getc();
			R_Data = DBG_BUF[i];
			if (FLAG) {
				if (R_Data != 0xFF) {
					printf("\n\rFastRead_Burst_CData ERR");
					printf(" addr=%lx,DBG_BUF[%x]=%x\n\r",
					       start_addr + i, i, DBG_BUF[i]);
					printf("\n\rFLAG=%x\n\r", FLAG);
					hexdump(DBG_BUF, 16);
					hexdump(G_DBG_BUF, 16);
					return 1;
				}
			} else {
				if (R_Data != C_Data[start_addr + i]) {
					printf("\n\rFastRead_Burst_CData ERR");
					printf("addr=%lx,R_Data=%x ",
					       start_addr + i, R_Data);
					printf("Data[%lx]=%x\n\r",
					       start_addr + i,
					       C_Data[start_addr + i]);
					return 1;
				}
			}
		}

		if (FLAG)
			printf("\rChecking...     : %d%%               ",
			       (++j * 100) / 0x7FF);
		else
			printf("\rVerifying...    : %d%%               ",
			       (++j * 100) / 0x7FF);

		write_com(CS_High, sizeof(CS_High));
		while (check_status() & 0x01)
			;
		write_com(disable_follow_mode, sizeof(disable_follow_mode));
		start_addr += 0x100;
	}
	return 0;
}

uint8_t pp_buf[20] = { W_CMD_PORT, DBUS_DATA,	   W_DATA_PORT,	      SPI_PP,
		       W_CMD_PORT, DBUS_DATA,	   W_DATA_PORT,	      0x00,
		       W_CMD_PORT, DBUS_DATA,	   W_DATA_PORT,	      0x00,
		       W_CMD_PORT, DBUS_DATA,	   W_DATA_PORT,	      0x00,
		       W_CMD_PORT, DBUS_256W_DATA, W_BURST_DATA_PORT, 0xFF };

void Page_Program_Burst_v2(unsigned long start_addr, unsigned long end_addr,
			   uint8_t *wr_Data)
{
	unsigned int i = 0, j = 0;

	while (start_addr < end_addr) {
		write_com(enable_follow_mode, sizeof(enable_follow_mode));
		write_com(spi_write_enable, sizeof(spi_write_enable));

		while (!(check_status() & 0x02))
			;
		write_com(CS_Low, sizeof(CS_Low));

		pp_buf[7] = (start_addr >> 16) & 0xFF;
		pp_buf[11] = (start_addr >> 8) & 0xFF;
		pp_buf[15] = (start_addr) & 0xFF;
		write_com(pp_buf, sizeof(pp_buf));

		for (i = 0; i < 0x100; i++) {
			debug_putc(wr_Data[start_addr + i]);
		}

		write_com(CS_High, sizeof(CS_High));
		while (check_status() & 0x01)
			;
		write_com(spi_write_disable, sizeof(spi_write_disable));
		write_com(disable_follow_mode, sizeof(disable_follow_mode));

		start_addr += 0x100;
		printf("\rPrograming...   : %d%%", (++j * 100) / 0x7FF);
		fflush(stdout);
	}
}

void write_flash(struct itecomdbgr_config *conf)
{
	Page_Program_Burst_v2(conf->update_start_addr, conf->update_end_addr,
			      g_writebuf);
	printf("\n\r");
}

BOOL check_flash(struct itecomdbgr_config *conf)
{
	DWORD addr = 0;
	UCHAR A2, A1, A0;
	UCHAR retry = 0;
	int i;
	int j = 0;
	BOOL result = TRUE;
	UCHAR DBG_BUF[256];
	int cnt;

	if (FastRead_Burst_CData(conf->update_start_addr, conf->update_end_addr,
				 NULL, 1)) {
		printf("\n\rcheck_flash : error");
		while (1)
			;
	}
	printf("\n\r");
	return 0;
}

BOOL verify_flash(struct itecomdbgr_config *conf)
{
	DWORD addr = 0;
	UCHAR A2, A1, A0;
	int i;
	int j = 0;
	BOOL result = TRUE;
	UCHAR DBG_BUF[256];
	int cnt;

	if (FastRead_Burst_CData(conf->update_start_addr, conf->update_end_addr,
				 g_writebuf, 0)) {
		printf("\n\rverify_flash : error");
		while (1)
			;
	}
	printf("\n\r");
	return 0;

	for (addr = 0; addr < (eFlashSizeInK * 1024); addr += 0x100) {
		A0 = (UCHAR)(addr & 0xFF);
		A1 = (UCHAR)((addr >> 8) & 0xFF);
		A2 = (UCHAR)((addr >> 16) & 0xFF);
		cnt = FastRead(A2, A1, A0, DBG_BUF);
		for (i = 0; i < 256; i++) {
			if (DBG_BUF[i] != g_writebuf[addr + i]) {
				printf("\n\rCompare Flash Error!!");
				printf("addr=%lx DBG_BUF[%x]=%x ", addr, i,
				       DBG_BUF[i]);
				printf("g_writebuf[%lx]=%x cnt=%x\n\r", addr,
				       g_writebuf[addr + i], cnt);
				printf("Command=>\n\r");
				hexdump(FastRead_burst_read, 48);
				printf("\n\rRead Buffer=>");
				hexdump(DBG_BUF, 256);
				fflush(stdout);
				result = FALSE;
				goto exit;
			}
		}
		printf("\rVerifying...    : %d%%", (++j * 100) / 0x7FF);
		fflush(stdout);
	}
exit:
	printf("\n\r\n\r");

	if (conf->SaveFlag) {
		FILE *pW = NULL;

		pW = fopen("save.bin", "w");
		fseek(pW, 0, SEEK_SET);
		fwrite(g_readbuf, 1, eFlashSizeInK * 1024, pW);
		fclose(pW);
	}

	return result;
}

void Enter_Uart_DBGR_mode(void)
{
	uint8_t en_dbgr_buf[7] = { 0x00, W_CMD_PORT,	    0x00, W_DATA_PORT,
				   0x00, W_BURST_DATA_PORT, 0x00 };
	write_com(en_dbgr_buf, sizeof(en_dbgr_buf));
	delay_ms(5);
}
void Exit_Uart_DBGR_mode(void)
{
	uint8_t ex_dbgr_buf[16] = { W_CMD_PORT, 0x80, W_DATA_PORT, 0xF0,
				    W_CMD_PORT, 0x2F, W_DATA_PORT, 0x1C,
				    W_CMD_PORT, 0x2E, W_DATA_PORT, 0x08,
				    W_CMD_PORT, 0x30, W_DATA_PORT, 0x80 };
	write_com(ex_dbgr_buf, sizeof(ex_dbgr_buf));
}

int uart_app(struct itecomdbgr_config *conf)
{
	char CHIPID[3];
	char DBGR_REG[2];
	uint8_t rData[2];
	int retry = 0;
	struct termios tty;

	if (conf->device_name == NULL) {
		fprintf(stderr,
			"open device fail , please set the device name");
		return -1;
	}

	g_fd = open(conf->device_name, O_RDWR | O_NOCTTY);

	if (g_fd < 0) {
		perror("open");
		return -1;
	}

	if (tcgetattr(g_fd, &tty) != 0) {
		perror("tcgetattr");
		return -1;
	}

	tty.c_cflag |= PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~CRTSCTS;
	tty.c_cflag |= CREAD | CLOCAL;
	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO;
	tty.c_lflag &= ~ECHOE;
	tty.c_lflag &= ~ECHONL;
	tty.c_lflag &= ~ISIG;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_iflag &=
		~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
	tty.c_oflag &= ~OPOST;
	tty.c_oflag &= ~ONLCR;

	tty.c_cc[VTIME] = 5;
	tty.c_cc[VMIN] = 0;

	if (USE_3M == 0) {
		/* set baud rate to 115200 */
		cfsetospeed(&tty, B115200);
		cfsetispeed(&tty, B115200);
	} else {
		cfsetospeed(&tty, B3000000);
		cfsetispeed(&tty, B3000000);
	}
	/*  apply settings to serial port */
	if (tcsetattr(g_fd, TCSANOW, &tty) != 0) {
		perror("tcsetattr");
	}
	tcflush(g_fd, TCIOFLUSH);

	g_steps = 0x87;
	while (1) {
		if (g_steps == 0x88) {
			Enter_Uart_DBGR_mode();
			Read_ID_2();
			g_steps = 0;
		}

		if (g_steps == 0x87) {
			Enter_Uart_DBGR_mode_and_Set_NACK_mode();
			tcflush(g_fd, TCIOFLUSH);

			/* dbgr reset */
			debug_putc(W_CMD_PORT);
			debug_putc(0x27);
			debug_putc(W_DATA_PORT);
			debug_putc(0x80);
			tcflush(g_fd, TCIOFLUSH);
			GetChipID(conf);
			Read_ID_2();

			tcflush(g_fd, TCIOFLUSH);
		}

		if (g_steps == 0x0) {
			tcflush(g_fd, TCIOFLUSH);
			break;
		}

		Sleep(70);
	}

	eflash_type = EFLASH_TYPE_KGD;

	switch (eflash_type) {
	case EFLASH_TYPE_8315:
		sector_erase_pages = 4;
		spi_cmd_sector_erase = SPI_CMD_SECTOR_ERASE_1K;
		break;
	case EFLASH_TYPE_KGD:
		sector_erase_pages = 16;
		spi_cmd_sector_erase = SPI_CMD_SECTOR_ERASE_4K;
		break;
	default:
		printf("\n\rInvalid EFLASH TYPE!");
		goto out;
	}
	erase_flash(conf);
	if (check_flash(conf) == FALSE)
		goto out;

	write_flash(conf);
	if (verify_flash(conf) == FALSE)
		goto out;

out:
	close(g_fd);
	return 0;
}

int main(int argc, char **argv)
{
	int r = 0;
	int option_index = 0;
	int c;
	char *optstring = "f:s:d:h";
	struct option long_options[] = {
		{ "filename", required_argument, NULL, 'f' },
		{ "device", required_argument, NULL, 'd' },
		{ "savebin", no_argument, NULL, 's' },
		{ 0, 0, 0, 0 }
	};

	struct itecomdbgr_config conf = { .g_blk_size = 65536,
					  .g_blk_no = 16,
					  .g_flash_size = g_blk_size * g_blk_no,
					  .SaveFlag = 0,
					  .file_name = NULL,
					  .device_name = NULL };

	while (1) {
		c = getopt_long(argc, argv, optstring, long_options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			conf.file_name = optarg;
			break;
		case 'd':
			conf.device_name = optarg;
			break;
		case 's':
			conf.SaveFlag = 1;
			break;
		case 'h':
		default:
			printf("\n\rITE COMDBGR Flash Tool:%s\n\r", VERSION);
			printf("\n\rUsage:");
			printf("\n\r	-f : fw filename");
			printf("\n\r	-d : device name");
			printf("\n\rExample : %s -f ec.bin -d /dev/ttyUSB3\n\r",
			       argv[0]);
			exit(1);
		}
	}

	printf("\n\rITE COMDBGR Linux Flash Tool: Version %s\n\r", VERSION);
	show_time();
	if (conf.file_name == NULL) {
		printf("\n\rchoose a file to flash..\n\r");
		return 0;
	}

	r = init_file(&conf);
	if (r) {
		printf("Open file error\n\r");
		exit(1);
	}

	uart_app(&conf);

	exit_file();
	show_time();
	return r;
}
