/*
 * Filename: itecomdbgr.c         For Chipset: ITE EC
 *
 * Function: ITE COM DBGR Flash Utility
 *
 * Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

#define VERSION "0.0.6 debug24"
#define ITE_ERR 0xF0

#define USE_3M 0

/* 1b: NACK mode , 0b: ACK mode */
#define NACK_mode 1

#define BOOL unsigned char
#define BYTE unsigned char
#define UCHAR unsigned char
#define WORD unsigned int
#define DWORD unsigned long

#define FW_UPDATE_START 0x00000
#define FW_UPDATE_END 0x80000

#define TRUE 0
#define FALSE 1

#define msleep(msecs)                                                     \
	nanosleep(&(struct timespec){ msecs / 1000,                       \
				      (msecs * 1000000) % 1000000000UL }, \
		  NULL)

#if NACK_mode
#define EnFollow_Mode()           \
	{                         \
		debug_putc(0xB4); \
		debug_putc(0x07); \
		debug_putc(0x6A); \
		debug_putc(0x7F); \
		debug_putc(0xB4); \
		debug_putc(0x06); \
		debug_putc(0x6A); \
		debug_putc(0xFF); \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		debug_putc(0x6A); \
		debug_putc(0xFF); \
		debug_putc(0xB4); \
		debug_putc(0x04); \
		debug_putc(0x6A); \
		debug_putc(0xFF); \
	}
#define DisFollow_Mode()          \
	{                         \
		debug_putc(0xB4); \
		debug_putc(0x07); \
		debug_putc(0x6A); \
		debug_putc(0x40); \
		debug_putc(0xB4); \
		debug_putc(0x06); \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#define CS_Low()                  \
	{                         \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		debug_putc(0x6A); \
		debug_putc(0xFD); \
	}
#define CS_High()                 \
	{                         \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		debug_putc(0x6A); \
		debug_putc(0xFE); \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#define spiWriteEnable()          \
	{                         \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		debug_putc(0x6A); \
		debug_putc(0xFD); \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		debug_putc(0x6A); \
		debug_putc(0x06); \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		debug_putc(0x6A); \
		debug_putc(0xFE); \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#define spiWriteDisable()         \
	{                         \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		debug_putc(0x6A); \
		debug_putc(0xFD); \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		debug_putc(0x6A); \
		debug_putc(0x04); \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		debug_putc(0x6A); \
		debug_putc(0xFE); \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#else
#define EnFollow_Mode()           \
	{                         \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x07); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x7F); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x06); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFF); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFF); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x04); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFF); \
	}
#define DisFollow_Mode()          \
	{                         \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x07); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x40); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x06); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#define CS_Low()                  \
	{                         \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFD); \
		delay_500us();    \
	}
#define CS_High()                 \
	{                         \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFE); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#define spiWriteEnable()          \
	{                         \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFD); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x06); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFE); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#define spiWriteDisable()         \
	{                         \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFD); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x04); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x05); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0xFE); \
		delay_500us();    \
		debug_putc(0xB4); \
		debug_putc(0x08); \
		delay_500us();    \
		debug_putc(0x6A); \
		debug_putc(0x00); \
	}
#endif

int g_steps;

FILE *fi;
unsigned char *g_readbuf;
unsigned char *g_writebuf;
int g_flash_size;
int g_blk_size;
int g_blk_no;
int g_fd;
UCHAR SaveFlag;
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

unsigned char Read_ID[32] = { 0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF,
			      0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x04, 0x6A, 0xFF,
			      0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFD,
			      0xB4, 0x08, 0x6A, 0x9F, 0xB4, 0x09, 0xF3, 0x02 };

unsigned char WriteEnable[40] = { 0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A,
				  0xFF, 0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x04,
				  0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00, 0xB4,
				  0x05, 0x6A, 0xFD, 0xB4, 0x08, 0x6A, 0x06,
				  0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x04, 0x6A,
				  0xFF, 0xB4, 0x08, 0x6A, 0x00 };
