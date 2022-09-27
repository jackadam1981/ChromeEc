# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(app PRIVATE
    src/battery_cut_off.c
    src/get_panic_info.c
    src/get_pd_port_caps.c
    src/host_event_commands.c
    src/host_event_commands_deprecated.c
    src/keyboard_mkbp.c
    src/motion_sense.c
    src/pd_control.c
    src/pd_chip_info.c
    src/pd_log.c
    src/usb_pd_control.c
    src/usb_pd_host_cmd.c
)

target_sources_ifdef((CONFIG_HOSTCMD_FLASHPD AND CONFIG_USB_PD_TCPMV2) app PRIVATE src/usb_pd_host_cmd_hc_remote_flash.c)
