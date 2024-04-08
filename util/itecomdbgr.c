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

#define VERSION "0.0.7 debug20"
#define ITE_ERR 0xF0

#define USE_3M 0

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

#define STEPS_EXIT 0x00
#define STEPS_NORMAL 0x01
#define STEPS_TEST 0xEE

const uint8_t enable_follow_mode[16] = { W_CMD_PORT,  DBUS_ADDR_3, W_DATA_PORT,
					 0x7F,	      W_CMD_PORT,  DBUS_ADDR_2,
					 W_DATA_PORT, 0xFF,	   W_CMD_PORT,
					 DBUS_ADDR_1, W_DATA_PORT, 0xFF,
					 W_CMD_PORT,  DBUS_ADDR_0, W_DATA_PORT,
					 0xFF };

const uint8_t disable_follow_mode[8] = { W_CMD_PORT,  DBUS_ADDR_3, W_DATA_PORT,
					 0x40,	      W_CMD_PORT,  DBUS_ADDR_2,
					 W_DATA_PORT, 0x00 };

const uint8_t CS_Low[4] = { W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFD };

const uint8_t CS_High[8] = { W_CMD_PORT, DBUS_ADDR_1, W_DATA_PORT, 0xFE,
			     W_CMD_PORT, DBUS_DATA,   W_DATA_PORT, 0x00 };

const uint8_t spi_write_enable[16] = { W_CMD_PORT,  DBUS_ADDR_1, W_DATA_PORT,
				       0xFD,	    W_CMD_PORT,	 DBUS_DATA,
				       W_DATA_PORT, SPI_WREN,	 W_CMD_PORT,
				       DBUS_ADDR_1, W_DATA_PORT, 0xFE,
				       W_CMD_PORT,  DBUS_DATA,	 W_DATA_PORT,
				       0x00 };

const uint8_t spi_write_disable[16] = { W_CMD_PORT,  DBUS_ADDR_1, W_DATA_PORT,
					0xFD,	     W_CMD_PORT,  DBUS_DATA,
					W_DATA_PORT, SPI_WRDI,	  W_CMD_PORT,
					DBUS_ADDR_1, W_DATA_PORT, 0xFE,
					W_CMD_PORT,  DBUS_DATA,	  W_DATA_PORT,
					0x00 };

/* Config mostly comes from the command line.  Defaults are set in main(). */
struct itecomdbgr_config {
	int g_steps;
	FILE *fi;
	int g_flash_size;
	int g_blk_size;
	int g_blk_no;

	uint8_t SaveFlag;
	unsigned long update_start_addr;
	unsigned long update_end_addr;
	char *device_name;
	char *file_name;
	int file_size;
	int g_fd;
	int eflash_size_in_k;
	uint8_t eflash_type;
	uint8_t sector_erase_pages;
	uint8_t spi_cmd_sector_erase;
	uint8_t G_DBG_BUF[256];
	uint8_t *g_readbuf;
	uint8_t *g_writebuf;
};

#define EFLASH_TYPE_8315 0x01
#define EFLASH_TYPE_KGD 0x02
#define EFLASH_TYPE_NONE 0xFF

#define SPI_CMD_SECTOR_ERASE_1K 0xD7
#define SPI_CMD_SECTOR_ERASE_4K 0x20

#define delay_ms(x) msleep(x)

const uint8_t Read_ID_buf[8] = { W_CMD_PORT,	    DBUS_DATA,	W_DATA_PORT,
				 SPI_RDID,	    W_CMD_PORT, DBUS_256R_DATA,
				 R_BURST_DATA_PORT, 0x02 };

const uint8_t write_enable_buf[40] = {
	W_CMD_PORT,  DBUS_ADDR_3, W_DATA_PORT, 0x7F,	    W_CMD_PORT,
	DBUS_ADDR_2, W_DATA_PORT, 0xFF,	       W_CMD_PORT,  DBUS_ADDR_1,
	W_DATA_PORT, 0xFE,	  W_CMD_PORT,  DBUS_ADDR_0, W_DATA_PORT,
	0xFF,	     W_CMD_PORT,  DBUS_DATA,   W_DATA_PORT, 0x00,
	W_CMD_PORT,  DBUS_ADDR_1, W_DATA_PORT, 0xFD,	    W_CMD_PORT,
	DBUS_DATA,   W_DATA_PORT, SPI_WREN,    W_CMD_PORT,  DBUS_ADDR_1,
	W_DATA_PORT, 0xFE,	  W_CMD_PORT,  DBUS_ADDR_0, W_DATA_PORT,
	0xFF,	     W_CMD_PORT,  DBUS_DATA,   W_DATA_PORT, 0x00
};

