#!/bin/bash
#
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

board_name=$1
out_dir=$2

SNK=0
SRC=3
DRP=4
IDH_PTYPE_UNDEF=0
IDH_PTYPE_HUB=1
IDH_PTYPE_PERIPH=2
IDH_PTYPE_AMA=5

usb_pd_policy="board/$board_name/usb_pd_policy.c"
board_h="board/$board_name/board.h"
config="$out_dir/.config"
vif_dir="$out_dir/vif"
usb_descriptor_h="include/usb_descriptor.h"
usb_pd_h="include/usb_pd.h"

# VIF Header values
vif_specification="Revision 0.54, Version 1.0"
vif_producer="genvif.sh"
vendor_name="Google"

# 0 - Revision 1.0
# 1 - Revision 2.0
# 2 - Revision 3.0
pd_spec_rev="1"
version_info=""
src_pdos=()
snk_pdos=()

#######################################
# Check if VIFs should be created.
#
# Arguments:
#  none
# Returns:
#   0 if .config file exists, board.h, vif dir exist and
#        CONFIG_USB_POWER_DELIVERY is defined
#   else 1
########################################
check_for_pd() {
  # Make sure the board .config exists
  if [ ! -e "$config" ]; then
    return 1
  fi

  # Make sure board.h exists
  if [ ! -e "$board_h" ]; then
    return 1
  fi

  # Make sure we have a valid usb_pd_policy file
  if [ ! -e "$usb_pd_policy" ]; then
    return 1
  fi

  # Make sure we have a valid vif dir
  if [ ! -d "$vif_dir" ]; then
    return 1
  fi

  # Make sure USB POWER DELIVERY is enabled
  usb_pd=($(grep "CONFIG_USB_POWER_DELIVERY=y" $config))
  if [ ${#usb_pd[@]} -eq 0 ]; then
    return 1
  fi

  return 0
}

#######################################
# Check if Dual Role Power is enabled
#
# Arguments:
#  none
# Returns:
#   0 if Dual Role Power is enabled
#   else 1
########################################
is_drp() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))

  if grep -q "PDO_FIXED_DUAL_ROLE" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if UUT is externally powered
