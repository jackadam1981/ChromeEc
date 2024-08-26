#include "pw_unit_test/framework.h"
#include "pw_unit_test/logging_event_handler.h"

#include "cros_board_info.h"
#include "flash.h"

// #include <zephyr/fff.h>
#include <zephyr/kernel.h>

// DEFINE_FFF_GLOBALS;
// DEFINE_FAKE_VALUE_FUNC(int, crec_flash_unprotected_read, int, int, char *);
namespace
{
// constexpr const struct device *kCrosFlashDev =
// 	DEVICE_DT_GET(DT_CHOSEN_cros_ec_flash_controller);
} // namespace

int main()
{
  struct cbi_header header = {
    .magic = { 'C', 'B', 'I'},
    .crc = 0,
    .major_version = CBI_VERSION_MAJOR,
    .minor_version = CBI_VERSION_MINOR,
    .total_size = sizeof(struct cbi_header),
  };
  header.crc = cbi_crc8(&header);
	crec_flash_physical_write(0x3f000, sizeof(header), reinterpret_cast<const char*>(&header));
  testing::InitGoogleTest(nullptr, nullptr);
	pw::unit_test::LoggingEventHandler handler;
	pw::unit_test::RegisterEventHandler(&handler);
  printk("Running tests...\n");
	return RUN_ALL_TESTS();
}
