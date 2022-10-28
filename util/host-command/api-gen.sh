#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -u

host_commands_pr=(
  adc_read
  battery_vendor_param
  cec_get
  charge_state
  chargesplash
  device_event
  fp_mode
  get_cmd_versions
  get_pd_port_caps
  hello
  hibernation_delay
  host_event
  i2c_control
  locate_chip
  pd_chip_info
  pwm_get_duty
  regulator_get_voltage
  regulator_is_enabled
  rgbkbd
  switch_enable_wireless_v1
  temp_sensor_get_info
  thermal_get_threshold
  tmp006_get_raw
  typec_status
  usb_pd_control
  usb_pd_mux_info
  usb_pd_power_info
  vboot_hash
)

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
  mkbp_simulate_key
  pd_control
  pd_write_log_entry
  pstore_write
  pwm_set_duty
  pwm_set_fan_duty_v0
  pwm_set_keyboard_backlight
  reboot_ec
  regulator_enable
  regulator_set_voltage
  rwsig_action
  set_base_state
  thermal_set_threshold
  thermal_set_threshold_v1
  typec_control
  usb_charge_set_mode
  usb_mux
  usb_pd_dps_control
  usb_pd_mux_ack
)

host_commands_r=(
  flash_info
  flash_spi_info
  fp_stats
  get_chip_info
  get_features
  get_next_event
  get_protocol_info
  get_version
  get_version_v1
  keyboard_factory_test
  mkbp_info
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

host_commands=(
  ap_reset
  console_snapshot
  reboot
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