unsigned char WriteDisable[40] = { 0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A,
				   0xFF, 0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x04,
				   0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00, 0xB4,
				   0x05, 0x6A, 0xFD, 0xB4, 0x08, 0x6A, 0x04,
				   0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x04, 0x6A,
				   0xFF, 0xB4, 0x08, 0x6A, 0x00 };
unsigned char ReadStatus[31] = { 0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF,
				 0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x04, 0x6A, 0xFF,
				 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFD,
				 0xB4, 0x08, 0x6A, 0x05, 0xB4, 0x08, 0x6B };
unsigned char SectorErase_20h[52] = {
	0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF, 0xB4, 0x05, 0x6A,
	0xFE, 0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05,
	0x6A, 0xFD, 0xB4, 0x08, 0x6A, 0x20, 0xB4, 0x08, 0x6A, 0x00, 0xB4,
	0x08, 0x6A, 0x80, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFE,
	0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00
};
unsigned char SectorErase_D7h[52] = {
	0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF, 0xB4, 0x05, 0x6A,
	0xFE, 0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05,
	0x6A, 0xFD, 0xB4, 0x08, 0x6A, 0xD7, 0xB4, 0x08, 0x6A, 0x00, 0xB4,
	0x08, 0x6A, 0x80, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFE,
	0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00
};
unsigned char PP_burst_write[60] = {
	0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF, 0xB4, 0x05, 0x6A, 0xFE,
	0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFD,
	0xB4, 0x08, 0x6A, 0x02, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6A, 0x80,
	0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x0A, 0xF2, 0x03, 0x11, 0x22, 0x33, 0x44,
	0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00
};

unsigned char PP_single_write[56] = {
	0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF, 0xB4, 0x05, 0x6A, 0xFE,
	0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFD,
	0xB4, 0x08, 0x6A, 0x02, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6A, 0x80,
	0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6A, 0x77, 0xB4, 0x05, 0x6A, 0xFE,
	0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00
};

unsigned char FastRead_burst_read[52] = {
	0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF, 0xB4, 0x05, 0x6A,
	0xFF, 0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x05, 0x6A, 0xFE, 0xB4, 0x08,
	0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFD, 0xB4, 0x08, 0x6A, 0x0B, 0xB4,
	0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6A, 0x00,
	0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x09, 0xF3, 0xFF
};

unsigned char FastRead_single_read[47] = {
	0xB4, 0x07, 0x6A, 0x7F, 0xB4, 0x06, 0x6A, 0xFF, 0xB4, 0x05, 0x6A, 0xFE,
	0xB4, 0x04, 0x6A, 0xFF, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x05, 0x6A, 0xFD,
	0xB4, 0x08, 0x6A, 0x0B, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6A, 0x80,
	0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6A, 0x00, 0xB4, 0x08, 0x6B
};

void hexdump(unsigned char *buffer, int len)
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

