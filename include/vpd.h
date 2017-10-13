#include "common.h"

struct vpd {
	/* Version of the VPD */
	uint32_t vpd_version;
	/* Board version: e.g. EVT1, DVT2 */
	char brd[4];
	/* OEM name */
	char oem[4];
	/* SKU ID */
	char sku[4];
	/* Check sum for the data above */
	uint8_t crc;
	/* For 32-bit alignment */
	uint8_t reserved[3];
	/* Data used by AP */
	uint8_t ap_blob[0];
};

extern const struct vpd *vpd_copy;

void vpd_init(void);
