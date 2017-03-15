#!/bin/bash
#
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

board_h=$1
vif_dir=$2
board_name=$3

SNK=0
SRC=3
DRP=4
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
device_type=""
power_as_source=""

#######################################
# Check if VIFs should be created.
#
# Arguments:
#  none
# Returns:
#   0 if board.h file exists, vif dir exist and
#        CONFIG_USB_POWER_DELIVERY is defined
#   else 1
########################################
check_for_pd() {
  # Make sure we have a valid board.h file
  if [ ! -e "$board_h" ]; then
    return 1
  fi

  # Make sure we have a valid vif dir
  if [ ! -d "$vif_dir" ]; then
    return 1
  fi

  # Make sure USB POWER DELIVERY is enabled
  usb_pd=($(grep "CONFIG_USB_POWER_DELIVERY" $board_h))
  if [ ${#usb_pd[@]} -eq 0 ]; then
    return 1
  fi

  # Check for #undef CONFIG_USB_POWER_DELIVERY
  if [[ "${usb_pd[0]}" == "#undef" ]]; then
    return 1
  fi

  return 0
}

#######################################
# Returns the number of USB Type-C Ports
#
# Arguments:
#  none
# Returns:
#   Number of ports
########################################
get_port_count() {
  local count
  local pc
  pc=($(grep "CONFIG_USB_PD_PORT_COUNT" $board_h))
  if [ ${#pc[@]} -eq 0 ]; then
    echo "0"
  fi

  echo "${pc[2]}"
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
  local drp
  drp=($(grep "CONFIG_USB_PD_DUAL_ROLE" $board_h))
  if [ ${#drp[@]} -eq 0 ]; then
    return 1
  fi

  return 0
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
  try=($(grep "CONFIG_USB_PD_TRY_SRC" $board_h))
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
  gb=($(grep "CONFIG_USB_PD_GIVE_BACK" $board_h))
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
  vs=($(grep "CONFIG_USBC_VCONN_SWAP" $board_h))
  if [ ${#vs[@]} -eq 0 ]; then
    return 1
  fi

  return 0
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
  fi

  echo "${pwr[2]}"
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
  vid=($(grep "USB_VID_GOOGLE" $usb_descriptor_h))
  echo "${vid[2]}"
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
# Write PDO to VIF
#
# Arguments:
#  $1 - path ot VIF
#  $2 - pdo to write
#  $3 - 1 if sink or 0 if source
# Returns:
#   max power of pdo
########################################
write_pdo_to_vif() {
  local vif=$1
  local pdo=$2
  local is_snk=$3
  local tmp
  local type
  local num
  local max_power
  local role=("Src" "Snk")

  IFS=$'('
  tmp=( $pdo )
  IFS=$' '
  type=( $tmp )

  num=${type[1]}
  n=${num:${#num}-1:1}

  IFS=$','
  vi=( ${tmp[1]} )

  if [[ "${type[2]}" == "PDO_FIXED" ]]; then
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
    if [[ "${type[2]}" == "PDO_BATT" ]]; then
      echo "${role[is_snk]}_PDO_Supply_Type$n: 1" >> $vif
      echo "${role[is_snk]}_PDO_Min_Voltage$n: $((${vi[0]} / 50))" >> $vif
      echo "${role[is_snk]}_PDO_Max_Voltage$n: $((${vi[1]} / 50))" >> $vif
      IFS=$')'
      p=( ${vi[2]} )
      if [[ ! "$is_snk" -eq "0" ]]; then
        echo "${role[is_snk]}_PDO_Op_Power$n: $((${p[0]} / 250))" >> $vif
      else
        echo "${role[is_snk]}_PDO_Max_Power$n: $((${p[0]} / 250))" >> $vif
      fi
      max_power=${p[0]}
    else
      if [[ "${type[2]}" == "PDO_VAR" ]]; then
	echo "${role[is_snk]}_PDO_Supply_Type$n: 2" >> $vif
	if [[ "$is_snk" -eq "0" ]]; then
	  echo "${role[is_snk]}_PDO_Peak_Current$n: 0" >> $vif
	fi
	echo "${role[is_snk]}_PDO_Min_Voltage$n: $((${vi[0]} / 50))" >> $vif
	echo "${role[is_snk]}_PDO_Max_Voltage$n: $((${vi[1]} / 50))" >> $vif
	IFS=$')'
        p=( ${vi[2]} )
	if [[ ! "$is_snk" -eq "0" ]]; then
	  echo "${role[is_snk]}_PDO_Op_Current$n: $((${p[0]} / 10))" >> $vif
	else
          echo "${role[is_snk]}_PDO_Max_Current$n: $((${p[0]} / 10))" >> $vif
	fi
	max_power=$((${vi[1]} * ${p[0]} / 1000))
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
#  $2 - device type (SNK, SRC or DRP)
#  $3 - 1 if externally powered, else 0
# Returns:
#   none
########################################
gen_vif() {
  # create the vif
  vif=$1
  dtype=$2
  extpwr=$3

  # write header
  echo "\$VIF_Specification: \"$vif_specification\"" > $vif
  echo "\$VIF_Producer: \"$vif_producer\"" >> $vif
  echo "\$Vendor_Name: \"$vendor_name\"" >> $vif
  echo "\$Product_Name: \"$board_name\"" >> $vif

  #set PD_Specification_Revision
  echo "PD_Specification_Revision: $pd_spec_rev" >> $vif

  # set UUT_Device_Type
  echo "UUT_Device_Type: $dtype" >> $vif

  # set USB_Comms_Capable
  echo "USB_Comms_Capable: YES" >> $vif

  # set DR_Swap_To_DFP_Supported
  echo "DR_Swap_To_DFP_Supported: YES" >> $vif

  # set DR_Swap_To_UFP_Supported
  echo "DR_Swap_To_UFP_Supported: YES" >> $vif

  # set Externally_Powered
  echo "Externally_Powered: $extpwr" >> $vif

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
  if [ "$dtype" == "$SRC" ] || [ "$dtype" == "$DRP" ]; then
    # set USB_Suspend_May_Be_Cleared
    echo "USB_Suspend_May_Be_Cleared: NO" >> $vif

    # set Sends_Pings
    echo "Sends_Pings: NO" >> $vif

    # set Num_Src_PDOs
    local src_pdos
    IFS=$'\n'
    src_pdos=($(grep "PD_SRC_MAX_PDO" $board_h))
    local num_src_pdos=${#src_pdos[@]}

    if [[ num_src_pdos -eq 0 ]]; then
      src_pdos=($(grep "PD_SRC_PDO" $board_h))
      num_src_pdos=${#src_pdos[@]}
    fi

    echo "Num_Src_PDOs: $num_src_pdos" >> $vif

    #### Source PDOs ####
    local i
    local mp="0"
    local tmp;
    for ((i = 0; i != num_src_pdos; i++)); do
      tmp=$(write_pdo_to_vif $vif "${src_pdos[i]}" 0)
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

    local snk_pdos
    IFS=$'\n'
    snk_pdos=($(grep "PD_SNK_PDO" $board_h))

    local num_snk_pdos=${#snk_pdos[@]}
    echo "Num_Snk_PDOs: $num_snk_pdos" >> $vif

    #### Sink PDOs ####
    local i
    local mp="0"
    local tmp;
    for ((i = 0; i != num_snk_pdos; i++)); do
      tmp=$(write_pdo_to_vif $vif "${snk_pdos[i]}" 1)
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
  echo "Product_Type_SOP: 0" >> $vif

  # set Modal_Operation_Supported_SOP
  echo "Modal_Operation_Supported_SOP: YES" >> $vif

  # set USB_VID_SOP
  echo "USB_VID_SOP: $(get_usb_vid_google)" >> $vif

  # set PID_SOP
  echo "PID_SOP: 0x0000" >> $vif

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
  echo "Type_C_Supports_VCONN_Powered_Accessory: NO" >> $vif

  # set Type_C_Is_VCONN_Powered_Accessory
  echo "Type_C_Is_VCONN_Powered_Accessory: NO" >> $vif

  # set Type_C_Can_Act_As_Host
  echo "Type_C_Can_Act_As_Host: YES" >> $vif

  # set Type_C_Host_Speed
  echo "Type_C_Host_Speed: 4" >> $vif

  # set Type_C_Can_Act_As_Device
  echo "Type_C_Can_Act_As_Device: NO" >> $vif

  # set Type_C_Device_Speed
  echo "Type_C_Device_Speed: 4" >> $vif

  # set Type_C_Power_Source
  echo "Type_C_Power_Source: 2" >> $vif

  # set Type_C_BC_1_2_Support
  echo "Type_C_BC_1_2_Support: 1" >> $vif

  # set Type_C_Battery_Powered
  echo "Type_C_Battery_Powered: YES" >> $vif

  # set Type_C_Port_On_Hub
  echo "Type_C_Port_On_Hub: NO" >> $vif

  # set Type_C_Supports_Audio_Accessory
  echo "Type_C_Supports_Audio_Accessory: NO" >> $vif

  # set Captive_Cable
  echo "Captive_Cable: NO" >> $vif

  #set Type_C_Source_Vconn
  echo "Type_C_Source_Vconn: YES" >> $vif
}

main() {
  if ! check_for_pd; then
    exit 0
  fi

  if ! is_drp; then
    vif=$vif_dir"/"$board_name"_source_extpwr_port.vif"
    gen_vif $vif $SRC "Y"
    vif=$vif_dir"/"$board_name"_source_port.vif"
    gen_vif $vif $SRC "N"
    vif=$vif_dir"/"$board_name"_sink_extpwr_port.vif"
    gen_vif $vif $SNK "Y"
    vif=$vif_dir"/"$board_name"_sink_port.vif"
    gen_vif $vif $SNK "N"
  else
    vif=$vif_dir"/"$board_name"_drp_extpwr_port.vif"
    gen_vif $vif $DRP "Y"
    vif=$vif_dir"/"$board_name"_drp_port.vif"
    gen_vif $vif $DRP "N"
  fi

  exit 0
}

main "$@"