int init_file(char *filename)
{
	int r = 0;
	int len;
	int file_size;

	printf("\n\rOpen file: %s\n\r", filename);
	fi = fopen(filename, "rb");
	if (fi != NULL) {
		fseek(fi, 0, SEEK_END);
		file_size = ftell(fi);
		fseek(fi, 0, SEEK_SET);
		g_blk_no = file_size / g_blk_size;
		if (file_size % g_blk_size)
			g_blk_no++;

		g_flash_size = g_blk_size * g_blk_no;

		g_writebuf = malloc(g_flash_size);
		if (g_writebuf == NULL) {
			printf("\n\ralloc g_writebuf fail");
		}
		g_readbuf = malloc(g_flash_size);
		if (g_readbuf == NULL) {
			printf("\n\ralloc g_readbuf fail");
		}
		len = fread(g_writebuf, 1, g_flash_size, fi);
	} else {
		printf("open file error : %s\n", filename);
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

void check_parameter(void)
{
	g_blk_size = 65536;
	g_blk_no = 16;
	g_flash_size = g_blk_size * g_blk_no;
}

unsigned int ReadCom(unsigned char *inbuff, int ReadBytes)
{
	BOOL bReadStat;

	bReadStat = read(g_fd, inbuff, ReadBytes);

	/* debug */
	memcpy(G_DBG_BUF, inbuff, 256);

	return bReadStat;
}

void debug_putc(unsigned char in_data)
{
	write(g_fd, &in_data, 1);
}

unsigned char debug_getc(void)
{
	unsigned char data[1];

	read(g_fd, data, 1);
	return data[0];
}

void delay_500us(void)
{
	Sleep(1);
}

unsigned char Chk_AAh(void)
{
	delay_500us();
	if (debug_getc() != 0xAA) {
		return 1;
	} else {
		return 0;
	}
}

void Wr_REG(unsigned long Address, unsigned char WrD0)
{
	unsigned char WrA2, WrA1, WrA0;

	WrA2 = (unsigned char)(Address >> 16);
	WrA1 = (unsigned char)(Address >> 8);
	WrA0 = (unsigned char)(Address);

#if NACK_mode
	debug_putc(0xB4);
	debug_putc(0x80);
	debug_putc(0x6A);
	debug_putc(WrA2);
	debug_putc(0xB4);
	debug_putc(0x2F);
	debug_putc(0x6A);
	debug_putc(WrA1);
	debug_putc(0xB4);
	debug_putc(0x2E);
	debug_putc(0x6A);
	debug_putc(WrA0);
	debug_putc(0xB4);
	debug_putc(0x30);
	debug_putc(0x6A);
	debug_putc(WrD0);
#else
	debug_putc(0xB4);
	debug_putc(0x80);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(WrA2);
	while (Chk_AAh())
		;
	debug_putc(0xB4);
	debug_putc(0x2F);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(WrA1);
	while (Chk_AAh())
		;
	debug_putc(0xB4);
	debug_putc(0x2E);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(WrA0);
	while (Chk_AAh())
		;
	debug_putc(0xB4);
	debug_putc(0x30);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(WrD0);
	while (Chk_AAh())
		;
#endif
}

unsigned char Rd_REG(unsigned long Address)
{
	unsigned char WrA2, WrA1, WrA0;

	WrA2 = (unsigned char)(Address >> 16);
	WrA1 = (unsigned char)(Address >> 8);
	WrA0 = (unsigned char)(Address);

#if NACK_mode
	debug_putc(0xB4);
	debug_putc(0x80);
	debug_putc(0x6A);
	debug_putc(WrA2);
	debug_putc(0xB4);
	debug_putc(0x2F);
	debug_putc(0x6A);
	debug_putc(WrA1);
	debug_putc(0xB4);
	debug_putc(0x2E);
	debug_putc(0x6A);
	debug_putc(WrA0);
	debug_putc(0xB4);
	debug_putc(0x30);
	debug_putc(0x6B);
	Sleep(1);
#else

	debug_putc(0xB4);
	debug_putc(0x80);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(WrA2);
	while (Chk_AAh())
		;
	debug_putc(0xB4);
	debug_putc(0x2F);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(WrA1);
	while (Chk_AAh())
		;
	debug_putc(0xB4);
	debug_putc(0x2E);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(WrA0);
	while (Chk_AAh())
		;
	debug_putc(0xB4);
	debug_putc(0x30);
	while (Chk_AAh())
		;
	debug_putc(0x6B);
	delay_500us();
#endif

	return debug_getc();
}

BOOL WriteCom(char *lpOutBuffer, int WriteBytes)
{
	BOOL bWriteStat;

	bWriteStat = write(g_fd, lpOutBuffer, WriteBytes);

	return bWriteStat;
}

void check_wel(void)
{
	UCHAR status[1];

	while (1) {
		WriteCom(ReadStatus, 31);
		Sleep(1);
		ReadCom(status, 1);
		if ((status[0] & 0x02))
			break;
	}
}

void check_busy(void)
{
	UCHAR status[1];

	while (1) {
		WriteCom(ReadStatus, 31);
		Sleep(1);
		ReadCom(status, 1);
		if (!(status[0] & 0x01))
			break;
	}
}

void write_enable(void)
{
	WriteCom(WriteEnable, 40);
	Sleep(2);
	check_wel();
}

void ComPortDebugWriteRegMem(unsigned int Addr, unsigned char Data)
{
	char Buffer[16];

	Buffer[0] = 0xB4;
	Buffer[1] = 0x80;
	Buffer[2] = 0x6A;
	Buffer[3] = (unsigned char)(Addr >> 16);
	Buffer[4] = 0xB4;
	Buffer[5] = 0x2F;
	Buffer[6] = 0x6A;
	Buffer[7] = (unsigned char)(Addr >> 8);
	Buffer[8] = 0xB4;
	Buffer[9] = 0x2E;
	Buffer[10] = 0x6A;
	Buffer[11] = (unsigned char)(Addr >> 0);
	Buffer[12] = 0xB4;
	Buffer[13] = 0x30;
	Buffer[14] = 0x6A;
	Buffer[15] = Data;
	WriteCom(Buffer, 16);
	Sleep(3);
}

BOOL ComPortDebugReadRegMem(unsigned int Addr, unsigned int len, char *Data,
			    BOOL FailToStop)
{
	char Buffer[15];
	unsigned int ReadBytes, i;

	for (i = 0; i < len; i++) {
		Buffer[0] = 0xB4;
		Buffer[1] = 0x80;
		Buffer[2] = 0x6A;
		Buffer[3] = (unsigned char)(Addr >> 16);
		Buffer[4] = 0xB4;
		Buffer[5] = 0x2F;
		Buffer[6] = 0x6A;
		Buffer[7] = (unsigned char)(Addr >> 8);
		Buffer[8] = 0xB4;
		Buffer[9] = 0x2E;
		Buffer[10] = 0x6A;
		Buffer[11] = (unsigned char)(Addr >> 0);
		Buffer[12] = 0xB4;
		Buffer[13] = 0x30;
		Buffer[14] = 0x6B;
		WriteCom(Buffer, 15);
		Sleep(5);
		ReadBytes = ReadCom(Data + i, 1);
		Sleep(1);
		Addr++;
	}

	return TRUE;
}

void Enter_Uart_DBGR_mode_and_Set_NACK_mode(void)
{
	unsigned char Data[1];
	unsigned char rData[2];

	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0xF2;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x00;
	WriteCom(Data, 1);

	Sleep(5);

	Data[0] = 0xB4;
	WriteCom(Data, 1);
	Data[0] = 0x80;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	WriteCom(Data, 1);
	Data[0] = 0xF0;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	WriteCom(Data, 1);
	Data[0] = 0x2F;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	WriteCom(Data, 1);
	Data[0] = 0x40;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	WriteCom(Data, 1);
	Data[0] = 0x2E;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	WriteCom(Data, 1);
	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	WriteCom(Data, 1);
	Data[0] = 0x30;
	WriteCom(Data, 1);
	Sleep(1);

#if NACK_mode
	Data[0] = 0x6A;
	WriteCom(Data, 1);
	Data[0] = 0x03;
	WriteCom(Data, 1);
	Sleep(1);
#else
	Data[0] = 0x6A;
	WriteCom(Data, 1);
	Data[0] = 0x02;
	WriteCom(Data, 1);
	Sleep(1);
#endif
}

void EnterDbgrMode(void)
{
	unsigned char Data[1];

	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0xF2;
	WriteCom(Data, 1);
	Sleep(1);

	Data[0] = 0x00;
	WriteCom(Data, 1);
	Sleep(1);
}

void ExitDbgrMode(void)
{
	ComPortDebugWriteRegMem(0xF01C08, 0x80);
}

void SetUartEnterDbgr(UCHAR Mode)
{
	char Data[2];

	Data[0] = 0xB4;
	Data[1] = 0x80;
	WriteCom(Data, 2);

	Sleep(2);
	ReadCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	Data[1] = 0xF0;
	WriteCom(Data, 2);
	Sleep(2);

	ReadCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	Data[1] = 0x2F;
	WriteCom(Data, 2);
	Sleep(2);
	ReadCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	Data[1] = 0x40;
	WriteCom(Data, 2);
	Sleep(2);
	ReadCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	Data[1] = 0x2E;
	WriteCom(Data, 2);
	Sleep(2);
	ReadCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	Data[1] = 0x00;
	WriteCom(Data, 2);
	Sleep(2);
	ReadCom(Data, 1);
	Sleep(1);

	Data[0] = 0xB4;
	Data[1] = 0x30;
	WriteCom(Data, 2);
	Sleep(2);
	ReadCom(Data, 1);
	Sleep(1);

	Data[0] = 0x6A;
	Data[1] = Mode;
	WriteCom(Data, 2);
	Sleep(2);
}

unsigned char CheckStatus(void)
{
	unsigned char Temp;

#if NACK_mode
	CS_Low();
	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6A);
	debug_putc(0x05);
	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6B);
	Temp = debug_getc();
	CS_High();
#else
	CS_Low();
	debug_putc(0xB4);
	debug_putc(0x08);
	while (Chk_AAh())
		;
	debug_putc(0x6A);
	debug_putc(0x05);
	while (Chk_AAh())
		;
	debug_putc(0xB4);
	debug_putc(0x08);
	while (Chk_AAh())
		;
	debug_putc(0x6B);
	Temp = debug_getc();
	CS_High();
#endif
	return Temp;
}

