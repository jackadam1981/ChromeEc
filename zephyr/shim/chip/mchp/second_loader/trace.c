/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "trace.h"

void trace_init(void)
{
	ser_init();
}

void _trace_puts(char *buf, unsigned int len)
{
	char *t = buf;

	while (len--)
		send_dbg_char(*t++);
}

static unsigned int mini_strlen(const char *s)
{
	unsigned int len = 0;

	while (s[len] != '\0')
		len++;
	return len;
}

static unsigned int mini_itoa(int value, unsigned int radix,
			      unsigned int uppercase, unsigned int unsig,
			      char *buffer, unsigned int zero_pad)
{
	char *pbuffer = buffer;
	int negative = 0;
	unsigned int i, len;

	/* No support for unusual radixes. */
	if (radix > 16)
		return 0;

	if (value < 0 && !unsig) {
		negative = 1;
		value = -value;
	}

	/* This builds the string back to front ... */
	do {
		int digit = value % radix;
		*(pbuffer++) = (digit < 10 ?
					'0' + digit :
					(uppercase ? 'A' : 'a') + digit - 10);
		value /= radix;
	} while (value > 0);

	for (i = (pbuffer - buffer); i < zero_pad; i++)
		*(pbuffer++) = '0';

	if (negative)
		*(pbuffer++) = '-';

	*(pbuffer) = '\0';

	/*
	 * Now we reverse it (could do it recursively but will
	 * conserve the stack space)
	 */
	len = (pbuffer - buffer);
	for (i = 0; i < len / 2; i++) {
		char j = buffer[i];

		buffer[i] = buffer[len - i - 1];
		buffer[len - i - 1] = j;
	}

	return len;
}

int mini_vsnprintf(const char *fmt, va_list va)
{
	char bf[24];
	char ch;

	while ((ch = *(fmt++))) {
		if (ch != '%')
			send_dbg_char(ch);
		else { /* format string */
			char zero_pad = 0;
			char *ptr;
			unsigned int len;

			ch = *(fmt++);

			/* Zero padding requested */
			if (ch == '0') {
				ch = *(fmt++);
				if (ch == '\0')
					goto end;
				if (ch >= '0' && ch <= '9')
					zero_pad = ch - '0';
				ch = *(fmt++);
			}

			switch (ch) {
			case 0:
				goto end;

			case 'u':
			case 'd':
				len = mini_itoa(va_arg(va, unsigned int), 10, 0,
						(ch == 'u'), bf, zero_pad);
				_trace_puts(bf, len);
				break;

			case 'x':
			case 'X':
				len = mini_itoa(va_arg(va, unsigned int), 16,
						(ch == 'X'), 1, bf, zero_pad);
				_trace_puts(bf, len);
				break;

			case 'c':
				send_dbg_char((char)(va_arg(va, int)));
				break;

			case 's':
				ptr = va_arg(va, char *);
				_trace_puts(ptr, mini_strlen(ptr));
				break;

			default:
				send_dbg_char(ch);
				break;
			}
		}
	}
end:
	return 0;
}

#if CONFIG_ENABLE_TRACE
void tracex(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	mini_vsnprintf(fmt, va);
	va_end(va);
}
#else
void tracex(const char *fmt, ...)
{
}
#endif
