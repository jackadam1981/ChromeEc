#
# Copyright (c) 2020 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0
#

add_library(subsys__host_service INTERFACE)

zephyr_include_directories(include)
zephyr_include_directories_ifdef(CONFIG_HOST_SERVICE ../../include/host_service)
zephyr_sources_ifdef(CONFIG_HOST_SERVICE zephyr_host_service.c)
zephyr_sources_ifdef(CONFIG_SYS_MNG sys_mng.c)

add_subdirectory_ifdef(CONFIG_HECI heci)