int wait_parity(void)
{
	int data;
	int cnt = 0;

	while (data = Rd_REG(0xF04009)) {
		Wr_REG(0xF04009, 0x01);
		cnt++;
	}
	while (data = Rd_REG(0xF0400B)) {
		Wr_REG(0xF0400B, 0x01);
		cnt++;
	}

	return cnt;
}

int wait_parity_2(void)
{
	int data;
	int cnt = 0;

	data = Rd_REG(0xF0400B);
	printf("\n\rwait_parity_2: data = %02x", data);
	return cnt;
}

int Read_ID_2(void)
{
	int result = 0;
	unsigned char FlashID[3];

	EnFollow_Mode();
	CS_Low();

	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6A);
	debug_putc(0x9F);

	debug_putc(0xB4);
	debug_putc(0x09);
	debug_putc(0xF3);
	debug_putc(0x02);

	FlashID[0] = debug_getc();
	FlashID[1] = debug_getc();
	FlashID[2] = debug_getc();

	CS_High();
	DisFollow_Mode();
	printf("\n\rFlash ID : ");
	hexdump(FlashID, 3);
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

int GetFlashID(void)
{
	unsigned char FlashID[3];
	int result = 0;

	WriteCom(Read_ID, 32);
	Sleep(2);
	ReadCom(FlashID, 3);
	Sleep(10);
	printf("\n\rFlashID = %02x %02x %02x\n\r", FlashID[0], FlashID[1],
	       FlashID[2]);
	if ((FlashID[0] == 0xFF) && (FlashID[1] == 0xFF) &&
	    (FlashID[2] == 0xFE)) {
		printf("\n\rFLASH TYPE = 8315");
		eflash_type = EFLASH_TYPE_8315;
		result = 0;
	} else if ((FlashID[0] == 0xC8) || (FlashID[0] == 0xEF)) {
		printf("\n\rFLASH TYPE = KGD");
		eflash_type = EFLASH_TYPE_KGD;
		result = 0;
	} else {
		printf("\n\rInvalid EFLASH TYPE");
		eflash_type = EFLASH_TYPE_NONE;
		result = 1;
	}
	return result;
}

