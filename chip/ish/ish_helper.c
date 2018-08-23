#include "util.h"

#define RSIZE_MAX 0x7FFFFFF

void memcpy_s(void *dest, uint32_t dmax, const void *src, uint32_t len)
{
	if (dest == NULL || dmax > RSIZE_MAX) {
		assert(0);
		return;
	}

	if (src == NULL || len > dmax) {
		memset(dest, 0, dmax);
		assert(0);
		return;
	}
	memcpy(dest, src, len);
}