#
# Arguments:
#  none
# Returns:
#   0 if externally powered
#   else 1
########################################
is_extpwr() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_EXTERNAL" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if TrySrc capable
#
# Arguments:
#  none
# Returns:
#   0 if TrySrc capable
#   else 1
########################################
can_try_src() {
  local try
  try=($(grep "CONFIG_USB_PD_TRY_SRC=y" $config))
  if [ ${#try[@]} -eq 0 ]; then
    return 1
  fi

  return 0
}

#######################################
# Check if GiveBack capable
#
# Arguments:
#  none
# Returns:
#   0 if GiveBack capable
#   else 1
########################################
can_giveback() {
  local gb
  gb=($(grep "CONFIG_USB_PD_GIVE_BACK=y" $config))
  if [ ${#gb[@]} -eq 0 ]; then
    return 1
  fi

  return 0
}

#######################################
# Check if VCONN Swap capable
#
# Arguments:
#  none
# Returns:
#   0 if VCONN Swap capable
#   else 1
########################################
is_vconn_swap() {
  local vs
  vs=($(grep "CONFIG_USBC_VCONN_SWAP=y" $config))
  if [ ${#vs[@]} -eq 0 ]; then
    return 1
  fi

  return 0
}

#######################################
# Check if USB Comms Capable
#
# Arguments:
#  none
# Returns:
#   0 if USB Comms Capable
#   else 1
########################################
is_usb_comms_cap() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_COMM_CAP" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if DR Swap to DFP Supported
#
# Arguments:
#  none
# Returns:
#   0 if DR Swap to DFP Supported
#   else 1
########################################
is_dr_swap_to_dfp_sup() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_DATA_SWAP" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if DR Swap to UFP Supported
#
# Arguments:
#  none
# Returns:
#   0 if DR Swap to UFP Supported
#   else 1
########################################
is_dr_swap_to_ufp_sup() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_DATA_SWAP" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if Accepts PR Swap as SRC
#
# Arguments:
#  none
# Returns:
#   0 if Accepts PR Swap as SRC
#   else 1
########################################
is_accepts_pr_swap_as_src() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_DUAL_ROLE" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if Accepts PR Swap as SNK
#
# Arguments:
#  none
# Returns:
#   0 if Accepts PR Swap as SNK
#   else 1
########################################
is_accepts_pr_swap_as_snk() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_DUAL_ROLE" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if Request PR Swap as SRC
#
# Arguments:
#  none
# Returns:
#   0 if Request PR Swap as SRC
#   else 1
########################################
is_request_pr_swap_as_src() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_DUAL_ROLE" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if Request PR Swap as SNK
#
# Arguments:
#  none
# Returns:
#   0 if Request PR Swap as SNK
#   else 1
########################################
is_request_pr_swap_as_snk() {
  local fixed_flags
  IFS=$''
  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if grep -q "PDO_FIXED_DUAL_ROLE" <<< "$fixed_flags" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if Attempts Discovery SOP
#
# Arguments:
#  none
# Returns:
#   0 if Attempts Discovery SOP
#   else 1
########################################
is_attempts_discov_sop() {
  local vs
  vs=($(grep "CONFIG_USB_PD_SIMPLE_DFP=y" $config))
  if [ ${#vs[@]} -eq 0 ]; then
    return 1
  fi

  return 0
}

#######################################
# Check if Supports VCONN Powered Accessory
#
# Arguments:
#  none
# Returns:
#   YES if Supports VCONN Powered Accessory
#   else NO
########################################
supports_vconn_powered_accessory() {
  local vpa
  vpa=($(grep "CONFIG_USB_PD_TCPM_FUSB302=y" $config))
  if [ ${#vpa[@]} -eq 0 ]; then
    echo "YES"
  else
    echo "NO"
  fi
}

#######################################
# Check if Can Act as Host
#
# Arguments:
#  none
# Returns:
#   YES if Can Act as Host
#   else NO
########################################
can_act_as_host() {
  local caa
  caa=($(grep "CONFIG_VIF_TYPE_C_CAN_ACT_AS_HOST=y" $config))
  if [ ${#caa[@]} -eq 0 ]; then
    echo "NO"
  else
    echo "YES"
  fi
}

#######################################
# Check if Can Act as Device
#
# Arguments:
#  none
# Returns:
#   YES if Can Act as Device
#   else NO
########################################
can_act_as_device() {
  local caa
  caa=($(grep "CONFIG_USB=y" $config))
  if [ ${#caa[@]} -eq 0 ]; then
    echo "NO"
  else
    echo "YES"
  fi
}

#######################################
# Check if DUT can dr swap to dfp
#
# Arguments:
#  none
# Returns:
#   YES if DUT can dr swap to dfp
#   else NO
########################################
get_dr_swap_to_dfp() {
  local tmp
  IFS=$''
  tmp=($(sed -n '/pd_check_data_swap(int\ port,\ int\ data_role)/,/}/p' \
    $usb_pd_policy))
  if grep -q "PD_ROLE_UFP" <<< "$tmp" ; then
    echo "YES"
    return
  fi

  if grep -q "return 1" <<< "$tmp" ; then
    echo "YES"
    return
  fi

  echo "NO"
}

#######################################
# Check if DUT can dr swap to ufp
#
# Arguments:
#  none
# Returns:
#   YES if DUT can dr swap to ufp
#   else NO
########################################
get_dr_swap_to_ufp() {
  local tmp
  IFS=$''
  tmp=($(sed -n '/pd_check_data_swap(int\ port,\ int\ data_role)/,/}/p' \
    $usb_pd_policy))
  if grep -q "PD_ROLE_DFP" <<< "$tmp" ; then
    echo "YES"
    return
  fi

  if grep -q "return 1" <<< "$tmp" ; then
    echo "YES"
    return
  fi

  echo "NO"
}

#######################################
# Check if UUT is Battery Powered
#
# Arguments:
#  none
# Returns:
#   YES if UUT is Battery Powered
#   else NO
########################################
is_battery_powered() {
  local bat
  bat=($(grep "CONFIG_BATTERY_BQ20Z453=y" $config))
  if [ ! ${#bat[@]} -eq 0 ]; then
    echo "YES"
    return
  fi

  bat=($(grep "CONFIG_BATTERY_BQ27541=y" $config))
  if [ ! ${#bat[@]} -eq 0 ]; then
    echo "YES"
    return
  fi

  bat=($(grep "CONFIG_BATTERY_BQ27621=y" $config))
  if [ ! ${#bat[@]} -eq 0 ]; then
    echo "YES"
    return
  fi

  bat=($(grep "CONFIG_BATTERY_RYU=y" $config))
  if [ ! ${#bat[@]} -eq 0 ]; then
    echo "YES"
    return
  fi

  bat=($(grep "CONFIG_BATTERY_SAMUS=y" $config))
  if [ ! ${#bat[@]} -eq 0 ]; then
    echo "YES"
    return
  fi

  bat=($(grep "CONFIG_BATTERY_SMART=y" $config))
  if [ ! ${#bat[@]} -eq 0 ]; then
    echo "YES"
    return
  fi

  echo "NO"
}

#######################################
# Check if UUT is Captive Cable
#
# Arguments:
#  none
# Returns:
#   YES if UUT is Captive Cable
#   else NO
########################################
is_captive_cable() {
  local caa
  caa=($(grep "CONFIG_VIF_CAPTIVE_CABLE=y" $config))
  if [ ${#caa[@]} -eq 0 ]; then
    echo "NO"
  else
    echo "YES"
  fi
}

#######################################
# Check if UUT can Source Vconn
#
# Arguments:
#  none
# Returns:
#   YES if UUT can Source Vconn
#   else NO
########################################
get_sources_vconn() {
  local caa
  caa=($(grep "CONFIG_USBC_VCONN=y" $config))
  if [ ${#caa[@]} -eq 0 ]; then
    echo "NO"
  else
    echo "YES"
  fi
}

#######################################
# Get RP value
#
# Arguments:
#  none
# Returns:
#   0 for DEFAULT USB
#   1 for 1A5
#   2 for 3A0
########################################
get_rp_value() {
  local rp
  IFS=$' '
  rp=($(grep "CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT" $board_h))
  if [ ${#rp[@]} -eq 0 ]; then
    echo "0" # Default
  else
    if [ "${rp[2]}" == "TYPEC_RP_3A0" ]; then
      echo "2"
    else
      if [ "${rp[2]}" == "TYPEC_RP_1A5" ]; then
        echo "1"
      else
        echo "0"
      fi
    fi
  fi
}

#######################################
# Get the operating power in source mode
#
# Arguments:
#  none
# Returns:
#   power in mw
########################################
get_pwr_as_source() {
  local pwr
  pwr=($(grep "PD_OPERATING_POWER_MW" $board_h))
  if [ ${#pwr[@]} -eq 0 ]; then
    # default to 500 mW
    echo "500"
  else
    echo "${pwr[2]}"
  fi
}

#######################################
# Get Google VID
#
# Arguments:
#  none
# Returns:
#   google vid
########################################
get_usb_vid_google() {
  local vid
  IFS=$' '
  vid=($(grep -m 1 "USB_VID_GOOGLE" $usb_descriptor_h))
  echo "${vid[2]}"
}

get_usb_pid() {
  local pid
  IFS=$' '
  pid=($(grep -m 1 "CONFIG_USB_PID" $board_h))
  if [ ${#pid[@]} -eq 0 ]; then
    echo "0x0000"
  else
    echo "${pid[2]}"
  fi
}

#######################################
# Get DisplayPort SID
#
# Arguments:
#  none
# Returns:
#   displayport sid
########################################
get_usb_vid_displayport() {
  local vid
  IFS=$' '
  vid=($(grep "USB_SID_DISPLAYPORT" $usb_pd_h))
  echo "${vid[2]}"
}

#######################################
# Check if any Source PDOs are defined
#
# Arguments:
#  none
# Returns:
#  0 if Source PDOs are defined
#  else 1
########################################
src_pdos_defined() {
  local pdos
  IFS=$''
  pdos=($(sed -n '/const\ uint32_t\ pd_src_pdo\[\]\ =\ {/,/};/p' \
    $usb_pd_policy))
  if [ ${#pdos[@]} -eq 0 ]; then
    # No src pdos found
    return 1
  fi

  if grep -q "PDO_FIXED" <<< "$pdos" ; then
    return 0
  fi

  if grep -q "PDO_BATT" <<< "$pdos" ; then
    return 0
  fi

  if grep -q "PDO_VAR" <<< "$pdos" ; then
    return 0
  fi

  return 1
}

#######################################
# Check if any Sink PDOs are defined
#
# Arguments:
#  none
# Returns:
#  0 if Sink PDOs are defined
#  else 1
########################################
snk_pdos_defined() {
  local pdos
  IFS=$''
  pdos=($(sed -n '/const\ uint32_t\ pd_snk_pdo\[\]\ =\ {/,/};/p' \
    $usb_pd_policy))
  if [ ${#pdos[@]} -eq 0 ]; then
    # No src pdos found
    return 1
  fi

  if grep -q "PDO_FIXED" <<< "$pdos" ; then
    return 0
  fi

  if grep -q "PDO_BATT" <<< "$pdos" ; then
    return 0
  fi

  if grep -q "PDO_VAR" <<< "$pdos" ; then
    return 0
  fi

  return 1
}

#######################################
# Get all defined SRC PDOs
#
# Arguments:
#  none
# Returns:
#  An array of SRC PDOs
########################################
get_src_pdos() {
  local pdos
  IFS=$''
  pdos=($(sed -n '/const\ uint32_t\ pd_src_pdo\[\]\ =\ {/,/};/p' \
    $usb_pd_policy))
  pdos=$(echo $pdos|tr -d '\n')
  pdos=$(echo $pdos|tr -d '\t')
  pdos=$(echo $pdos|tr -d ' ')
  pdos=${pdos##*\{}
  pdos=${pdos%,*}

  pdos=${pdos//"),"/") "}

  IFS=$' '
  read -r -a pdos <<< "$pdos"
  src_pdos=(${pdos[@]})
}

#######################################
# Get all defined SNK PDOs
#
# Arguments:
#  none
# Returns:
#  An array of SNK PDOs
########################################
get_snk_pdos() {
  local pdos
  IFS=$''
  pdos=($(sed -n '/const\ uint32_t\ pd_snk_pdo\[\]\ =\ {/,/};/p' \
    $usb_pd_policy))
  pdos=$(echo $pdos|tr -d '\n')
  pdos=$(echo $pdos|tr -d '\t')
  pdos=$(echo $pdos|tr -d ' ')
  pdos=${pdos##*\{}
  pdos=${pdos%,*}

  pdos=${pdos//"),"/") "}

  IFS=$' '
  read -r -a pdos <<< "$pdos"
  snk_pdos=(${pdos[@]})
}

#######################################
# Get Product Type
#
# Arguments:
#  none
# Returns:
#  Product Type
########################################
get_product_type() {
  local ptype
  IFS=$''
  ptype=($(sed -n '/const\ uint32_t\ vdo_idh\ =\ VDO_IDH(/,/);/p' \
    $usb_pd_policy))
  if [ ${#ptype[@]} -eq 0 ]; then
    # No procudt type found
    echo "$IDH_PTYPE_UNDEF"
    return
  fi

  if grep -q "IDH_PTYPE_UNDEF" <<< "$ptype" ; then
    echo "$IDH_PTYPE_UNDEF"
    return
  fi

  if grep -q "IDH_PTYPE_HUB" <<< "$ptype" ; then
    echo "$IDH_PTYPE_HUB"
    return
  fi

  if grep -q "IDH_PTYPE_PERIPH" <<< "$ptype" ; then
    echo "$IDH_PTYPE_PERIPH"
    return
  fi

  if grep -q "IDH_PTYPE_AMA" <<< "$ptype" ; then
    echo "$IDH_PTYPE_AMA"
    return
  fi

  echo "$IDH_PTYPE_UNDEF"
}

#######################################
# Get Device Type
#
# Arguments:
#  none
# Returns:
#  Device Type
########################################
get_device_type() {
  if is_drp; then
    echo "$DRP"
    return
  fi

  if src_pdos_defined; then
    echo "$SRC"
    return
  fi

  if snk_pdos_defined; then
    echo "$SNK"
    return
  fi

  echo "$SRC"
}

#######################################
# Get VIF Filename
# Parse the PDO_FIXED_FLAGS and generate
# vif filename.
#
# Arguments:
#  none
# Returns:
#
########################################
get_vif_filename() {
  local role=""
  local pwr=""
  local fixed_flags
  local name=$board_name"_src_vif.txt"
  IFS=$''

  fixed_flags=($(sed -n '/#define PDO_FIXED_FLAGS/,/)/p' $usb_pd_policy))
  if [ ${#fixed_flags[@]} -eq 0 ]; then
    # PDO_FIXED_FLAGS not found. Default to src
    echo "$name"
    return
  fi

  if is_extpwr; then
    pwr="_extpwr"
  fi

  if is_drp; then
    name=$board_name"_drp"$pwr"_vif.txt"
    echo "$name"
    return
  fi

  if src_pdos_defined; then
    name=$board_name"_src"$pwr"_vif.txt"
    echo "$name"
    return
  fi

  if snk_pdos_defined; then
    name=$board_name"_snk"$pwr"_vif.txt"
    echo "$name"
    return
  fi

  echo "$name"
}

#######################################
# Write PDO to VIF
#
# Arguments:
#  $1 - path ot VIF
#  $2 - pdo to write
#  $3 - 1 if sink or 0 if source
#  $4 - pdo number
# Returns:
#   max power of pdo
########################################
write_pdo_to_vif() {
  local vif=$1
  local pdo=$2
  local is_snk=$3
  local n=$4
  local tmp
  local type
  local num
  local max_power=0
  local role=("Src" "Snk")

  # remove any assignments before PDO
  # ie. [PDO_IDX_5V] = PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS)
  pdo=${pdo#*=}

  IFS=$'('
  tmp=( $pdo )
  type=${tmp[0]}
  values=${tmp[1]}
  values=${values::-1}

  IFS=$','
  vi=( $values )

  if [[ "$type" == "PDO_FIXED" ]]; then
    echo "${role[is_snk]}_PDO_Supply_Type$n: 0" >> $vif
    if [[ "$is_snk" -eq "0" ]]; then
      echo "${role[is_snk]}_PDO_Peak_Current$n: 0" >> $vif
    fi
    echo "${role[is_snk]}_PDO_Voltage$n: $((${vi[0]} / 50))" >> $vif
    if [[ ! "$is_snk" -eq "0" ]]; then
      echo "${role[is_snk]}_PDO_Op_Current$n: $((${vi[1]} / 10))" >> $vif
    else
      echo "${role[is_snk]}_PDO_Max_Current$n: $((${vi[1]} / 10))" >> $vif
    fi
    max_power=$((${vi[0]} * ${vi[1]} / 1000))
  else
    if [[ "$type" == "PDO_BATT" ]]; then
      echo "${role[is_snk]}_PDO_Supply_Type$n: 1" >> $vif
      echo "${role[is_snk]}_PDO_Min_Voltage$n: $((${vi[0]} / 50))" >> $vif
      echo "${role[is_snk]}_PDO_Max_Voltage$n: $((${vi[1]} / 50))" >> $vif
      if [[ ! "$is_snk" -eq "0" ]]; then
        echo "${role[is_snk]}_PDO_Op_Power$n: $((${vi[2]} / 250))" >> $vif
      else
        echo "${role[is_snk]}_PDO_Max_Power$n: $((${vi[2]} / 250))" >> $vif
      fi
      max_power=${vi[2]}
    else
      if [[ "$type" == "PDO_VAR" ]]; then
	echo "${role[is_snk]}_PDO_Supply_Type$n: 2" >> $vif
	if [[ "$is_snk" -eq "0" ]]; then
	  echo "${role[is_snk]}_PDO_Peak_Current$n: 0" >> $vif
	fi
	echo "${role[is_snk]}_PDO_Min_Voltage$n: $((${vi[0]} / 50))" >> $vif
	echo "${role[is_snk]}_PDO_Max_Voltage$n: $((${vi[1]} / 50))" >> $vif
	if [[ ! "$is_snk" -eq "0" ]]; then
	  echo "${role[is_snk]}_PDO_Op_Current$n: $((${vi[2]} / 10))" >> $vif
	else
          echo "${role[is_snk]}_PDO_Max_Current$n: $((${vi[2]} / 10))" >> $vif
	fi
	max_power=$((${vi[1]} * ${vi[2]} / 1000))
      fi
    fi
  fi
  echo "$max_power"
}

#######################################
# Generate the VIF
#
# Arguments:
#  $1 - path to VIF
# Returns:
#   none
########################################
gen_vif() {
  # create the vif
  vif=$1

  # write header
  echo "\$VIF_Specification: \"$vif_specification\"" > $vif
  echo "\$VIF_Producer: \"$vif_producer\"" >> $vif
  echo "\$Vendor_Name: \"$vendor_name\"" >> $vif
  echo "\$Product_Name: \"$board_name\"" >> $vif

  #set PD_Specification_Revision
  echo "PD_Specification_Revision: $pd_spec_rev" >> $vif

  # set UUT_Device_Type
  echo "UUT_Device_Type: $(get_device_type)" >> $vif

  # set USB_Comms_Capable
  echo "USB_Comms_Capable: YES" >> $vif

  # set DR_Swap_To_DFP_Supported
  echo "DR_Swap_To_DFP_Supported: $(get_dr_swap_to_dfp)" >> $vif

  # set DR_Swap_To_UFP_Supported
  echo "DR_Swap_To_UFP_Supported: $(get_dr_swap_to_ufp)" >> $vif

  # set Externally_Powered
  if is_extpwr; then
    echo "Externally_Powered: YES" >> $vif
  else
    echo "Externally_Powered: NO" >> $vif
  fi

  # Set VCONN_Swap_To_On/Off_Supported
  if ! is_vconn_swap; then
    vconn_swap="NO"
  else
    vconn_swap="YES"
  fi

  echo "VCONN_Swap_To_On_Supported: $vconn_swap" >> $vif
  echo "VCONN_Swap_To_Off_Supported: $vconn_swap" >> $vif

  # set Responds_To_Discov_SOP
  echo "Responds_To_Discov_SOP: YES"  >> $vif

  # set Attempts_Discov_SOP
  echo "Attempts_Discov_SOP: NO" >> $vif

  # set SOP_Capable
  echo "SOP_Capable: YES" >> $vif

  # set SOP_P_Capable
  echo "SOP_P_Capable: NO" >> $vif

  # set SOP_PP_Capable
  echo "SOP_PP_Capable: NO" >> $vif

  # set SOP_P_Debug_Capable
  echo "SOP_P_Debug_Capable: NO" >> $vif

  # set SOP_PP_Debug_Capable
  echo "SOP_PP_Debug_Capable: NO" >> $vif

  #### Source Fields ####
  dtype=$(get_device_type)
  if [ "$dtype" == "$SRC" ] || [ "$dtype" == "$DRP" ]; then
    # set USB_Suspend_May_Be_Cleared
    echo "USB_Suspend_May_Be_Cleared: NO" >> $vif

    # set Sends_Pings
    echo "Sends_Pings: NO" >> $vif

    # set Num_Src_PDOs
    get_src_pdos
    local num_src_pdos=${#src_pdos[@]}

    echo "Num_Src_PDOs: $num_src_pdos" >> $vif

    #### Source PDOs ####
    local i
    local mp="0"
    local tmp;
    for ((i = 0; i != num_src_pdos; i++)); do
      tmp=$(write_pdo_to_vif $vif "${src_pdos[i]}" 0 $((i+1)))
      if [ "$tmp" -gt "$mp" ]; then
        mp=$tmp
      fi
    done

    # set PD_Power_as_Source
    echo "PD_Power_as_Source: $mp" >> $vif
  fi

  #### Sink Field ####
  if [ "$dtype" == "$SNK" ] || [ "$dtype" == "$DRP" ]; then
    # set No_USB_Suspend_May_Be_Set
    echo "No_USB_Suspend_May_Be_Set: NO" >> $vif

    # set GiveBack_May_Be_Set
    if ! can_giveback; then
      giveback="NO"
    else
      giveback="YES"
    fi

    # set GiveBack_May_Be_Set
    echo "GiveBack_May_Be_Set: $giveback" >> $vif

    # set Higher_Capability_Set
    echo "Higher_Capability_Set: NO" >> $vif

    get_snk_pdos
    local num_snk_pdos=${#snk_pdos[@]}
    echo "Num_Snk_PDOs: $num_snk_pdos" >> $vif

    #### Sink PDOs ####
    local i
    local mp="0"
    local tmp;
    for ((i = 0; i != num_snk_pdos; i++)); do
      tmp=$(write_pdo_to_vif $vif "${snk_pdos[i]}" 1 $((i+1)))
      if [ "$tmp" -gt "$mp" ]; then
        mp=$tmp
      fi
    done

    # set PD_Power_as_Sink
    echo "PD_Power_as_Sink: $mp" >> $vif
  fi

  #### Dual Role Fields ####
  if [ "$dtype" == "$DRP" ]; then
    echo "Accepts_PR_Swap_As_Src: YES" >> $vif
    echo "Accepts_PR_Swap_As_Snk: YES" >> $vif
    echo "Requests_PR_Swap_As_Src: YES" >> $vif
    echo "Requests_PR_Swap_As_Snk: YES" >> $vif
  fi

  #### SOP Discovery Fields ####

  # set Structured_VDM_Version_SOP
  echo "Structured_VDM_Version_SOP: 0" >> $vif

  # set XID_SOP
  echo "XID_SOP: 0" >> $vif

  # set Data_Capable_as_USB_Host_SOP
  echo "Data_Capable_as_USB_Host_SOP: YES" >> $vif

  # set Data_Capable_as_USB_Device_SOP
  echo "Data_Capable_as_USB_Device_SOP: NO" >> $vif

  # set Product_Type_SOP
  echo "Product_Type_SOP: $(get_product_type)" >> $vif

  # set Modal_Operation_Supported_SOP
  echo "Modal_Operation_Supported_SOP: YES" >> $vif

  # set USB_VID_SOP
  echo "USB_VID_SOP: $(get_usb_vid_google)" >> $vif

  # set PID_SOP
  echo "PID_SOP: $(get_usb_pid)" >> $vif

  # set bcdDevice_SOP
  echo "bcdDevice_SOP: 0x0000" >> $vif

  local num_svids=0
  local vid
  vid=$(get_usb_vid_google)
  if [ -n "$vid" ]; then
    ((num_svids++))
    echo "SVID1_SOP: $vid" >> $vif
    echo "SVID1_num_modes_min_SOP: 1" >> $vif
    echo "SVID1_num_modes_max_SOP: 1" >> $vif
    echo "SVID1_num_modes_fixed_SOP: YES" >> $vif
    echo "SVID1_mode1_enter_SOP: YES" >> $vif
  fi

  vid=$(get_usb_vid_displayport)
  if [ -n "$vid" ]; then
    ((num_svids++))
    echo "SVID2_SOP: $vid" >> $vif
    echo "SVID2_num_modes_min_SOP: 2" >> $vif
    echo "SVID2_num_modes_max_SOP: 2" >> $vif
    echo "SVID2_num_modes_fixed_SOP: YES" >> $vif
    echo "SVID2_mode1_enter_SOP: YES" >> $vif
    echo "SVID2_mode2_enter_SOP: YES" >> $vif
  fi

  echo "Num_SVIDs_min_SOP: $num_svids" >> $vif
  echo "Num_SVIDs_max_SOP: $num_svids" >> $vif
  echo "SVID_fixed_SOP: YES" >> $vif

  #### USB Type-C Fields ####

  # set Type_C_State_Machine
  local typec
  if [ "$dtype" == "$DRP" ]; then
    typec="2"
  else
    if [ "$dtype" == "$SNK" ]; then
      typec="1"
    else
      typec="0"
    fi
  fi
  echo "Type_C_State_Machine: $typec" >> $vif

  # set Type_C_Implements_Try_SRC
  local try_src
  if ! can_try_src; then
    try_src="NO"
  else
    try_src="YES"
  fi
  echo "Type_C_Implements_Try_SRC: $try_src" >> $vif

  # set Try_C_Implements_Try_SNK
  echo "Type_C_Implements_Try_SNK: NO" >> $vif

  # set Rp_Value
  echo "Rp_Value: $(get_rp_value)" >> $vif

  # set Type_C_Supports_VCONN_Powered_Accessory
  echo "Type_C_Supports_VCONN_Powered_Accessory: \
    $(supports_vconn_powered_accessory)" >> $vif

  # set Type_C_Is_VCONN_Powered_Accessory
  echo "Type_C_Is_VCONN_Powered_Accessory: NO" >> $vif

  # set Type_C_Can_Act_As_Host
  echo "Type_C_Can_Act_As_Host: $(can_act_as_host)" >> $vif

  # set Type_C_Host_Speed
  echo "Type_C_Host_Speed: 4" >> $vif

  # set Type_C_Can_Act_As_Device
  echo "Type_C_Can_Act_As_Device: $(can_act_as_device)" >> $vif

  # set Type_C_Device_Speed
  echo "Type_C_Device_Speed: 4" >> $vif

  # set Type_C_Power_Source
  echo "Type_C_Power_Source: 2" >> $vif

  # set Type_C_BC_1_2_Support
  echo "Type_C_BC_1_2_Support: 1" >> $vif

  # set Type_C_Battery_Powered
  echo "Type_C_Battery_Powered: $(is_battery_powered)" >> $vif

  # set Type_C_Port_On_Hub
  echo "Type_C_Port_On_Hub: NO" >> $vif

  # set Type_C_Supports_Audio_Accessory
  echo "Type_C_Supports_Audio_Accessory: NO" >> $vif

  # set Captive_Cable
  echo "Captive_Cable: $(is_captive_cable)" >> $vif

  #set Type_C_Source_Vconn
  echo "Type_C_Source_Vconn: $(get_sources_vconn)" >> $vif
}

main() {
  if ! check_for_pd; then
    exit 0
  fi

  vif=$vif_dir"/"$(get_vif_filename)
  gen_vif $vif

  exit 0
}

main "$@"