int FastRead_Burst(UCHAR BA, UCHAR A1, UCHAR A0, UCHAR *buffer)
{
	unsigned int i = 0;
	int result;

	EnFollow_Mode();
	while (CheckStatus() & 0x01)
		;
	Sleep(1);
	CS_Low();

	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6A);
	debug_putc(0x0B);
	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6A);
	debug_putc(BA);
	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6A);
	debug_putc(A1);
	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6A);
	debug_putc(A0);
	debug_putc(0xB4);
	debug_putc(0x08);
	debug_putc(0x6A);
	debug_putc(0x00);

	debug_putc(0xB4);
	debug_putc(0x09);
	debug_putc(0xF3);
	debug_putc(0xFF);

	if (USE_3M) {
		Sleep(6);
	} else {
		Sleep(60);
	}

	result = ReadCom(buffer, 256);

	CS_High();
	while (CheckStatus() & 0x01)
		;
	Sleep(1);
	DisFollow_Mode();
	tcflush(g_fd, TCIOFLUSH);

	return result;
}

int FastRead(UCHAR BA, UCHAR A1, UCHAR A0, UCHAR *buffer)
{
	int result;
	int i;

	FastRead_burst_read[31 + 4] = BA;
	FastRead_burst_read[35 + 4] = A1;
	FastRead_burst_read[39 + 4] = A0;
	WriteCom(FastRead_burst_read, 52);
	tcflush(g_fd, TCIOFLUSH);
	if (USE_3M) {
		Sleep(6);
	} else {
		Sleep(60);
	}
	result = ReadCom(buffer, 256);
	tcflush(g_fd, TCIOFLUSH);
	return result;
}

