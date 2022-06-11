/* Copyright 2022 The Chromium OS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * See more information about this code provenance below.
 */
/*********************************************************************

MODULE NAME:    b64.c

AUTHOR:         Bob Trower 08/04/01

PROJECT:        Crypt Data Packaging

COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001

NOTES:          This source code may be used as you wish, subject to
		the MIT license.  See the LICENCE section below.

		Canonical source should be at:
		    http://base64.sourceforge.net

DESCRIPTION:
		This little utility implements the Base64
		Content-Transfer-Encoding standard described in
		RFC1113 (http://www.faqs.org/rfcs/rfc1113.html).

		This is the coding scheme used by MIME to allow
		binary data to be transferred by SMTP mail.

		Groups of 3 bytes from a binary stream are coded as
		groups of 4 bytes in a text stream.

		The input stream is 'padded' with zeros to create
		an input that is an even multiple of 3.

		A special character ('=') is used to denote padding so
		that the stream can be decoded back to its exact size.

		Encoded output is formatted in lines which should
		be a maximum of 72 characters to conform to the
		specification.  This program sets the length to
		64 characters.

		Example encoding:

		The stream 'ABCD' is 32 bits long.  It is mapped as
		follows:

		ABCD

		 A (65)     B (66)     C (67)     D (68)   (None) (None)
		01000001   01000010   01000011   01000100

		16 (Q)  20 (U)  9 (J)   3 (D)    17 (R) 0 (A)  NA (=) NA (=)
		010000  010100  001001  000011   010001 000000 000000 000000


		QUJDRA==

DEPENDENCIES:   None

LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

		Permission is hereby granted, free of charge, to any person
		obtaining a copy of this software and associated
		documentation files (the "Software"), to deal in the
		Software without restriction, including without limitation
		the rights to use, copy, modify, merge, publish, distribute,
		sublicense, and/or sell copies of the Software, and to
		permit persons to whom the Software is furnished to do so,
		subject to the following conditions:

		The above copyright notice and this permission notice shall
		be included in all copies or substantial portions of the
		Software.

		THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
		KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
		WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
		PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
		OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
		OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
		OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
		SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

VERSION HISTORY:
		Bob Trower 08/04/01 -- Create Version 0.00.00B
		Bob Trower 08/17/01 -- Correct documentation, messages.
				    -- Correct help for linesize syntax.
				    -- Force error on too many arguments.
		Bob Trower 08/19/01 -- Add sourceforge.net reference to
				       help screen prior to release.
		Bob Trower 10/22/04 -- Cosmetics for package/release.
		Bob Trower 02/28/08 -- More Cosmetics for package/release.
		Bob Trower 02/14/11 -- Cast altered to fix warning in VS6.
       ChromiumOS authors  06/10/22 -- Trimmed down to support encoding
				       from memory only.
*********************************************************************/
#include "common.h"
#include "console.h"
#include "b64.h"

/*
** Translation Table as described in RFC1113
*/
static const char cb64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
** encodeblock
**
** encode 3 8-bit binary bytes as 4 '6-bit' characters
*/
static void encodeblock(uint8_t *in, uint8_t *out, int len)
{
	out[0] = (uint8_t)cb64[(int)(in[0] >> 2)];
	out[1] = (uint8_t)
		cb64[(int)(((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4))];
	out[2] = (uint8_t)(len > 1 ? cb64[(int)(((in[1] & 0x0f) << 2) |
						((in[2] & 0xc0) >> 6))] :
					   '=');
	out[3] = (uint8_t)(len > 2 ? cb64[(int)(in[2] & 0x3f)] : '=');
}

static void printit(char c, void (*f)(char c))
{
	if (f)
		f(c);
	else
		ccprintf("%c", c);
}

#define LINE_SIZE 64
void base64_encode_to_console(const uint8_t *data, size_t size,
			      void (*func)(char c))
{
	uint8_t in[3];
	uint8_t out[4];
	int j, blocksout;
	size_t togo;

	togo = size;
	j = 0;
	blocksout = 0;
	while (togo) {
		int i;
		int in_length;

		for (i = 0; (i < sizeof(in)) && togo; i++, togo--)
			in[i] = data[j++];
		in_length = i;
		while (i < sizeof(in))
			in[i++] = 0;

		encodeblock(in, out, in_length);
		for (i = 0; i < sizeof(out); i++)
			printit(out[i], func);

		blocksout++;

		if (blocksout >= (LINE_SIZE / 4)) {
			printit('\n', func);
			blocksout = 0;
			if (!func)
				cflush();
		}
	}
	if (blocksout)
		printit('\n', func);
}
