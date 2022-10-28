#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -u

#
# This script generates a list of function declarations for EC host
# command. The output is suitable for inclusion in the
# include/ec_cmd_api.h file.
#
# Host command names can have an optional "_%u" suffix to generate
# versioned host command declarations.
#
# As a simple example, the "hello" host command generates this declaration:
#
# static inline int ec_cmd_hello(
#         CROS_EC_COMMAND_INFO *h,
#         const struct ec_params_hello *p,
#         struct ec_response_hello *r)
# {
#         return CROS_EC_COMMAND(h, EC_CMD_HELLO, 0,
#                                p, sizeof(*p),
#                                r, sizeof(*r));
# }
#
# where CROS_EC_COMMAND_INFO and CROS_EC_COMMAND need to be provided by
# the environment where these declarations are used and other
# declarations are provided by "include/ec_commands.h".
#

# Host commands that have parameter data and response data

host_commands_pr=(
  adc_read
  battery_vendor_param
  cec_get
  chargesplash
  charge_state
  device_event
  flash_protect
  fp_mode
  get_cmd_versions
  get_pd_port_caps
  gpio_get
  gpio_get_v1
  hello
  hibernation_delay
  host_event
  host_sleep_event_v1
  i2c_control
  i2c_passthru_protect
  locate_chip
  mkbp_info
  pd_chip_info
  pwm_get_duty
  regulator_get_voltage
  regulator_is_enabled
  rgbkbd
  smart_discharge
  switch_enable_wireless_v1
  temp_sensor_get_info
  test_protocol
  thermal_get_threshold
  tmp006_get_raw
  typec_discovery
  typec_status
  typec_vdm_response
  usb_pd_control
  usb_pd_mux_info
  usb_pd_power_info
  vboot_hash
  vstore_read
)

# Host commands that only have parameter data

host_commands_p=(
  button
  cec_set
  config_power_button
  efs_verify
  external_power_limit_v1
  flash_erase
  force_lid_open
  fp_seed
  gpio_set
  hang_detect
  host_sleep_event
  mkbp_set_config
  mkbp_simulate_key
  pd_control
  pd_write_log_entry
  pstore_write
  pwm_set_duty
  pwm_set_fan_duty_v0
  pwm_set_keyboard_backlight
  reboot_ap_on_g3_v1
  reboot_ec
  regulator_enable
  regulator_set_voltage
  rwsig_action
  set_base_state
  set_tablet_mode
  thermal_set_threshold
  thermal_set_threshold_v1
  typec_control
  usb_charge_set_mode
  usb_mux
  usb_pd_dps_control
  usb_pd_mux_ack
  usb_pd_rw_hash_entry
  vstore_write
)

# Host commands that only have response data

host_commands_r=(
  charge_port_count
  display_soc
  flash_info
  flash_spi_info
  fp_stats
  get_boot_time
  get_chip_info
  get_comms_status
  get_features
  get_next_event
  get_protocol_info
  get_version
  get_version_v1
  keyboard_factory_test
  mkbp_get_config
  pchg_count
  port80_last_boot
  power_info_v1
  pstore_info
  pwm_get_keyboard_backlight
  rollback_info
  rwsig_check_status
  rwsig_info
  sysinfo
  usb_pd_ports
)

# Host commands that have no parameter or response data

host_commands=(
  ap_reset
  battery_cut_off
  console_snapshot
  reboot
  reboot_ap_on_g3
  thermal_auto_fan_ctrl
  tp_frame_snapshot
  tp_self_test
)

# shellcheck disable=SC2016
api_template_pr='
static inline int ec_cmd_${cmd}(
	CROS_EC_COMMAND_INFO *h,
	const struct ec_params_${cmd} *p,
	struct ec_response_${cmd} *r)
{
	return CROS_EC_COMMAND(h, EC_CMD_${upper}, ${version},
			       p, sizeof(*p),
			       r, sizeof(*r));
}
'

# shellcheck disable=SC2016
api_template_p='
static inline int ec_cmd_${cmd}(
	CROS_EC_COMMAND_INFO *h,
	const struct ec_params_${cmd} *p)
{
	return CROS_EC_COMMAND(h, EC_CMD_${upper}, ${version},
			       p, sizeof(*p),
			       NULL, 0);
}
'

# shellcheck disable=SC2016
api_template_r='
static inline int ec_cmd_${cmd}(
	CROS_EC_COMMAND_INFO *h,
	struct ec_response_${cmd} *r)
{
	return CROS_EC_COMMAND(h, EC_CMD_${upper}, ${version},
			       NULL, 0,
			       r, sizeof(*r));
}
'

# shellcheck disable=SC2016
api_template='
static inline int ec_cmd_${cmd}(CROS_EC_COMMAND_INFO *h)
{
	return CROS_EC_COMMAND(h, EC_CMD_${upper}, ${version},
			       NULL, 0,
			       NULL, 0);
}
'

# $1: template to use
# $cmd: name of function

emit_f()
{
  local upper
  local stem
  local template=$1
  local version=0
  local v_string

  stem="${cmd%_v[0-9]}"
  # shellcheck disable=SC2034
  upper="${stem@U}"

  v_string="${cmd#"${stem}"}"
  if [[ -n "${v_string}" ]]; then
    # shellcheck disable=SC2034
    version="${v_string#_v}"
  fi

  eval echo -n \""${template}"\"
}

main()
{
  local cmd

  for cmd in "${host_commands_pr[@]}"
  do
    emit_f "${api_template_pr}"
  done

  for cmd in "${host_commands_p[@]}"
  do
    emit_f "${api_template_p}"
  done

  for cmd in "${host_commands_r[@]}"
  do
    emit_f "${api_template_r}"
  done

  for cmd in "${host_commands[@]}"
  do
    emit_f "${api_template}"
  done
}

main "$@"