int FastRead128(UCHAR BA, UCHAR A1, UCHAR A0, UCHAR *buffer)
{
	int result;
	int i;

	FastRead_burst_read[31] = BA;
	FastRead_burst_read[35] = A1;
	FastRead_burst_read[39] = A0;
	FastRead_burst_read[47] = 0x7F;
	WriteCom(FastRead_burst_read, 48);
	tcflush(g_fd, TCIOFLUSH);
	if (USE_3M) {
		Sleep(6);
	} else {
		Sleep(60);
	}
	result = ReadCom(buffer, 128);
	tcflush(g_fd, TCIOFLUSH);
	return result;
}

void FastRead2(UCHAR BA, UCHAR A1, UCHAR A0, UCHAR *buffer)
{
	FastRead_burst_read[31] = BA;
	FastRead_burst_read[35] = A1;
	FastRead_burst_read[39] = A0;
	WriteCom(FastRead_burst_read, 48);
	Sleep(41);
	ReadCom(buffer, 256);
	hexdump(buffer, 256);
	ReadCom(buffer, 256);
	hexdump(buffer, 256);
}

void PageWrite(UCHAR BA, UCHAR A1, UCHAR A0, UCHAR *buffer)
{
	write_enable();
	PP_burst_write[31] = BA;
	PP_burst_write[35] = A1;
	PP_burst_write[39] = A0;
	PP_burst_write[43] = 0xFF;
	WriteCom(PP_burst_write, 44);
	WriteCom(buffer, 256);
	WriteCom(&PP_burst_write[48], 12);
	check_busy();
}

void SectorErase_1K(void)
{
}

void SectorErase_4K(UCHAR BA, UCHAR A1)
{
	write_enable();
	SectorErase_20h[31] = BA;
	SectorErase_20h[35] = A1;

	WriteCom(SectorErase_20h, 52);
	Sleep(2);
	check_busy();
}
void Erase_4K(unsigned long start_addr, unsigned long end_addr)
{
	int i = 0;

	while (start_addr < end_addr) {
		EnFollow_Mode();
		spiWriteEnable();
		while (!(CheckStatus() & 0x02))
			;
		CS_Low();
#if NACK_mode
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(0x20);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(0X00);
#else
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(0x20);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(0X00);
		while (Chk_AAh())
			;
#endif
		CS_High();
		while (CheckStatus() & 0x01)
			;
		spiWriteDisable();
		DisFollow_Mode();
		start_addr += 0x1000;
		printf("\rEraseing...     : %d%%", (++i * 100) / 0x7F);
		fflush(stdout);
	}
}
void Erase_1K(unsigned long start_addr, unsigned long end_addr)
{
	while (start_addr < end_addr) {
		EnFollow_Mode();
		spiWriteEnable();
		while (!(CheckStatus() & 0x02))
			;
		CS_Low();
#if NACK_mode
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(0xD7);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(0X00);
#else
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(0xD7);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(0X00);
		while (Chk_AAh())
			;
#endif
		CS_High();
		while (CheckStatus() & 0x01)
			;
		spiWriteDisable();
		DisFollow_Mode();
		start_addr += 0x400;
	}
}
void erase_flash(void)
{
	Erase_4K(FW_UPDATE_START, FW_UPDATE_END);
	printf("\n\r");
}

