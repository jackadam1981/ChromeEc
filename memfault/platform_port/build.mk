_this_dir:=$(dir $(lastword $(MAKEFILE_LIST)))

MEMFAULT_COMPONENTS := util core panics metrics
MEMFAULT_SDK_ROOT := memfault/memfault

VPATH=memfault

include $(MEMFAULT_SDK_ROOT)/makefiles/MemfaultWorker.mk

memfault-$(CONFIG_MEMFAULT)=${MEMFAULT_COMPONENTS_SRCS:$(MEMFAULT_SDK_ROOT)/%.c=%.o}
# memfault-$(CONFIG_MEMFAULT)+=ports/panics/src/memfault_platform_ram_backed_coredump.o

includes-$(CONFIG_MEMFAULT)+=$(MEMFAULT_COMPONENTS_INC_FOLDERS)

dirs-$(CONFIG_MEMFAULT)+=memfault/components/core/src
dirs-$(CONFIG_MEMFAULT)+=memfault/components/util/src
dirs-$(CONFIG_MEMFAULT)+=memfault/components/panics/src
dirs-$(CONFIG_MEMFAULT)+=memfault/components/metrics/src
dirs-$(CONFIG_MEMFAULT)+=memfault/components/demo/src
dirs-$(CONFIG_MEMFAULT)+=memfault/components/demo/src/panics
dirs-$(CONFIG_MEMFAULT)+=memfault/ports/panics/src

# Override some headers so we don't need to patch memfault
memfault_cflag_extras:=-I$(_this_dir)/include_overrides
# Set memfault fault handlers to override the weak handlers in cros EC
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_HARD_FAULT=hard_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_MEMORY_MANAGEMENT=mpu_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_BUS_FAULT=bus_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_USAGE_FAULT=usage_fault_handler
memfault_cflag_extras+=-DMEMFAULT_EXC_HANDLER_NMI=nmi_handler
memfault_cflag_extras+=-DMEMFAULT_COREDUMP_COLLECT_LOG_REGIONS=1
memfault_C_WARN=-Wstrict-prototypes -Wno-pointer-sign

$(out)/RW/memfault/%.o: override CFLAGS+=$(memfault_cflag_extras)
$(out)/RO/memfault/%.o: override CFLAGS+=$(memfault_cflag_extras)

$(out)/RW/memfault/%.o: override C_WARN=$(memfault_C_WARN)
$(out)/RO/memfault/%.o: override C_WARN=$(memfault_C_WARN)

dirs-$(CONFIG_MEMFAULT)+=memfault/platform_port

includes-$(CONFIG_MEMFAULT)+=$(_this_dir)

memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_core.o
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_device_info.o
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_log.o
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_commands.o
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_coredump.o
memfault-$(CONFIG_MEMFAULT)+=platform_port/memfault_platform_metrics.o