const uint8_t read_status[31] = {
	W_CMD_PORT,  DBUS_ADDR_3, W_DATA_PORT, 0x7F,	    W_CMD_PORT,
	DBUS_ADDR_2, W_DATA_PORT, 0xFF,	       W_CMD_PORT,  DBUS_ADDR_1,
	W_DATA_PORT, 0xFE,	  W_CMD_PORT,  DBUS_ADDR_0, W_DATA_PORT,
	0xFF,	     W_CMD_PORT,  DBUS_DATA,   W_DATA_PORT, 0x00,
	W_CMD_PORT,  DBUS_ADDR_1, W_DATA_PORT, 0xFD,	    W_CMD_PORT,
	DBUS_DATA,   W_DATA_PORT, SPI_RDSR,    W_CMD_PORT,  DBUS_DATA,
	R_DATA_PORT
};

const uint8_t read_status_buf[7] = { W_CMD_PORT, DBUS_DATA,  W_DATA_PORT,
				     SPI_RDSR,	 W_CMD_PORT, DBUS_DATA,
				     R_DATA_PORT };

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
	conf->fi = fopen(conf->file_name, "rb");
	if (conf->fi != NULL) {
		fseek(conf->fi, 0, SEEK_END);
		conf->file_size = ftell(conf->fi);
		fseek(conf->fi, 0, SEEK_SET);
		conf->g_writebuf = malloc(conf->file_size);
		if (conf->g_writebuf == NULL) {
			printf("\n\ralloc g_writebuf fail");
		}
		conf->g_readbuf = malloc(conf->file_size);
		if (conf->g_readbuf == NULL) {
			printf("\n\ralloc g_readbuf fail");
		}
		len = fread(conf->g_writebuf, 1, conf->file_size, conf->fi);
	} else {
		printf("open file error : %s\n", conf->file_name);
		r = ITE_ERR;
	}

	return r;
}

void exit_file(struct itecomdbgr_config *conf)
{
	free(conf->g_writebuf);
	free(conf->g_readbuf);
	fclose(conf->fi);
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

unsigned int read_com(struct itecomdbgr_config *conf, uint8_t *inbuff,
		      int ReadBytes)
{
	bool bReadStat;

	bReadStat = read(conf->g_fd, inbuff, ReadBytes);

	/* debug */
	memcpy(conf->G_DBG_BUF, inbuff, 256);

	return bReadStat;
}

bool write_com(struct itecomdbgr_config *conf, const char *lpOutBuffer,
	       int WriteBytes)
{
	bool bWriteStat;

	bWriteStat = write(conf->g_fd, lpOutBuffer, WriteBytes);

	return bWriteStat;
}

void debug_putc(struct itecomdbgr_config *conf, uint8_t in_data)
{
	write(conf->g_fd, &in_data, 1);
}

uint8_t debug_getc(struct itecomdbgr_config *conf)
{
	uint8_t data[1];

	read(conf->g_fd, data, 1);
	return data[0];
}

void delay_500us(void)
{
	Sleep(1);
}

uint8_t rw_reg(struct itecomdbgr_config *conf, unsigned long Address,
	       uint8_t RW)
{
	/* [3][7][11] mapping to Address A2 A1 A0 */
	uint8_t rw_reg_buf[15] = { W_CMD_PORT,
				   0x80,
				   W_DATA_PORT,
				   (uint8_t)(Address >> 16),
				   W_CMD_PORT,
				   0x2F,
				   W_DATA_PORT,
				   (uint8_t)(Address >> 8),
				   W_CMD_PORT,
				   0x2E,
				   W_DATA_PORT,
				   (uint8_t)(Address),
				   W_CMD_PORT,
				   0x30,
				   0 };

	if (RW == REG_WRITE) {
		rw_reg_buf[14] = W_DATA_PORT;
	} else {
		rw_reg_buf[14] = R_DATA_PORT;
	}
	write_com(conf, rw_reg_buf, sizeof(rw_reg_buf));
}

void Wr_REG(struct itecomdbgr_config *conf, unsigned long Address, uint8_t WrD0)
{
	uint8_t Data[1];

	Data[0] = WrD0;
	rw_reg(conf, Address, REG_WRITE);
	write_com(conf, Data, sizeof(Data));
}

uint8_t Rd_REG(struct itecomdbgr_config *conf, unsigned long Address)
{
	rw_reg(conf, Address, REG_READ);
	Sleep(1);
	return debug_getc(conf);
}

/* disable protect path from DBGR */
static int dbgr_disable_protect_path(struct itecomdbgr_config *conf)
{
	int ret = 0, i;

	printf("\n\rDisabling protect path...\n");

	for (i = 0; i < 32; i++) {
		Wr_REG(conf, 0xF020A0 + i, 0);
	}

	if (ret < 0)
		fprintf(stderr, "DISABLE PROTECT PATH FROM DBGR FAILED!\n");

	return ret;
}

void check_wel(struct itecomdbgr_config *conf)
{
	uint8_t status[1];

	while (1) {
		write_com(conf, read_status, sizeof(read_status));
		Sleep(1);
		read_com(conf, status, 1);
		if ((status[0] & 0x02))
			break;
	}
}

void check_busy(struct itecomdbgr_config *conf)
{
	uint8_t status[1];

	while (1) {
		write_com(conf, read_status, sizeof(read_status));
		Sleep(1);
		read_com(conf, status, 1);
		if (!(status[0] & 0x01))
			break;
	}
}

void write_enable(struct itecomdbgr_config *conf)
{
	write_com(conf, write_enable_buf, sizeof(write_enable_buf));
	Sleep(2);
	check_wel(conf);
}

void Enter_Uart_DBGR_mode_and_Set_NACK_mode(struct itecomdbgr_config *conf)
{
	uint8_t Data[1];
	uint8_t rData[2];

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_BURST_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));

	Sleep(5);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x80;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0xF0;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x2F;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x40;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x2E;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x00;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_CMD_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x30;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);

	Data[0] = W_DATA_PORT;
	write_com(conf, Data, sizeof(Data));
	Data[0] = 0x03;
	write_com(conf, Data, sizeof(Data));
	Sleep(1);
}

