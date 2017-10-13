#include "common.h"
#include "console.h"
#include "crc8.h"
#include "hooks.h"
#include "system.h"
#include "util.h"
#include "vpd.h"

#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

#define VPD_VERSION		0
#define SYSJUMP_TAG_VPD		0xec

#ifdef SECTION_IS_RO
const struct vpd ro_vpd __keep __attribute__((section(".google"))) = {
	.vpd_version = 0,
	.brd = {'N', 'O', 'N', 'E'},
	.oem = {'N', 'O', 'N', 'E'},
	.sku = {'N', 'O', 'N', 'E'},
	.crc = 0,
};
#endif

const struct vpd *vpd_copy = 0;

static int check_crc(void)
{
	uint8_t crc;
	if (!vpd_copy)
		return EC_ERROR_INVAL;
	crc = crc8((const uint8_t*)vpd_copy, sizeof(*vpd_copy) - sizeof(crc));
	return crc == vpd_copy->crc ? EC_SUCCESS : EC_ERROR_INVAL;
}

int vpd_is_valid(void)
{
	return check_crc();
}

#ifdef SECTION_IS_RO
static void store_vpd(void)
{
	if (system_add_jump_tag(SYSJUMP_TAG_VPD, 0, sizeof(ro_vpd), &ro_vpd))
		CPRINTS("Failed to store VPD");
}
DECLARE_HOOK(HOOK_SYSJUMP, store_vpd, HOOK_PRIO_DEFAULT);
#endif

void vpd_init(void)
{
#ifdef SECTION_IS_RO
	vpd_copy = &ro_vpd;
#else
	int size;
	vpd_copy = (const void*)system_get_jump_tag(SYSJUMP_TAG_VPD, 0, &size);
	if (!vpd_copy || size != sizeof(*vpd_copy)) {
		CPRINTS("Failed to load VPD");
		return;
	}
#endif
	if (vpd_copy->vpd_version < VPD_VERSION || check_crc())
		CPRINTS("Invalid VPD");
}

static int command_vpd(int argc, char **argv)
{
	char buf[5] = { 0 };

	ccprintf("VPD version: %d\n", vpd_copy->vpd_version);
	memcpy(buf, vpd_copy->brd, sizeof(vpd_copy->brd));
	ccprintf("BRD: '%s'\n", buf);
	memcpy(buf, vpd_copy->oem, sizeof(vpd_copy->oem));
	ccprintf("OEM: '%s'\n", buf);
	memcpy(buf, vpd_copy->sku, sizeof(vpd_copy->sku));
	ccprintf("SKU: '%s'\n", buf);
	ccprintf("CRC: %s\n", check_crc() ? "bad" : "good");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(vpd, command_vpd, "vpd", "Dump VPD");
