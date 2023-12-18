#include <zephyr/linker/iterable_sections.h>

#if defined(CONFIG_PLATFORM_EC_HOSTCMD) && !defined(CONFIG_EC_HOST_CMD)
ITERABLE_SECTION_ROM(host_command, 4)
#endif

#ifdef CONFIG_PLATFORM_EC_MKBP_EVENT
ITERABLE_SECTION_ROM(mkbp_event_source, 4)
#endif

#ifdef CONFIG_PLATFORM_EC_HOOKS
#include "../shim/include/hook_types.h"
#define HOOK_TYPE_SECTION(type) \
	ITERABLE_SECTION_ROM(zephyr_shim_hook_##type, 4)
FOR_EACH(HOOK_TYPE_SECTION, (
), HOOK_TYPES_LIST)
#endif

ITERABLE_SECTION_ROM(state_cache_uint8_t_tag, 1)
ITERABLE_SECTION_ROM(state_cache_uint16_t_tag, 1)
ITERABLE_SECTION_ROM(state_cache_uint32_t_tag, 1)
ITERABLE_SECTION_ROM(state_cache_uint64_t_tag, 1)

ITERABLE_SECTION_ROM(state_cache_uint8_t_ptr_tag, 1)
ITERABLE_SECTION_ROM(state_cache_uint16_t_ptr_tag, 1)
ITERABLE_SECTION_ROM(state_cache_uint32_t_ptr_tag, 1)
ITERABLE_SECTION_ROM(state_cache_uint64_t_ptr_tag, 1)