uint8_t check_status(struct itecomdbgr_config *conf)
{
	uint8_t Temp;

	write_com(conf, CS_Low, sizeof(CS_Low));
	write_com(conf, read_status_buf, sizeof(read_status_buf));
	Temp = debug_getc(conf);
	write_com(conf, CS_High, sizeof(CS_High));

	return Temp;
}

int GetChipID(struct itecomdbgr_config *conf)
{
	uint8_t chipid[3], chipver, eflash_size_flag;

	chipid[0] = Rd_REG(conf, 0xF02085);
	chipid[1] = Rd_REG(conf, 0xF02086);
	chipid[2] = Rd_REG(conf, 0xF02087);
	chipver = Rd_REG(conf, 0xF02002);
	printf("\n\rChip ID = %02x%02x%02x", chipid[0], chipid[1], chipid[2]);
	printf(" , Chip Ver= %02x", chipver);
	eflash_size_flag = chipver >> 4;
	if (eflash_size_flag == 0xC)
		conf->eflash_size_in_k = 1024;
	if (eflash_size_flag == 0x8)
		conf->eflash_size_in_k = 512;
	printf(" , eflash size = %04d KB", conf->eflash_size_in_k);
	printf(" , file size = %04d B", conf->file_size);

	/* Get the real flash size , 64K for 1 Block*/
	/* Reset the global flash value */
	conf->g_flash_size = conf->eflash_size_in_k * 1024;
	conf->g_blk_size = conf->eflash_size_in_k / 64;

	conf->update_start_addr = 0;
	if (conf->file_size < conf->g_flash_size) {
		conf->update_end_addr = conf->file_size;
	} else {
		conf->update_end_addr = conf->g_flash_size;
	}
}

