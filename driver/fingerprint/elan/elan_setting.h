/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Private sensor interface */

#ifndef _ELAN_SETTING_H
#define _ELAN_SETTING_H

#include <stdint.h>

#define VID							0x04F3
#define PID							0x0903
#define MID							0x01
#define VERSION						0x1007

#define CONFIG_SPI_TX_BUF_SIZE		1024
#define CONFIG_SPI_RX_BUF_SIZE		5120
#define gucIOIRQ					0x08

#define WRITE_REG_HEAD				0x80
#define READ_REG_HEAD               0x40
#define READ_SERIER_REG_HEAD		0xC0
#define START_SCAN					0x01
#define START_READ_IMAGE			0x10
#define SRST						0x31
#define FP_Page0					0x00
#define FP_Page1					0x01
#define FUSE_LOAD					0x04
#define REG_0x2C_Value				0xA0


#define eFSA515                     1
#define eFSA80SC                    2
#if defined(CONFIG_FP_SENSOR_ELAN80)
#define IC_Selection                          eFSA80SC
#elif defined(CONFIG_FP_SENSOR_ELAN515)
#define IC_Selection                          eFSA515
#endif

#if (IC_Selection == eFSA80SC)
#define _imageWidth                 80
#define _imageHeight                80
#elif (IC_Selection == eFSA515)
#define _imageWidth                 150
#define _imageHeight                52
#endif

#define FP_DUMMY_BYTE               2

#define raw_byts					2
#define _RawCaptue_TIME_OUT_THD 10000
#define _imageTotalPixel (_imageWidth*_imageHeight)
#define _rawpixelsize	(_imageHeight * raw_byts)
#define _radatasize	(_rawpixelsize + FP_DUMMY_BYTE)
#define _imgbufsize		(_radatasize * _imageWidth)

#define _CalibrationTarget                        3000
#define _LowBound_ComputeMean                     1000
#define _HighBound_ComputeMean                    10000
#define _BaseAverageTimes                         4
#define _FingerOnTHD                              750
#define _FingerDownTHD                            800
#define _isAlertEnrollingDuplicateEnable          1
#define _isSelfCheckEnrollingDuplicate            1
#define _SelfCheckEnrollingDuplicate_threshold    92
#define _isSameFingerRejectionEnable              1
#define _base_StdThreshold                        2100
#define VCB_REG						0x04

#define _Rek_Times					3
#define _REek_Mean					-300

#define _WOE_IDEL_TIME					0x0A
#define LOGE_SA(format, args...) cprints(CC_FP, format, ## args)

/**
 * set ELAN fingerprint sensor register initialization
 *
 * @return 0 on success.
 *         negative value on error.
 */
int RegisterInitialization(void);

/**
 * to calibrate ELAN fingerprint sensor and keep the calibration results
 * for correcting fingerprint image data
 *
 * @return 0 on success.
 *         negative value on error.
 */
int Calibration(void);

#endif /* _ELAN_SETTING_H */