unsigned char FastRead_Burst_CData(unsigned long start_addr,
				   unsigned long end_addr,
				   unsigned char *C_Data, unsigned char FLAG)
{
	unsigned int i = 0;
	unsigned char R_Data;
	UCHAR DBG_BUF[256];
	int j = 0;

	while (start_addr < end_addr) {
		EnFollow_Mode();
		while (CheckStatus() & 0x01)
			;
		CS_Low();
#if NACK_mode
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(0x0B);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(0x00);
		debug_putc(0xB4);
		debug_putc(0x09);
		debug_putc(0xF3);
		debug_putc(0xFF);

		for (i = 0; i < 0x100; i++) {
			DBG_BUF[i] = debug_getc();
			R_Data = DBG_BUF[i];
			if (FLAG) {
				if (R_Data != 0xFF) {
					printf("\n\rFastRead_Burst_CData ERR");
					printf("addr=%lx,DBG_BUF[%x]=%x\n\r",
					       start_addr + i, i, DBG_BUF[i]);
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

#else
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(0x0B);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(0x00);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x09);
		while (Chk_AAh())
			;
		debug_putc(0xF3);
		debug_putc(0xFF);

		for (i = 0; i < 0x100; i++) {
			ECReg(DLM + 0x800 + i) = debug_getc();
			R_Data = ECReg(DLM + 0x800 + i);

			if (FLAG) {
				if (R_Data != 0xFF) {
					ECReg(DLM + 0x73E) = R_Data;
					ECReg(DLM + 0x73F) = i;
					return 1;
				}
			} else {
				if (R_Data != i) {
					ECReg(DLM + 0x73E) = R_Data;
					ECReg(DLM + 0x73F) = i;
					return 1;
				}
			}
		}
#endif
		CS_High();
		while (CheckStatus() & 0x01)
			;
		DisFollow_Mode();
		start_addr += 0x100;
	}
	return 0;
}

void Page_Program_Burst_v2(unsigned long start_addr, unsigned long end_addr,
			   unsigned char *wr_Data)
{
	unsigned int i = 0, j = 0;

	while (start_addr < end_addr) {
		EnFollow_Mode();
		spiWriteEnable();
		while (!(CheckStatus() & 0x02))
			;
		CS_Low();
#if NACK_mode
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(0x02);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		debug_putc(0xB4);
		debug_putc(0x08);
		debug_putc(0x6A);
		debug_putc(start_addr);
		debug_putc(0xB4);
		debug_putc(0x0A);
		debug_putc(0xF2);
		debug_putc(0xFF);
		for (i = 0; i < 0x100; i++) {
			debug_putc(wr_Data[start_addr + i]);
		}
#else
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(0x02);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 16);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr >> 8);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x08);
		while (Chk_AAh())
			;
		debug_putc(0x6A);
		debug_putc(start_addr);
		while (Chk_AAh())
			;
		debug_putc(0xB4);
		debug_putc(0x0A);
		while (Chk_AAh())
			;
		debug_putc(0xF2);
		debug_putc(0xFF);
		while (Chk_AAh())
			;
		for (i = 0; i < 0x100; i++) {
			debug_putc(i);
			while (Chk_AAh())
				;
		}
#endif
		CS_High();
		while (CheckStatus() & 0x01)
			;
		spiWriteDisable();
		DisFollow_Mode();

		start_addr += 0x100;
		printf("\rPrograming...   : %d%%", (++j * 100) / 0x7FF);
		fflush(stdout);
	}
}

void write_flash(void)
{
	Page_Program_Burst_v2(FW_UPDATE_START, FW_UPDATE_END, g_writebuf);
	printf("\n\r");
}

BOOL check_flash(void)
{
	DWORD addr = 0;
	UCHAR A2, A1, A0;
	UCHAR retry = 0;
	int i;
	int j = 0;
	BOOL result = TRUE;
	UCHAR DBG_BUF[256];
	int cnt;

	if (FastRead_Burst_CData(FW_UPDATE_START, FW_UPDATE_END, NULL, 1)) {
		printf("\n\rcheck_flash : error");
		while (1)
			;
	}
	printf("\n\r");
	return 0;
}