int Read_ID_2(struct itecomdbgr_config *conf)
{
	int result = 0;
	uint8_t FlashID[3];

	write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));
	write_com(conf, CS_Low, sizeof(CS_Low));
	write_com(conf, Read_ID_buf, sizeof(Read_ID_buf));

	FlashID[0] = debug_getc(conf);
	FlashID[1] = debug_getc(conf);
	FlashID[2] = debug_getc(conf);

	write_com(conf, CS_High, sizeof(CS_High));
	write_com(conf, disable_follow_mode, sizeof(disable_follow_mode));
	printf("\n\rFlash ID :%02x %02x %02x", FlashID[0], FlashID[1],
	       FlashID[2]);
	tcflush(conf->g_fd, TCIOFLUSH);

	if ((FlashID[0] == 0xFF) && (FlashID[1] == 0xFF) &&
	    (FlashID[2] == 0xFE)) {
		printf("\n\rFLASH TYPE = 8315");
		conf->eflash_type = EFLASH_TYPE_8315;
		result = 0;
	} else if ((FlashID[0] == 0xC8) || (FlashID[0] == 0xEF)) {
		printf("\n\rFLASH TYPE = KGD");
		conf->eflash_type = EFLASH_TYPE_KGD;
		result = 0;
		conf->g_steps = STEPS_EXIT;
	} else {
		printf("\n\rInvalid EFLASH TYPE");
		conf->eflash_type = EFLASH_TYPE_NONE;
		result = 1;
	}
	return result;
}

int FastRead(struct itecomdbgr_config *conf, uint8_t BA, uint8_t A1, uint8_t A0,
	     uint8_t *buffer)
{
	int result;
	int i;
	uint8_t fast_read[24] = {
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       SPI_FAST_READ,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       BA,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       A1,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       A0,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
		W_CMD_PORT, DBUS_256R_DATA, R_BURST_DATA_PORT, 0xFF
	};

	write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));
	write_com(conf, CS_High, sizeof(CS_High));
	write_com(conf, CS_Low, sizeof(CS_Low));
	write_com(conf, fast_read, sizeof(read));

	tcflush(conf->g_fd, TCIOFLUSH);
	if (USE_3M) {
		Sleep(6);
	} else {
		Sleep(60);
	}
	result = read_com(conf, buffer, 256);
	tcflush(conf->g_fd, TCIOFLUSH);
	return result;
}

void Erase_4K(struct itecomdbgr_config *conf)
{
	int i = 0;
	unsigned long start_addr = conf->update_start_addr;
	unsigned long end_addr = conf->update_end_addr;
	int total_size = (end_addr - start_addr) / 0x1000;

	/* [3] mapping to spi erase command ,*/
	/* [7][11][15] mapping to Address A2 A1 A0 */
	uint8_t erase_buf[16] = {
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, SPI_SE_4K,
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, 0x00,
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, 0x00,
		W_CMD_PORT, DBUS_DATA, W_DATA_PORT, 0x00,
	};

	while (start_addr < end_addr) {
		write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));
		write_com(conf, spi_write_enable, sizeof(spi_write_enable));
		while (!(check_status(conf) & 0x02))
			;
		write_com(conf, CS_Low, sizeof(CS_Low));
		erase_buf[7] = start_addr >> 16;
		erase_buf[11] = start_addr >> 8;
		erase_buf[15] = 0;
		write_com(conf, erase_buf, sizeof(erase_buf));
		write_com(conf, CS_High, sizeof(CS_High));
		while (check_status(conf) & 0x01)
			;
		write_com(conf, spi_write_disable, sizeof(spi_write_disable));
		write_com(conf, disable_follow_mode,
			  sizeof(disable_follow_mode));
		start_addr += 0x1000;
		printf("\rEraseing...     : %d%%",
		       (++i * 100) / (total_size - 1));
		fflush(stdout);
	}
}

