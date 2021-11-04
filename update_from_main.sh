#!/bin/bash
# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# The files we care about syncing to main.  For style, files we have
# in our tree but don't want to update are left in the list but
# commented out For style, files we have in our tree but don't want to
# update are left in the list but commented out.
UPDATE_FILES=(
    .checkpatch.conf
    .clang-format
    .dir-locals.el
    .gitignore
    .vscode/README.md
    .vscode/settings.json.default
    LICENSE
    #Makefile
    #Makefile.rules
    Makefile.toolchain
    #OWNERS
    #PRESUBMIT.cfg
    README.md
    board/arcada_ish/board.c
    board/arcada_ish/board.h
    board/arcada_ish/build.mk
    board/arcada_ish/ec.tasklist
    board/arcada_ish/gpio.inc
    board/drallion_ish/board.c
    board/drallion_ish/board.h
    board/drallion_ish/build.mk
    board/drallion_ish/ec.tasklist
    board/drallion_ish/gpio.inc
    builtin/assert.h
    builtin/endian.h
    builtin/inttypes.h
    builtin/limits.h
    builtin/math.h
    builtin/stdarg.h
    builtin/stdbool.h
    builtin/stddef.h
    builtin/stdint.h
    builtin/stdnoreturn.h
    builtin/string.h
    builtin/time.h
    chip/ish/aontaskfw/ipapg.S
    chip/ish/aontaskfw/ish_aon_defs.h
    chip/ish/aontaskfw/ish_aon_share.h
    chip/ish/aontaskfw/ish_aontask.c
    chip/ish/aontaskfw/ish_aontask.lds.S
    chip/ish/build.mk
    chip/ish/clock.c
    chip/ish/config_chip.h
    chip/ish/config_flash_layout.h
    chip/ish/dma.c
    chip/ish/flash.c
    chip/ish/gpio.c
    chip/ish/hbm.h
    chip/ish/heci.c
    chip/ish/heci_client.h
    chip/ish/hid_device.h
    chip/ish/hid_subsys.c
    chip/ish/host_command_heci.c
    chip/ish/hpet.h
    chip/ish/hwtimer.c
    chip/ish/i2c.c
    chip/ish/ipc_heci.c
    chip/ish/ipc_heci.h
    chip/ish/ish_dma.h
    chip/ish/ish_fwst.h
    chip/ish/ish_i2c.h
    chip/ish/ish_persistent_data.c
    chip/ish/ish_persistent_data.h
    chip/ish/power_mgt.c
    chip/ish/power_mgt.h
    chip/ish/registers.h
    chip/ish/system.c
    chip/ish/system_state.h
    chip/ish/system_state_subsys.c
    chip/ish/uart.c
    chip/ish/uart_defs.h
    chip/ish/util/pack_ec.py
    chip/ish/watchdog.c
    #common/build.mk
    common/chargen.c
    common/chipset.c
    common/console.c
    common/console_output.c
    common/ec_features.c
    common/firmware_image.S
    common/firmware_image.lds.S
    common/fpsensor/build.mk
    common/fpsensor/fpsensor_detect_strings.c
    common/gpio.c
    common/gpio_commands.c
    common/hooks.c
    common/host_command.c
    common/host_event_commands.c
    common/i2c_controller.c
    common/irq_locking.c
    common/kasa.c
    common/lid_switch.c
    common/mag_cal.c
    common/main.c
    common/mat33.c
    common/mat44.c
    common/math_util.c
    common/memory_commands.c
    common/mkbp_event.c
    common/motion_lid.c
    common/motion_sense.c
    common/motion_sense_fifo.c
    common/panic_output.c
    common/peripheral.c
    common/printf.c
    common/queue.c
    common/queue_policies.c
    common/shared_mem.c
    common/system.c
    common/tablet_mode.c
    common/timer.c
    common/uart_buffering.c
    common/uart_hostcmd.c
    common/uart_printf.c
    common/uptime.c
    common/usb_pd_flags.c
    common/util.c
    common/util_stdlib.c
    common/vec3.c
    common/version.c
    core/minute-ia/atomic.h
    core/minute-ia/build.mk
    core/minute-ia/config_core.h
    core/minute-ia/cpu.c
    core/minute-ia/cpu.h
    core/minute-ia/ec.lds.S
    core/minute-ia/ia_structs.h
    core/minute-ia/include/fpu.h
    core/minute-ia/init.S
    core/minute-ia/interrupts.c
    core/minute-ia/interrupts.h
    core/minute-ia/irq_handler.h
    core/minute-ia/irq_handler_common.S
    core/minute-ia/mia_panic_internal.h
    core/minute-ia/mpu.c
    core/minute-ia/panic.c
    core/minute-ia/switch.S
    core/minute-ia/task.c
    core/minute-ia/task_defs.h
    driver/accel_lis2dh.c
    driver/accel_lis2dh.h
    driver/accelgyro_lsm6dsm.c
    driver/accelgyro_lsm6dsm.h
    #driver/build.mk
    driver/mag_lis2mdl.c
    driver/mag_lis2mdl.h
    driver/sensorhub_lsm6dsm.h
    driver/stm_mems_common.c
    driver/stm_mems_common.h
    include/2id.h
    include/accelgyro.h
    include/acpi.h
    include/ap_hang_detect.h
    include/battery.h
    include/battery_smart.h
    include/board_config.h
    include/body_detection.h
    include/button.h
    include/charge_manager.h
    include/charge_state.h
    include/charge_state_v2.h
    include/charger.h
    include/chipset.h
    include/clock.h
    include/common.h
    include/compile_time_macros.h
    include/compiler.h
    #include/config.h
    include/console.h
    include/console_channel.inc
    include/consumer.h
    include/crc8.h
    include/cros_board_info.h
    include/cros_version.h
    include/dma.h
    include/driver/mag_bmm150.h
    include/ec_commands.h
    include/ec_ec_comm_client.h
    include/eeprom.h
    include/extpower.h
    include/flash.h
    include/fpsensor_detect.h
    include/gesture.h
    include/gpio.h
    include/gpio.wrap
    include/gpio_list.h
    include/gpio_signal.h
    include/hooks.h
    include/host_command.h
    include/host_command_heci.h
    include/hwtimer.h
    include/i2c.h
    include/i2c_bitbang.h
    include/i2c_private.h
    include/ioexpander.h
    include/kasa.h
    include/keyboard_config.h
    include/keyboard_scan.h
    include/lid_angle.h
    include/lid_switch.h
    include/lightbar.h
    include/lightbar_msg_list.h
    include/link_defs.h
    include/lpc.h
    include/mag_cal.h
    include/mat33.h
    include/mat44.h
    include/math_util.h
    include/mkbp_event.h
    include/module_id.h
    include/motion_lid.h
    include/motion_orientation.h
    include/motion_sense.h
    include/motion_sense_fifo.h
    include/ocpc.h
    include/online_calibration.h
    include/otp.h
    include/panic.h
    include/power.h
    include/printf.h
    include/producer.h
    include/queue.h
    include/queue_policies.h
    include/reset_flag_desc.inc
    include/rollback.h
    include/rsa.h
    include/rwsig.h
    include/sha256.h
    include/shared_mem.h
    include/software_panic.h
    include/spi.h
    include/spi_flash.h
    include/stack_trace.h
    include/sysjump.h
    include/system.h
    include/tablet_mode.h
    include/task.h
    include/task_filter.h
    include/task_id.h
    include/test_util.h
    include/timer.h
    include/uart.h
    include/usb_console.h
    include/usb_pd.h
    include/usb_pd_flags.h
    include/usb_pd_tbt.h
    include/usb_pd_tcpm.h
    include/usb_pd_vdo.h
    include/util.h
    include/vb21_struct.h
    include/vboot.h
    include/vec3.h
    include/vec4.h
    include/virtual_battery.h
    include/watchdog.h
    pylintrc
    unblocked_terms.txt
    util/getversion.sh
)

git checkout cros/main -- "${UPDATE_FILES[@]}"

if [[ -z "$(git status --porcelain)" ]]; then
    echo "Branch is up-to-date.  Done." >&2
    exit 0
fi

# Make sure things still build.
rm -rf build
make BOARD=arcada_ish
make BOARD=drallion_ish

git add .
git commit -s -m "ish: Sync branch with main

Run update_from_main.sh to sync affected files with main (currently
at commit $(git rev-parse cros/main)).

BUG=none
BRANCH=ish
TEST=make BOARD=arcada_ish
TEST=make BOARD=drallion_ish
TEST=<< PLEASE ALSO DESCRIBE MANUAL TESTING PERFORMED >>
"

# Get the user to edit the commit message.
git commit --amend