BOOL verify_flash(void)
{
	DWORD addr = 0;
	UCHAR A2, A1, A0;
	int i;
	int j = 0;
	BOOL result = TRUE;
	UCHAR DBG_BUF[256];
	int cnt;

	if (FastRead_Burst_CData(FW_UPDATE_START, FW_UPDATE_END, g_writebuf,
				 0)) {
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
				       DBG_BUF[i], addr);
				printf("g_writebuf[%lx]=%x cnt=%x\n\r",
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

	if (SaveFlag) {
		FILE *pW = NULL;

		pW = fopen("save.bin", "w");
		fseek(pW, 0, SEEK_SET);
		fwrite(g_readbuf, 1, eFlashSizeInK * 1024, pW);
		fclose(pW);
	}

	return result;
}

void read_flash(void)
{
	DWORD addr = 0;
	UCHAR A2, A1, A0;

	for (addr = 0; addr < (eFlashSizeInK * 1024); addr += 0x100) {
		A0 = (UCHAR)(addr & 0xFF);
		A1 = (UCHAR)((addr >> 8) & 0xFF);
		A2 = (UCHAR)((addr >> 16) & 0xFF);
		printf("\n\r addr = %lx A2=%x A1=%x A0=%x", addr, A2, A1, A0);
		FastRead(A2, A1, A0, &g_readbuf[addr]);
	}

	if (SaveFlag) {
		FILE *pW = NULL;

		pW = fopen("save.bin", "w");
		fseek(pW, 0, SEEK_SET);
		fwrite(g_readbuf, 1, eFlashSizeInK * 1024, pW);
		fclose(pW);
	}
}

void Enter_Uart_DBGR_mode(void)
{
	debug_putc(0x00);
	debug_putc(0xB4);
	debug_putc(0x00);
	debug_putc(0x6A);
	debug_putc(0x00);
	debug_putc(0xF2);
	debug_putc(0x00);
	delay_ms(5);
}
void Exit_Uart_DBGR_mode(void)
{
	debug_putc(0xB4);
	debug_putc(0x80);
	debug_putc(0x6A);
	debug_putc(0xF0);
	debug_putc(0xB4);
	debug_putc(0x2F);
	debug_putc(0x6A);
	debug_putc(0x1C);
	debug_putc(0xB4);
	debug_putc(0x2E);
	debug_putc(0x6A);
	debug_putc(0x08);
	debug_putc(0xB4);
	debug_putc(0x30);
	debug_putc(0x6A);
	debug_putc(0x80);
}

int uart_app(char *devicename)
{
	char CHIPID[3];
	char DBGR_REG[2];
	unsigned char rData[2];
	int retry = 0;
	struct termios tty;

	if (devicename == NULL) {
		perror("open device fail , please set the device name");
		return -1;
	}

	g_fd = open(devicename, O_RDWR | O_NOCTTY);

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
			debug_putc(0xB4);
			debug_putc(0x27);
			debug_putc(0x6A);
			debug_putc(0x80);
			tcflush(g_fd, TCIOFLUSH);
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
	erase_flash();
	if (check_flash() == FALSE)
		goto out;

	write_flash();
	if (verify_flash() == FALSE)
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
	char *filename = NULL;
	char *devicename = NULL;
	char *optstring = "f:s:d:";
	struct option long_options[] = {
		{ "filename", required_argument, NULL, 'f' },
		{ "device", required_argument, NULL, 'd' },
		{ "savebin", no_argument, NULL, 's' },
		{ 0, 0, 0, 0 }
	};

	SaveFlag = 0;
	while (1) {
		c = getopt_long(argc, argv, optstring, long_options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			filename = optarg;
			break;
		case 'd':
			devicename = optarg;
			break;
		case 's':
			SaveFlag = 1;
			break;

		default:
			printf("Usage: %s [...]\n", argv[0]);
			exit(1);
		}
	}

	check_parameter();

	printf("\n\rITE COMDBGR Linux Flash Tool: Version %s\n\r", VERSION);
	show_time();
	if (filename == NULL) {
		printf("\n\rchoose a file to flash..\n\r");
		return 0;
	}

	r = init_file(filename);
	if (r) {
		printf("Open file error\n\r");
		exit(1);
	}

	uart_app(devicename);

	exit_file();
	show_time();
	return r;
}