void erase_flash(struct itecomdbgr_config *conf)
{
	Erase_4K(conf);
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

uint8_t FastRead_Burst_CData(struct itecomdbgr_config *conf, uint8_t *C_Data,
			     uint8_t FLAG)
{
	unsigned int i = 0;
	uint8_t R_Data;
	uint16_t divisor;
	uint8_t DBG_BUF[256];
	int j = 0;
	unsigned long start_addr = conf->update_start_addr;
	unsigned long end_addr = conf->update_end_addr;

	int total_size = (end_addr - start_addr) / 256;

	if (conf->eflash_size_in_k == 1024)
		divisor = 0xFFF;
	else
		divisor = 0x7FF;

	while (start_addr < end_addr) {
		write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));
		while (check_status(conf) & 0x01)
			;
		write_com(conf, CS_Low, sizeof(CS_Low));

		FastRead_buf[7] = (start_addr >> 16) & 0xFF;
		FastRead_buf[11] = (start_addr >> 8) & 0xFF;
		FastRead_buf[15] = (start_addr) & 0xFF;
		write_com(conf, FastRead_buf, sizeof(FastRead_buf));

		for (i = 0; i < 0x100; i++) {
			DBG_BUF[i] = debug_getc(conf);
			R_Data = DBG_BUF[i];

			if (!FLAG && conf->SaveFlag) {
				conf->g_readbuf[start_addr + i] = DBG_BUF[i];
			}

			if (FLAG) {
				if (R_Data != 0xFF) {
					printf("\n\rFastRead_Burst_CData ERR");
					printf(" addr=%lx,DBG_BUF[%x]=%x\n\r",
					       start_addr + i, i, DBG_BUF[i]);
					printf("\n\rFLAG=%x\n\r", FLAG);
					hexdump(DBG_BUF, 16);
					hexdump(conf->G_DBG_BUF, 16);
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
			       (++j * 100) / (total_size - 1));
		else
			printf("\rVerifying...    : %d%%               ",
			       (++j * 100) / (total_size - 1));

		write_com(conf, CS_High, sizeof(CS_High));
		while (check_status(conf) & 0x01)
			;
		write_com(conf, disable_follow_mode,
			  sizeof(disable_follow_mode));
		start_addr += 0x100;
	}

	if (!FLAG && conf->SaveFlag) {
		FILE *pW = NULL;

		pW = fopen("save.bin", "w");
		fseek(pW, 0, SEEK_SET);
		fwrite(conf->g_readbuf, 1, conf->eflash_size_in_k * 1024, pW);
		fclose(pW);
		printf("\n\rSave FW to save.bin\n\r");
	}

	return 0;
}

void Page_Program_Burst_v2(struct itecomdbgr_config *conf, uint8_t *wr_Data)
{
	unsigned int i = 0, j = 0;
	unsigned long start_addr = conf->update_start_addr;
	unsigned long end_addr = conf->update_end_addr;
	int total_size = (end_addr - start_addr) / 256;
	uint8_t pp_buf[20] = {
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       SPI_PP,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
		W_CMD_PORT, DBUS_DATA,	    W_DATA_PORT,       0x00,
		W_CMD_PORT, DBUS_256W_DATA, W_BURST_DATA_PORT, 0xFF
	};

	while (start_addr < end_addr) {
		write_com(conf, enable_follow_mode, sizeof(enable_follow_mode));
		write_com(conf, spi_write_enable, sizeof(spi_write_enable));

		while (!(check_status(conf) & 0x02))
			;
		write_com(conf, CS_Low, sizeof(CS_Low));

		pp_buf[7] = (start_addr >> 16) & 0xFF;
		pp_buf[11] = (start_addr >> 8) & 0xFF;
		pp_buf[15] = (start_addr) & 0xFF;
		write_com(conf, pp_buf, sizeof(pp_buf));

		for (i = 0; i < 0x100; i++) {
			debug_putc(conf, wr_Data[start_addr + i]);
		}

		write_com(conf, CS_High, sizeof(CS_High));
		while (check_status(conf) & 0x01)
			;
		write_com(conf, spi_write_disable, sizeof(spi_write_disable));
		write_com(conf, disable_follow_mode,
			  sizeof(disable_follow_mode));

		start_addr += 0x100;
		printf("\rPrograming...   : %d%%",
		       (++j * 100) / (total_size - 1));
		fflush(stdout);
	}
}

void write_flash(struct itecomdbgr_config *conf)
{
	Page_Program_Burst_v2(conf, conf->g_writebuf);
	printf("\n\r");
}

bool check_flash(struct itecomdbgr_config *conf)
{
	uint64_t addr = 0;
	uint8_t A2, A1, A0;
	uint8_t retry = 0;
	int i;
	int j = 0;
	bool result = TRUE;
	uint8_t DBG_BUF[256];
	int cnt;

	if (FastRead_Burst_CData(conf, NULL, 1)) {
		printf("\n\rcheck_flash : error");
		while (1)
			;
	}
	printf("\n\r");
	return 0;
}

bool verify_flash(struct itecomdbgr_config *conf)
{
	uint64_t addr = 0;
	uint8_t A2, A1, A0;
	int i;
	int j = 0;
	bool result = TRUE;
	uint8_t DBG_BUF[256];
	int cnt;

	if (FastRead_Burst_CData(conf, conf->g_writebuf, 0)) {
		printf("\n\rverify_flash : error");
		while (1)
			;
	}
	printf("\n\r");
	return 0;
}

void Enter_Uart_DBGR_mode(struct itecomdbgr_config *conf)
{
	uint8_t en_dbgr_buf[7] = { 0x00, W_CMD_PORT,	    0x00, W_DATA_PORT,
				   0x00, W_BURST_DATA_PORT, 0x00 };
	write_com(conf, en_dbgr_buf, sizeof(en_dbgr_buf));
	delay_ms(5);
}
void Exit_Uart_DBGR_mode(struct itecomdbgr_config *conf)
{
	uint8_t ex_dbgr_buf[16] = { W_CMD_PORT, 0x80, W_DATA_PORT, 0xF0,
				    W_CMD_PORT, 0x2F, W_DATA_PORT, 0x1C,
				    W_CMD_PORT, 0x2E, W_DATA_PORT, 0x08,
				    W_CMD_PORT, 0x30, W_DATA_PORT, 0x80 };
	write_com(conf, ex_dbgr_buf, sizeof(ex_dbgr_buf));
}

int uart_app(struct itecomdbgr_config *conf)
{
	char CHIPID[3];
	char DBGR_REG[2];
	uint8_t rData[2];
	int retry = 0;
	struct termios tty;
	uint8_t dbgr_reset_buf[4] = { W_CMD_PORT, 0x27, W_DATA_PORT, 0x80 };

	if (conf->device_name == NULL) {
		fprintf(stderr,
			"open device fail , please set the device name");
		return -1;
	}

	conf->g_fd = open(conf->device_name, O_RDWR | O_NOCTTY);

	if (conf->g_fd < 0) {
		perror("open");
		return -1;
	}

	if (tcgetattr(conf->g_fd, &tty) != 0) {
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
	if (tcsetattr(conf->g_fd, TCSANOW, &tty) != 0) {
		perror("tcsetattr");
	}
	tcflush(conf->g_fd, TCIOFLUSH);

	while (1) {
		if (conf->g_steps == STEPS_TEST) {
			Enter_Uart_DBGR_mode(conf);
			Read_ID_2(conf);
			conf->g_steps = STEPS_EXIT;
		}

		if (conf->g_steps == STEPS_NORMAL) {
			Enter_Uart_DBGR_mode_and_Set_NACK_mode(conf);
			tcflush(conf->g_fd, TCIOFLUSH);

			/* dbgr reset */
			write_com(conf, dbgr_reset_buf, sizeof(dbgr_reset_buf));
			tcflush(conf->g_fd, TCIOFLUSH);
			GetChipID(conf);
			Read_ID_2(conf);

			tcflush(conf->g_fd, TCIOFLUSH);
		}

		if (conf->g_steps == STEPS_EXIT) {
			tcflush(conf->g_fd, TCIOFLUSH);
			break;
		}

		Sleep(70);
	}

	dbgr_disable_protect_path(conf);

	conf->eflash_type = EFLASH_TYPE_KGD;

	switch (conf->eflash_type) {
	case EFLASH_TYPE_8315:
		conf->sector_erase_pages = 4;
		conf->spi_cmd_sector_erase = SPI_CMD_SECTOR_ERASE_1K;
		break;
	case EFLASH_TYPE_KGD:
		conf->sector_erase_pages = 16;
		conf->spi_cmd_sector_erase = SPI_CMD_SECTOR_ERASE_4K;
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
	close(conf->g_fd);
	return 0;
}

int main(int argc, char **argv)
{
	int r = 0;
	int option_index = 0;
	int c;
	char *optstring = "f:d:sh";
	struct option long_options[] = {
		{ "filename", required_argument, NULL, 'f' },
		{ "device", required_argument, NULL, 'd' },
		{ "savebin", no_argument, NULL, 's' },
		{ 0, 0, 0, 0 }
	};

	/* Set initial value , it will update the real value later */
	struct itecomdbgr_config conf = { .g_steps = STEPS_NORMAL,
					  .g_blk_size = 65536,
					  .g_blk_no = 16,
					  .g_flash_size = 0x100000,
					  .eflash_size_in_k = 0,
					  .eflash_type = 0xFF,
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
			printf("\n\r	-s : save read fw to save.bin");
			printf("\n\rExample :\n\r");
			printf("\n\r    %s -f ec.bin -d /dev/ttyUSB3\n\r",
			       argv[0]);
			printf("\n\r    %s -f ec.bin -d /dev/ttyUSB3 -s\n\r",
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
	exit_file(&conf);
	show_time();
	return r;
}
