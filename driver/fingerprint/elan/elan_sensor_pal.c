/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* ELAN Platform Abstraction Layer callbacks */

#include <stddef.h>
#include "common.h"
#include "console.h"
#include "endian.h"
#include "fpsensor.h"
#include "gpio.h"
#include "link_defs.h"
#include "spi.h"
#include "system.h"
#include "timer.h"
#include "util.h"
#include "shared_mem.h"
#include "math_util.h"

#include "elan_setting.h"
#include "elan_sensor.h"
#include "elan_sensor_pal.h"


int elan_write_CMD(uint8_t FP_CMD)
{
	int rc = 0;

	memset((char *)tx_buf, 0, CONFIG_SPI_TX_BUF_SIZE);
	memset((char *)rx_buf, 0, CONFIG_SPI_RX_BUF_SIZE);

	tx_buf[0] = FP_CMD;
	rc = spi_transaction(&spi_devices[0], (char *)tx_buf,
				2, (char *)rx_buf, SPI_READBACK_ALL);
	return rc;
}

int elan_read_CMD(uint8_t FP_CMD, uint8_t *RegData)
{
	int ret = 0;

	memset((char *)tx_buf, 0, CONFIG_SPI_TX_BUF_SIZE);
	memset((char *)rx_buf, 0, CONFIG_SPI_RX_BUF_SIZE);


	tx_buf[0] =  FP_CMD; /* one byte data read */
	ret = spi_transaction(&spi_devices[0], (char *)tx_buf,
					2, (char *)rx_buf, SPI_READBACK_ALL);
	*RegData = rx_buf[1];

	return ret;
}

int elan_spi_transaction(uint8_t *tx, int tx_len, uint8_t *rx, int rx_len)
{
	int ret = 0;

	memset((char *)tx_buf, 0, CONFIG_SPI_TX_BUF_SIZE);
	memset((char *)rx_buf, 0, CONFIG_SPI_RX_BUF_SIZE);


	memcpy((char *)tx_buf, tx, tx_len);
	ret = spi_transaction(&spi_devices[0], (char *)tx_buf,
					tx_len, (char *)rx_buf, rx_len);
	memcpy(rx, (char *)rx_buf, rx_len);
	return ret;
}

int elan_write_register(uint8_t RegAddr, uint8_t RegData)
{
	int ret = 0;

	memset((char *)tx_buf, 0, CONFIG_SPI_TX_BUF_SIZE);
	memset((char *)rx_buf, 0, CONFIG_SPI_RX_BUF_SIZE);

	tx_buf[0] = WRITE_REG_HEAD + RegAddr; /* one byte data write */
	tx_buf[1] = RegData;
	ret = spi_transaction(&spi_devices[0], (char *)tx_buf,
				2, (char *)rx_buf, SPI_READBACK_ALL);
	return ret;
}

int elan_write_Page(uint8_t page)
{
	int ret = 0;

	memset((char *)tx_buf, 0, CONFIG_SPI_TX_BUF_SIZE);
	memset((char *)rx_buf, 0, CONFIG_SPI_RX_BUF_SIZE);

	tx_buf[0] = 0x07;
	tx_buf[1] = page;
	ret = spi_transaction(&spi_devices[0], (char *)tx_buf,
				2, (char *)rx_buf, SPI_READBACK_ALL);

	return ret;
}

int elan_write_reg_vector(const uint8_t *reg_table, int length)
{
	int ret = 0;
	int i = 0;
	uint8_t Write_RegAddr;
	uint8_t Write_RegData;

	for (i = 0; i < length; i = i + 2) {
		Write_RegAddr = reg_table[i];
		Write_RegData = reg_table[i+1];
		ret = elan_write_register(Write_RegAddr, Write_RegData);
		if (ret < 0)
			break;
	}
	return ret;
}

int RawCapture(unsigned short *pShortRaw)
{
	int ret = 0, i = 0, image_index = 0, index = 0;
	int cnt_timer = 0;
	int DMA_WrapLoop = 0, DMA_Length = 0;
	uint8_t RegData[4] = {0};
	char *imagebuffer;

	memset(pShortRaw, 0, sizeof(unsigned short)*_imageTotalPixel);

	ret = shared_mem_acquire(sizeof(uint8_t)*_imgbufsize, &imagebuffer);
	if (ret) {
		LOGE_SA("%s Can't get shared mem\n", __func__);
		return ret;
	}
	memset(imagebuffer, 0, sizeof(uint8_t)*_imgbufsize);

	/*Write start scan command to fp sensor*/
	if (elan_write_CMD(0x01) < 0) {
		ret = WRITE_ERR;
		LOGE_SA("%s SPISendCommand( SSP2, 0x01 ) fail ret = %d",
			__func__, ret);
		goto Exit_RawCapture;
	}
	cnt_timer = 0;

	/*Polling scan status*/
	while (1) {
		usleep(1000);
		cnt_timer++;
		RegData[0] = 0x03;
		elan_spi_transaction(RegData, 2, RegData, 2);
		if (RegData[0] & 0x04)
			break;

		if (cnt_timer > _RawCaptue_TIME_OUT_THD) {
			ret = SCAN_ERR;
			LOGE_SA("%s RegData = 0x%x, fail ret = %d",
				__func__, RegData[0], ret);
			goto Exit_RawCapture;
		}
	}

	/*Read image from fp sensor */
	DMA_WrapLoop = 4;
	DMA_Length = _imgbufsize / DMA_WrapLoop;

	for (i = 0; i < DMA_WrapLoop; i++) {
		memset((char *)tx_buf, 0, CONFIG_SPI_TX_BUF_SIZE);
		memset((char *)rx_buf, 0, CONFIG_SPI_RX_BUF_SIZE);
		tx_buf[0] = 0x10;
		ret = spi_transaction(&spi_devices[0], (char *)tx_buf,
			2, (char *)rx_buf, DMA_Length);
		memcpy(&imagebuffer[DMA_Length * i], (char *)rx_buf,
			DMA_Length);
	}

	/*Remove dummy byte*/
	for (image_index = 1; image_index < _imageWidth; image_index++)
		memcpy(&imagebuffer[_rawpixelsize*image_index],
			&imagebuffer[_radatasize*image_index], _rawpixelsize);

	for (index = 0; index < _imageTotalPixel; index++)
		pShortRaw[index] = (imagebuffer[index * 2] << 8) +
				imagebuffer[index * 2 + 1];


Exit_RawCapture:
	if (imagebuffer != NULL) {
		memset(imagebuffer, 0, sizeof(uint8_t) * _imgbufsize);
		shared_mem_release(imagebuffer);
	}

	if (ret != 0)
		LOGE_SA("%s error = %d", __func__, ret);

	return ret;
}

int ElanFP_ExcuteCalibration(void)
{
	int retry_time = 0;
	int ret = 0;

START_RE_CALIBRATION:
	elan_write_CMD(SRST);
	elan_write_CMD(FUSE_LOAD);
	RegisterInitialization();
	ElanFP_SENSINGMODE();

	ret = Calibration();

	if (ret != 0  && retry_time < _Rek_Times) {
		retry_time++;
		goto START_RE_CALIBRATION;
	}

	return ret;
}
