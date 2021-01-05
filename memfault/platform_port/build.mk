_this_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

MEMFAULT_COMPONENTS := util core

ifdef CONFIG_MEMFAULT_METRICS
MEMFAULT_COMPONENTS+=metrics
endif

ifdef CONFIG_MEMFAULT_PANICS
MEMFAULT_COMPONENTS+=panics
endif

MEMFAULT_SDK_ROOT := memfault/memfault

VPATH=memfault

include $(MEMFAULT_SDK_ROOT)/makefiles/MemfaultWorker.mk

memfault-$(CONFIG_MEMFAULT)=${MEMFAULT_COMPONENTS_SRCS:$(MEMFAULT_SDK_ROOT)/%.c=%.o}

includes-$(CONFIG_MEMFAULT)+=$(MEMFAULT_COMPONENTS_INC_FOLDERS)

dirs-$(CONFIG_MEMFAULT)+=memfault/components/core/src
dirs-$(CONFIG_MEMFAULT)+=memfault/components/util/src
ifdef CONFIG_MEMFAULT_PANICS
dirs-$(CONFIG_MEMFAULT)+=memfault/components/panics/src
dirs-$(CONFIG_MEMFAULT)+=memfault/ports/panics/src
endif
ifdef CONFIG_MEMFAULT_METRICS
dirs-$(CONFIG_MEMFAULT)+=memfault/components/metrics/src
endif

# Override some headers so we don't need to patch memfault
memfault_cflag_extras:=-I$(_this_dir)/include_overrides
# Set memfault fault handlers to override the weak handlers in cros EC
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_HARD_FAULT=hard_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_MEMORY_MANAGEMENT=mpu_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_BUS_FAULT=bus_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_USAGE_FAULT=usage_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_NMI=nmi_handler
memfault_cflag_extras+=-DMEMFAULT_DATA_SOURCE_RLE_ENABLED=1
memfault_cflag_extras+=-DMEMFAULT_PLATFORM_HAS_LOG_CONFIG=1
ifdef CONFIG_MEMFAULT_SAVE_LOG
memfault_cflag_extras+=-DMEMFAULT_SDK_LOG_SAVE_DISABLE=0
memfault_cflag_extras+=-DMEMFAULT_COREDUMP_COLLECT_LOG_REGIONS=1
else
memfault_cflag_extras+=-DMEMFAULT_SDK_LOG_SAVE_DISABLE=1
memfault_cflag_extras+=-DMEMFAULT_COREDUMP_COLLECT_LOG_REGIONS=0
endif
memfault_C_WARN=-Wstrict-prototypes -Wno-pointer-sign -Wno-unused-variable -Wno-unused-but-set-variable

$(out)/RW/memfault/%.o: override CFLAGS+=$(memfault_cflag_extras)
$(out)/RO/memfault/%.o: override CFLAGS+=$(memfault_cflag_extras)

$(out)/RW/memfault/%.o: override C_WARN=$(memfault_C_WARN)
$(out)/RO/memfault/%.o: override C_WARN=$(memfault_C_WARN)

dirs-$(CONFIG_MEMFAULT)+=memfault/platform_port

includes-$(CONFIG_MEMFAULT)+=$(_this_dir)

memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_core.o
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_device_info.o
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_commands.o
ifdef CONFIG_MEMFAULT_PANICS
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_coredump.o
endif
ifdef CONFIG_MEMFAULT_METRICS
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_metrics.o
endif