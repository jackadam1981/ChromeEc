/*
 * Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The file lib_crc.c contains the private and public functions used for
 * the calculation of CRC-16, CRC-CCITT and CRC-32 cyclic redundancy values.
 */

#define LIB_CRC_C

#include "lib_crc.h"

/*********************************************************************
 *                                                                   *
 *   Library         : lib_crc                                       *
 *   File            : lib_crc.c                                     *
 *   Author          : Lammert Bies  1999-2005                       *
 *   E-mail          : info@lammertbies.nl                           *
 *   Language        : ANSI C                                        *
 *                                                                   *
 *                                                                   *
 *   Description                                                     *
 *   ===========                                                     *
 *                                                                   *
 *   The file lib_crc.c contains the private  and  public  func-     *
 *   tions  used  for  the  calculation of CRC-16, CRC-CCITT and     *
 *   CRC-32 cyclic redundancy values.                                *
 *                                                                   *
 *                                                                   *
 *   Dependencies                                                    *
 *   ============                                                    *
 *                                                                   *
 *   lib_crc.h       CRC definitions and prototypes                  *
 *                                                                   *
 *                                                                   *
 *   Modification history                                            *
 *   ====================                                            *
 *                                                                   *
 *   Date        Version Comment                                     *
 *                                                                   *
 *   2005-05-14  1.12    Added CRC-CCITT with start value 0          *
 *                                                                   *
 *   2005-02-05  1.11    Fixed bug in CRC-DNP routine                *
 *                                                                   *
 *   2005-02-04  1.10    Added CRC-DNP routines                      *
 *                                                                   *
 *   1999-02-21  1.01    Added FALSE and TRUE mnemonics              *
 *                                                                   *
 *   1999-01-22  1.00    Initial source                              *
 *                                                                   *
 *********************************************************************
 */

/*********************************************************************
 *                                                                   *
 *   #define P_xxxx                                                  *
 *                                                                   *
 *   The CRC's are computed using polynomials. The  coefficients     *
 *   for the algorithms are defined by the following constants.      *
 *                                                                   *
 *********************************************************************
 */

#define P_16 0xA001

/*********************************************************************
 *                                                                   *
 *   static void crc...tab_val();                                    *
 *                                                                   *
 *   Three local functions are used  to  initialize  the  tables     *
 *   with values for the algorithm.                                  *
 *                                                                   *
 *********************************************************************
 */
static unsigned short crc16_tab_val(unsigned short);

/*********************************************************************
 *                                                                   *
 *   unsigned short update_crc( unsigned long crc, char c );         *
 *                                                                   *
 *   The function update_crc calculates a  new  CRC-16  value        *
 *   based  on  the  previous value of the CRC and the next byte     *
 *   of the data to be checked.                                      *
 *                                                                   *
 *********************************************************************
 */
unsigned short update_crc(unsigned short crc, char c)
{
	unsigned short tmp, short_c;

	short_c = 0x00ff & (unsigned short)c;
	tmp = crc ^ short_c;
	crc = (crc >> 8) ^ crc16_tab_val(tmp & 0xff);

	return crc;

} /* update_crc */

/*********************************************************************
 *                                                                   *
 *   static void crc16_tab_val( unsigned short );                    *
 *                                                                   *
 *   The function crc16_tab_val() is used  to  fill  the  array      *
 *   for calculation of the CRC-16 with values.                      *
 *                                                                   *
 *********************************************************************
 */
static unsigned short crc16_tab_val(unsigned short c)
{
	int j;
	unsigned short crc;

	crc = 0;

	for (j = 0; j < 8; j++) {
		if ((crc ^ c) & 0x0001)
			crc = (crc >> 1) ^ P_16;
		else
			crc = crc >> 1;

		c = c >> 1;
	}

	return crc;

} /* crc16_tab_val */

#undef LIB_CRC_C
